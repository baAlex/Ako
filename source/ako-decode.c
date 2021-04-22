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

static inline void sDecompressLZ4(size_t input_size, size_t output_size, const void* input, void* output)
{
	int decompressed_data_size = 0;
	decompressed_data_size = LZ4_decompress_safe(input, output, (int)input_size, (int)output_size);
	assert((size_t)decompressed_data_size == output_size);
}
#endif


static inline size_t sDecodeDimension(uint32_t no)
{
	assert(no < VALID_DIMENSIONS_NO);
	return s_valid_dimensions[no];
}


static inline void sReadHead(const void* blob, size_t* dimension, size_t* channels, size_t* compressed_data_size)
{
	const struct AkoHead* head = blob;

	assert(memcmp(head->magic, "AkoIm", 5) == 0);

	assert(head->major == AKO_VER_MAJOR);
	assert(head->minor == AKO_VER_MINOR);
	assert(head->format == AKO_FORMAT);

	*dimension = (size_t)sDecodeDimension((head->info << (32 - 4)) >> (32 - 4));
	*channels = (size_t)(head->info >> 4);
	*compressed_data_size = (size_t)head->compressed_data_size;
}


static void sDeTransformColorFormat(size_t dimension, size_t channels, const uint16_t* input, uint8_t* out)
{
	uint16_t* out16 = (uint16_t*)out; // HACK!

	// Interlace from planes
	DevBenchmarkStart("DeTransform - Format");
	switch (channels)
	{
	case 4:
		for (size_t i = 0; i < (dimension * dimension); i++)
			out16[i * channels + 3] = input[(dimension * dimension * 3) + i];
	case 3:
		for (size_t i = 0; i < (dimension * dimension); i++)
			out16[i * channels + 2] = input[(dimension * dimension * 2) + i];
	case 2:
		for (size_t i = 0; i < (dimension * dimension); i++)
			out16[i * channels + 1] = input[(dimension * dimension * 1) + i];
	case 1:
		for (size_t i = 0; i < (dimension * dimension); i++)
			out16[i * channels] = input[i];
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
			const int16_t y = (int16_t)out16[i];
			const int16_t co = (int16_t)(out16[i + 1] - 128);
			const int16_t cg = (int16_t)(out16[i + 2] - 128);

			const int16_t r = y - cg + co;
			const int16_t g = y + cg;
			const int16_t b = y - cg - co;

			out[i] = (r > 0) ? (r < 255) ? (uint8_t)r : 255 : 0;
			out[i + 1] = (g > 0) ? (g < 255) ? (uint8_t)g : 255 : 0;
			out[i + 2] = (b > 0) ? (b < 255) ? (uint8_t)b : 255 : 0;
		}
	}
	else
	{
		for (size_t i = 0; i < (dimension * dimension * channels); i++)
			out[i] = (uint8_t)out16[i];
	}
	DevBenchmarkStop();
#else
	for (size_t i = 0; i < (dimension * dimension * channels); i++)
		out[i] = (uint8_t)out16[i];
#endif
}


#if (AKO_WAVELET_TRANSFORMATION == 1)
static void sUnLiftPlane(size_t dimension, void* buffer_a, void* buffer_b)
{
	(void)dimension;
	(void)buffer_a;
	(void)buffer_b;
}
#endif


uint8_t* AkoDecode(size_t input_size, const void* input, size_t* out_dimension, size_t* out_channels)
{
	size_t dimension = 0;
	size_t channels = 0;
	size_t data_len = 0;
	size_t compressed_data_size = 0;

	uint16_t* buffer_a = NULL;
	uint16_t* buffer_b = NULL;

	assert(input_size >= sizeof(struct AkoHead));
	sReadHead(input, &dimension, &channels, &compressed_data_size);

	data_len = dimension * dimension * channels;
	buffer_a = calloc(1, sizeof(uint16_t) * data_len);
	buffer_b = calloc(1, sizeof(uint16_t) * data_len);
	assert(buffer_a != NULL);
	assert(buffer_b != NULL);

	// Decompress
	{
		const uint16_t* data = (uint16_t*)((uint8_t*)input + sizeof(struct AkoHead));

#if (AKO_COMPRESSION == 1)
		DevBenchmarkStart("DecompressLZ4");
		sDecompressLZ4(compressed_data_size, sizeof(uint16_t) * data_len, data, buffer_a);
		DevBenchmarkStop();
#else
		for (size_t i = 0; i < data_len; i++)
			buffer_a[i] = data[i];
#endif
	}

	// UnLift
#if (AKO_WAVELET_TRANSFORMATION != 0)
	DevBenchmarkStart("UnLift");
	switch (channels)
	{
	case 4: sUnLiftPlane(dimension, data_a + (dimension * dimension * 3), data_b + (dimension * dimension * 3));
	case 3: sUnLiftPlane(dimension, data_a + (dimension * dimension * 2), data_b + (dimension * dimension * 2));
	case 2: sUnLiftPlane(dimension, data_a + (dimension * dimension * 1), data_b + (dimension * dimension * 1));
	case 1: sUnLiftPlane(dimension, data_a, data_b); break;
	}
	DevBenchmarkStop();
#else
	for (size_t i = 0; i < (dimension * dimension * channels); i++)
		buffer_b[i] = buffer_a[i];
#endif

	// DeTransform color-format
	sDeTransformColorFormat(dimension, channels, buffer_b, (uint8_t*)buffer_a); // HACK!

	// Bye!
	DevBenchmarkTotal();

	if (out_dimension != NULL)
		*out_dimension = dimension;
	if (out_channels != NULL)
		*out_channels = channels;

	free(buffer_b);
	return (uint8_t*)buffer_a; // TODO, realloc buffer
}
