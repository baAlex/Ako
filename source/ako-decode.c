/*-----------------------------

MIT License

Copyright (c) 2021 Alexander Brandt

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

-------------------------------

 [ako-decode.c]
 - Alexander Brandt 2021
-----------------------------*/

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ako-tables.h"
#include "ako.h"


extern void DevBenchmarkStart(const char* name);
extern void DevBenchmarkStop();
extern void DevBenchmarkTotal();
extern void DevSaveGrayPgm(size_t dimension, const uint8_t* data, const char* prefix);


#if (AKO_COMPRESSION == 1)
#include "lz4.h"
#include "lz4hc.h"

static inline void sDecompressLZ4(const void* input, void* output, size_t input_size, size_t output_size)
{
	int decompressed_size = 0;
	decompressed_size = LZ4_decompress_safe(input, output, (int)input_size, (int)output_size);
	assert((size_t)decompressed_size == output_size);
}
#endif


static inline size_t sDecodeDimension(uint32_t no)
{
	assert(no < VALID_DIMENSIONS_NO);
	return s_valid_dimensions[no];
}


static inline void sReadHead(const void* blob, size_t* dimension, size_t* channels, size_t* cdata_size)
{
	const struct AkoHead* head = blob;

	assert(memcmp(head->magic, "AkoIm", 5) == 0);

	assert(head->major == AKO_VER_MAJOR);
	assert(head->minor == AKO_VER_MINOR);
	assert(head->format == AKO_FORMAT);

	*dimension = (size_t)sDecodeDimension((head->info << (32 - 4)) >> (32 - 4));
	*channels = (size_t)(head->info >> 4);
	*cdata_size = (size_t)head->compressed_size;
}


static void sDeTransformColorFormat(size_t dimension, size_t channels, const uint8_t* input, uint8_t* out)
{
	// Interlace from planes
	DevBenchmarkStart("DeTransform - Format");
	switch (channels)
	{
	case 4:
		for (size_t i = 0; i < (dimension * dimension); i++)
			out[i * channels + 3] = input[(dimension * dimension * 3) + i];
	case 3:
		for (size_t i = 0; i < (dimension * dimension); i++)
			out[i * channels + 2] = input[(dimension * dimension * 2) + i];
	case 2:
		for (size_t i = 0; i < (dimension * dimension); i++)
			out[i * channels + 1] = input[(dimension * dimension * 1) + i];
	case 1:
		for (size_t i = 0; i < (dimension * dimension); i++)
			out[i * channels] = input[i];
	}
	DevBenchmarkStop();

#if (AKO_COLOR_TRANSFORMATION == 1)
	// Color transformation, YCoCg to sRgb
	// https://en.wikipedia.org/wiki/YCoCg

	DevBenchmarkStart("DeTransform - Color");
	if (channels == 3 || channels == 4)
	{
		for (size_t i = 0; i < (dimension * dimension * channels); i += channels)
		{
			const int16_t y = (int16_t)out[i];
			const int16_t co = (int16_t)(out[i + 1] - 128);
			const int16_t cg = (int16_t)(out[i + 2] - 128);

			const int16_t r = y - cg + co;
			const int16_t g = y + cg;
			const int16_t b = y - cg - co;

			out[i] = (r > 0) ? (r < 255) ? (uint8_t)r : 255 : 0;
			out[i + 1] = (g > 0) ? (g < 255) ? (uint8_t)g : 255 : 0;
			out[i + 2] = (b > 0) ? (b < 255) ? (uint8_t)b : 255 : 0;
		}
	}
	DevBenchmarkStop();
#endif
}


#if (AKO_WAVELET_TRANSFORMATION == 1)
static inline void sUnLift1(uint8_t* inout) // Only for the unlift 1x1 px to 2x2 px
{
	// Apply differences
	const uint8_t a = inout[0] - inout[3] + inout[1];
	const uint8_t b = inout[0] - inout[3] - inout[1];
	const uint8_t c = inout[0] + inout[3] + inout[2];
	const uint8_t d = inout[0] + inout[3] - inout[2];

	// Output
	inout[0] = a;
	inout[1] = b;
	inout[2] = c;
	inout[3] = d;
}

