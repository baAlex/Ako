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
extern void DevSaveGrayPgm(size_t dimension, const int16_t* data, const char* filename_format, ...);


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


static void sDeTransformColorFormat(size_t dimension, size_t channels, int16_t* input, uint8_t* output)
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
			output[i * channels + 3] = (uint8_t)input[(dimension * dimension * 3) + i];
	case 3:
		for (size_t i = 0; i < (dimension * dimension); i++)
			output[i * channels + 2] = (uint8_t)input[(dimension * dimension * 2) + i];
	case 2:
		for (size_t i = 0; i < (dimension * dimension); i++)
			output[i * channels + 1] = (uint8_t)input[(dimension * dimension * 1) + i];
	case 1:
		for (size_t i = 0; i < (dimension * dimension); i++)
			output[i * channels] = (uint8_t)input[i];
	}
	DevBenchmarkStop();
}


#if (AKO_DECODER_WAVELET_TRANSFORMATION == 1)
__attribute__((always_inline)) static inline void sUnlift1d(size_t len, const int16_t* in, int16_t* out)
{
#if 0 // Half of the CPU usage comes from memory management in sUnlift2d()
	memset(out, 0, sizeof(int16_t) * (len << 1));
#else

	// Even
	out[0] = in[0] - (in[len] / 2);

	for (size_t i = 1; i < len; i++)
	{
		const int16_t lp = in[i];
		const int16_t hp = in[len + i];
		const int16_t hp_prev = in[len + i - 1];

		out[i << 1] = lp - ((hp + hp_prev) / 4);
	}

	// Odd
	for (size_t i = 0; i < (len - 2); i++)
	{
		const int16_t hp = in[len + i];
		const int16_t even = out[i << 1];
		const int16_t even_next = out[(i << 1) + 2];

		out[(i << 1) + 1] = hp + ((even + even_next) / 2);
	}

	out[((len - 2) << 1) + 1] = in[len + (len - 2)] + out[(len - 2) << 1];
	out[((len - 1) << 1) + 1] = in[len + (len - 1)] + out[(len - 1) << 1];
#endif
}

