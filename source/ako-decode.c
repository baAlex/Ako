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


static void sDeTransformColorFormat(size_t dimension, size_t channels, int16_t* input, uint8_t* out)
{
	// Color transformation, YCoCg to sRgb
	// https://en.wikipedia.org/wiki/YCoCg

#if (AKO_DECODER_COLOR_TRANSFORMATION == 1)
	DevBenchmarkStart("DeTransform - Color");
	if (channels == 3)
	{
		for (size_t i = 0; i < (dimension * dimension); i++)
		{
			const int16_t y = input[(dimension * dimension * 0) + i];
			const int16_t co = input[(dimension * dimension * 1) + i] - 128;
			const int16_t cg = input[(dimension * dimension * 2) + i] - 128;

			const int16_t r = (y - cg + co);
			const int16_t g = (y + cg);
			const int16_t b = (y - cg - co);

			input[(dimension * dimension * 0) + i] = (r > 0) ? (r < 255) ? r : 255 : 0;
			input[(dimension * dimension * 1) + i] = (g > 0) ? (g < 255) ? g : 255 : 0;
			input[(dimension * dimension * 2) + i] = (b > 0) ? (b < 255) ? b : 255 : 0;
		}
	}
	else if (channels == 4)
	{
		for (size_t i = 0; i < (dimension * dimension); i++)
		{
			const int16_t y = input[(dimension * dimension * 0) + i];
			const int16_t co = input[(dimension * dimension * 1) + i] - 128;
			const int16_t cg = input[(dimension * dimension * 2) + i] - 128;
			const int16_t a = input[(dimension * dimension * 3) + i];

			const int16_t r = (y - cg + co);
			const int16_t g = (y + cg);
			const int16_t b = (y - cg - co);

			input[(dimension * dimension * 0) + i] = (r > 0) ? (r < 255) ? r : 255 : 0;
			input[(dimension * dimension * 1) + i] = (g > 0) ? (g < 255) ? g : 255 : 0;
			input[(dimension * dimension * 2) + i] = (b > 0) ? (b < 255) ? b : 255 : 0;
			input[(dimension * dimension * 3) + i] = (a > 0) ? (a < 255) ? a : 255 : 0;
		}
	}
	else
	{
		for (size_t i = 0; i < (dimension * dimension * channels); i++)
		{
			const int16_t c = input[i];
			input[i] = (c > 0) ? (c < 255) ? c : 255 : 0;
		}
	}
	DevBenchmarkStop();
#else
	for (size_t i = 0; i < (dimension * dimension * channels); i++)
	{
		const int16_t c = input[i];
		input[i] = (c > 0) ? (c < 255) ? c : 255 : 0;
	}
#endif

	// Interlace from planes
	DevBenchmarkStart("DeTransform - Format");
	switch (channels)
	{
	case 4:
		for (size_t i = 0; i < (dimension * dimension); i++)
			out[i * channels + 3] = (uint8_t)input[(dimension * dimension * 3) + i];
	case 3:
		for (size_t i = 0; i < (dimension * dimension); i++)
			out[i * channels + 2] = (uint8_t)input[(dimension * dimension * 2) + i];
	case 2:
		for (size_t i = 0; i < (dimension * dimension); i++)
			out[i * channels + 1] = (uint8_t)input[(dimension * dimension * 1) + i];
	case 1:
		for (size_t i = 0; i < (dimension * dimension); i++)
			out[i * channels] = (uint8_t)input[i];
	}
	DevBenchmarkStop();
}