static inline void sUnLift4(size_t dimension, size_t total_len, uint8_t* in, uint8_t* out)
{
	const size_t len = (dimension * dimension) / 4;
	const size_t hdimension = dimension / 2; // Useless, Clang is more intelligent than this

	for (size_t i = 0; i < len; i++)
	{
		// Apply differences
		const uint8_t a = in[i] - in[len * 3 + i] + in[len * 1 + i];
		const uint8_t b = in[i] - in[len * 3 + i] - in[len * 1 + i];
		const uint8_t c = in[i] + in[len * 3 + i] + in[len * 2 + i];
		const uint8_t d = in[i] + in[len * 3 + i] - in[len * 2 + i];

		// Output
		const size_t col = (i % hdimension) * 2;
		const size_t row = ((i / hdimension) * 2) * dimension + col;

		out[row] = a;
		out[row + 1] = b;
		out[row + dimension] = c;
		out[row + dimension + 1] = d;
	}

	if (total_len != (dimension * dimension))
		memcpy(in, out, (dimension * dimension)); // Output is the next unlift input
}

static void sUnLiftPlane(size_t dimension, uint8_t* buffer_a, uint8_t* buffer_b)
{
	sUnLift1(buffer_a);

	size_t current_dimension = 4;
	while (current_dimension <= dimension)
	{
		sUnLift4(current_dimension, (dimension * dimension), buffer_a, buffer_b);
		current_dimension = current_dimension << 1;
	}
}
#endif


uint8_t* AkoDecode(size_t input_size, const uint8_t* input, size_t* out_dimension, size_t* out_channels)
{
	size_t dimension = 0;
	size_t channels = 0;
	size_t data_size = 0;
	size_t cdata_size = 0;

	uint8_t* data_a = NULL;
	uint8_t* data_b = NULL;

	assert(input_size >= sizeof(struct AkoHead));
	sReadHead(input, &dimension, &channels, &cdata_size);

	data_size = dimension * dimension * channels;
	data_a = calloc(1, data_size);
	data_b = calloc(1, data_size);
	assert(data_a != NULL);
	assert(data_b != NULL);

	// Decompress
	{
		const uint8_t* input_data = input + sizeof(struct AkoHead);

#if (AKO_COMPRESSION == 1)
		DevBenchmarkStart("DecompressLZ4");
		sDecompressLZ4(input_data, data_a, cdata_size, data_size);
		DevBenchmarkStop();
#else
		for (size_t i = 0; i < cdata_size; i++)
			data_a[i] = input_data[i];
#endif
	}

	// UnLift
#if (AKO_WAVELET_TRANSFORMATION != 0)
	DevBenchmarkStart("UnLift");
	switch (channels)
	{
	case 4:
		sUnLiftPlane(dimension, (uint8_t*)(data_a + (dimension * dimension * 3)),
		             (uint8_t*)(data_b + (dimension * dimension * 3)));
	case 3:
		sUnLiftPlane(dimension, (uint8_t*)(data_a + (dimension * dimension * 2)),
		             (uint8_t*)(data_b + (dimension * dimension * 2)));
	case 2:
		sUnLiftPlane(dimension, (uint8_t*)(data_a + (dimension * dimension * 1)),
		             (uint8_t*)(data_b + (dimension * dimension * 1)));
	case 1: sUnLiftPlane(dimension, (uint8_t*)(data_a), (uint8_t*)(data_b)); break;
	}
	DevBenchmarkStop();
#else
	for (size_t i = 0; i < (dimension * dimension * channels); i++)
		data_b[i] = data_a[i];
#endif

	// DeTransform color-format
	sDeTransformColorFormat(dimension, channels, data_b, data_a);

	// Bye!
	DevBenchmarkTotal();

	if (out_dimension != NULL)
		*out_dimension = dimension;
	if (out_channels != NULL)
		*out_channels = channels;

	free(data_b);
	return data_a;
}