static void sUnlift2d(size_t dimension, size_t channels, int16_t* aux_buffer, const int16_t* input, int16_t* output)
{
	// I'm deviating from papers nomenclature, so:

	// | ---- | ---- |      | ---- | ---- |
	// | LL   | LH   |      | LP   | B    |
	// | ---- | ---- |  ->  | ---- | ---- |
	// | HL   | HH   |      | C    | D    |
	// | ---- | ---- |      | ---- | ---- |

	// - LP as lowpass
	// - B, C, D as 'highpasses' or HP

	// --------

	// The encoder saves in this order:
	// [LP0 (2x2 px)], [C1, B1, D1], [C2, B2, D2], [C3, B3, D3]... etc.

	// Such bizarre thing because I'm resolving columns first.
	// LP with C, then B with D. Finally rows.

	// Even better, columns in C, B and D are packed as rows. Left
	// to right, ready to be consumed in a linear way with memcpy().

	// And finally, columns in B and D are bundled (or interleaved):
	// [Column 0B, Column 0D], [Column 1B, Column 1D]... etc.

	// --------

#define ITS_1993 1 // Dude wake up it's 1993!, let's unroll loops!.

	// Being serious, not only I'm unrolling loops, but also removing
	// multiplications, doing 64 bits reads and interleaving/ordering
	// operations that work on areas close each other in RAM... this
	// help the cache usage?, right?.

	// Nothing more than what a modern compiler already does.
	// And incredible, this kind of 90s optimizations saves 7 ms
	// on my old Celeron. So yeahh...

	// --------

	// Copy initial lowpass (2x2 px)
	for (size_t ch = 0; ch < channels; ch++)
		memcpy(output + dimension * dimension * ch, input + 2 * 2 * ch, sizeof(int16_t) * 2 * 2);

	// Unlift steps, aka: apply highpasses
	const int16_t* hp = input + (2 * 2 * channels); // HP starts after the initial 2x2 px LP

	for (size_t current = 2; current < dimension; current = current * 2)
	{
		const size_t current_x2 = current * 2; // Doesn't help the compiler, is just for legibility
		int16_t* lp = output;

		for (size_t ch = 0; ch < channels; ch++)
		{
			// Columns
			for (size_t col = 0; col < current; col++)
			{
#if (ITS_1993 == 1)
				int16_t* temp = lp + col;
				for (size_t i = 0; i < current; i = i + 2) // LP is a column
				{
					aux_buffer[i + 0] = *(temp);
					aux_buffer[i + 1] = *(temp + current);
					temp = temp + current_x2;
				}
#else
				int16_t* temp = lp + col;
				for (size_t i = 0; i < current; i++)
					aux_buffer[i] = temp[i * current];
#endif

				memcpy(aux_buffer + current, (hp + current * col),
				       sizeof(int16_t) * current); // Encoder packed C as a row, no weird loops here

				// Unlift LP and C (first halve)
				// Input first half of the buffer, output in the last half
				sUnlift1d(current, aux_buffer, aux_buffer + current_x2);

				// Unlift B and D (second halve)
				// - '(hp + current * current)' is C, we skip it
				// - '(current_x2 * col)' is the counterpart of '(hp + current * col)' above in C
				//   except that B and D are bundled together by the encoder, thus the x2
				sUnlift1d(current, (hp + current * current) + (current_x2 * col), aux_buffer);

				// Write unlift results back to LP
#if (ITS_1993 == 1)
				temp = lp + col;
				for (size_t i = 0; i < current_x2; i = i + 4)
				{
					// Hopefully the compiler will notice that the intention of my
					// bitshifts is to translate those into 'MOV [MEM], REG16' of a REG64
					const uint64_t data = *(uint64_t*)((uint16_t*)aux_buffer + i + current_x2);
					const uint64_t data2 = *(uint64_t*)((uint16_t*)aux_buffer + i);

					*(temp) = (int16_t)((data >> 0) & 0xFFFF);
					*(temp + current) = (int16_t)((data2 >> 0) & 0xFFFF);

					*(temp + current_x2) = (int16_t)((data >> 16) & 0xFFFF);
					*(temp + current_x2 + current) = (int16_t)((data2 >> 16) & 0xFFFF);

					*(temp + current_x2 * 2) = (int16_t)((data >> 32) & 0xFFFF);
					*(temp + current_x2 * 2 + current) = (int16_t)((data2 >> 32) & 0xFFFF);

					*(temp + current_x2 * 3) = (int16_t)((data >> 48));
					*(temp + current_x2 * 3 + current) = (int16_t)((data2 >> 48));

					temp = temp + (current_x2 << 2);
				}
#else
				temp = lp + col;
				for (size_t i = 0; i < current_x2; i++)
					temp[i * current_x2] = aux_buffer[current_x2 + i];

				temp = lp + current + col;
				for (size_t i = 0; i < current_x2; i++)
					temp[i * current_x2] = aux_buffer[i];
#endif
			}

			// Rows, operates just in LP
			for (size_t row = 0; row < current_x2; row++)
			{
				memcpy(aux_buffer, lp + row * current_x2, sizeof(int16_t) * current_x2);
				sUnlift1d(current, aux_buffer, lp + row * current_x2);
			}

			// Done with this channel
			DevSaveGrayPgm(current_x2, lp, "d-ch%zu", ch);
			hp = hp + current * current * 3; // Move from current B, C and D
			lp = lp + dimension * dimension;
		}
	}
}
#endif


uint8_t* AkoDecode(size_t input_size, const void* input, size_t* out_dimension, size_t* out_channels)
{
	size_t dimension = 0;
	size_t channels = 0;
	size_t data_len = 0;
	size_t compressed_data_size = 0;

	int16_t* buffer_a = NULL;
	int16_t* buffer_b = NULL;
	int16_t* aux_buffer = NULL;

	assert(input_size >= sizeof(struct AkoHead));
	sReadHead(input, &dimension, &channels, &compressed_data_size);

	data_len = dimension * dimension * channels;
	buffer_a = calloc(1, sizeof(int16_t) * data_len);
	buffer_b = calloc(1, sizeof(int16_t) * data_len);
	aux_buffer = calloc(1, sizeof(int16_t) * dimension * 2);

	assert(buffer_a != NULL);
	assert(buffer_b != NULL);
	assert(aux_buffer != NULL);

	// Decompress
	{
		const int16_t* temp = (int16_t*)((uint8_t*)input + sizeof(struct AkoHead));

#if (AKO_COMPRESSION == 1)
		DevBenchmarkStart("DecompressLZ4");
		sDecompressLZ4(compressed_data_size, sizeof(int16_t) * data_len, temp, buffer_a);
		DevBenchmarkStop();
#else
		for (size_t i = 0; i < data_len; i++)
			buffer_a[i] = temp[i];
#endif
	}

	// Unlift
#if (AKO_DECODER_WAVELET_TRANSFORMATION != 0)
	DevBenchmarkStart("Unpack/Unlift");
	sUnlift2d(dimension, channels, aux_buffer, buffer_a, buffer_b);
	DevBenchmarkStop();
#else
	for (size_t i = 0; i < data_len; i++)
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

	free(aux_buffer);
	free(buffer_b);
	return (uint8_t*)buffer_a;
}