#if (AKO_DECODER_WAVELET_TRANSFORMATION == 1)
static inline void sUnlift1d(size_t len, const int16_t* in, int16_t* out)
{
#if 1
	// Even
	out[0] = in[0] - (in[len] * 2) / 2;

	for (size_t i = 1; i < len; i++)
	{
		const int16_t hp = in[len + i] * 2;          // De-squish
		const int16_t hp_prev = in[len + i - 1] * 2; // Ditto
		out[i << 1] = in[i] - (hp + hp_prev) / 4;
	}

	// Odd
	for (size_t i = 0; i < (len - 2); i++)
	{
		const int16_t hp = in[len + i] * 2;
		out[(i << 1) + 1] = hp + (out[i << 1] + out[(i << 1) + 2]) / 2;
	}

	out[((len - 2) << 1) + 1] = (in[len + (len - 2)] * 2) + (out[(len - 2) << 1]);
	out[((len - 1) << 1) + 1] = (in[len + (len - 1)] * 2) + (out[(len - 1) << 1]);
#endif
}

static void sUnlift2d(size_t dimension, size_t final_dimension, int16_t* aux_buffer, int16_t* inout)
{
	// Columns
	for (size_t i = 0; i < dimension; i++)
	{
		int16_t* temp = inout + i;
		for (size_t u = 0; u < dimension; u++)
			aux_buffer[u] = temp[u * final_dimension];

		sUnlift1d(dimension / 2, aux_buffer, aux_buffer + dimension);

		temp = inout + i;
		for (size_t u = 0; u < dimension; u++)
			temp[u * final_dimension] = aux_buffer[dimension + u];
	}

	// Rows
	for (size_t i = 0; i < dimension; i++)
	{
		memcpy(aux_buffer, inout, sizeof(int16_t) * dimension);
		sUnlift1d(dimension / 2, aux_buffer, inout);
		inout = inout + final_dimension;
	}
}

static inline void sUnliftPlane(size_t dimension, void** aux_buffer, void* inout)
{
	for (size_t current = 4; current <= dimension; current = current * 2)
		sUnlift2d(current, dimension, *aux_buffer, inout);
}
#endif


uint8_t* AkoDecode(size_t input_size, const void* input, size_t* out_dimension, size_t* out_channels)
{
	size_t dimension = 0;
	size_t channels = 0;
	size_t data_len = 0;
	size_t compressed_data_size = 0;

	int16_t* buffer_a = NULL;
	uint8_t* buffer_b = NULL;

	assert(input_size >= sizeof(struct AkoHead));
	sReadHead(input, &dimension, &channels, &compressed_data_size);

	data_len = dimension * dimension * channels;
	buffer_a = calloc(1, sizeof(int16_t) * data_len);
	buffer_b = calloc(1, sizeof(uint8_t) * data_len);
	assert(buffer_a != NULL);
	assert(buffer_b != NULL);

	// Decompress
	{
		const int16_t* data = (int16_t*)((uint8_t*)input + sizeof(struct AkoHead));

#if (AKO_COMPRESSION == 1)
		DevBenchmarkStart("DecompressLZ4");
		sDecompressLZ4(compressed_data_size, sizeof(int16_t) * data_len, data, buffer_a);
		DevBenchmarkStop();
#else
		for (size_t i = 0; i < data_len; i++)
			buffer_a[i] = data[i];
#endif
	}

	// Unlift
#if (AKO_DECODER_WAVELET_TRANSFORMATION != 0)
	DevBenchmarkStart("Unlift");
	switch (channels)
	{
	case 4: sUnliftPlane(dimension, (void**)&buffer_b, buffer_a + (dimension * dimension * 3));
	case 3: sUnliftPlane(dimension, (void**)&buffer_b, buffer_a + (dimension * dimension * 2));
	case 2: sUnliftPlane(dimension, (void**)&buffer_b, buffer_a + (dimension * dimension * 1));
	case 1: sUnliftPlane(dimension, (void**)&buffer_b, buffer_a); break;
	}
	DevBenchmarkStop();
#endif

	// DeTransform color-format
	sDeTransformColorFormat(dimension, channels, buffer_a, buffer_b); // HACK!

	// Bye!
	DevBenchmarkTotal();

	if (out_dimension != NULL)
		*out_dimension = dimension;
	if (out_channels != NULL)
		*out_channels = channels;

	free(buffer_a);
	return (uint8_t*)buffer_b;
}
