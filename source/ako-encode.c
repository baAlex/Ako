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

 [ako-encode.c]
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


struct LiftSettings
{
	float uniform_gate;
	float detail_gate;
};


#if (AKO_COMPRESSION == 1)
#include "lz4.h"
#include "lz4hc.h"

static size_t sCompressLZ4(void* data, void** aux_buffer, size_t size)
{
	assert(size <= LZ4_MAX_INPUT_SIZE);
	const int worst_size = LZ4_compressBound((int)size);

	*aux_buffer = realloc(*aux_buffer, (size_t)worst_size);
	assert(*aux_buffer != NULL);

	const int cdata_size = LZ4_compress_HC(data, *aux_buffer, (int)size, worst_size, LZ4HC_CLEVEL_MAX);
	assert(cdata_size != 0);

	memcpy(data, *aux_buffer, (size_t)cdata_size);
	return (size_t)cdata_size;
}
#endif


static uint32_t sEncodeDimension(size_t no)
{
	for (uint32_t i = 0; i < VALID_DIMENSIONS_NO; i++)
	{
		if (s_valid_dimensions[i] == no)
			return i;
	}

	return UINT32_MAX;
}


static void sWriteHead(void* blob, size_t dimension, size_t channels, size_t cdata_size)
{
	struct AkoHead* head = blob;

	head->magic[0] = 'A';
	head->magic[1] = 'k';
	head->magic[2] = 'o';
	head->magic[3] = 'I';
	head->magic[4] = 'm';

	head->major = AKO_VER_MAJOR;
	head->minor = AKO_VER_MINOR;
	head->format = AKO_FORMAT;

	assert(cdata_size <= UINT32_MAX);

	head->compressed_size = (uint32_t)cdata_size;
	head->info = sEncodeDimension(dimension) | (uint32_t)(channels << 4);
}


static void sTransformColorFormat(size_t dimension, size_t channels, const uint8_t* input, uint8_t* out)
{
	// De-interlace into planes
	// If there is an alpha channel, the pixels are copied (or not) accordingly
	DevBenchmarkStart("Transform - Format");
	switch (channels)
	{
	case 4: // Alpha channel
		for (size_t i = 0; i < (dimension * dimension); i++)
			out[(dimension * dimension * 3) + i] = input[i * channels + 3];
	case 3:
		for (size_t i = 0; i < (dimension * dimension); i++)
		{
			if (channels < 4 || input[i * channels + 3] != 0)
				out[(dimension * dimension * 2) + i] = input[i * channels + 2];
			else
				out[(dimension * dimension * 2) + i] = 0;
		}
	case 2: // Alpha channel if 'channels == 2'
		for (size_t i = 0; i < (dimension * dimension); i++)
		{
			if (channels < 4 || input[i * channels + 3] != 0)
				out[(dimension * dimension * 1) + i] = input[i * channels + 1];
			else
				out[(dimension * dimension * 1) + i] = 0;
		}
	case 1:
		for (size_t i = 0; i < (dimension * dimension); i++)
		{
			if (channels == 1 || (channels == 2 && input[i * channels + 1] != 0))
				out[i] = input[i * channels];
			else if (channels < 4 || input[i * channels + 3] != 0)
				out[i] = input[i * channels];
			else
				out[i] = 0;
		}
		break;
	}
	DevBenchmarkStop();

#if (AKO_COLOR_TRANSFORMATION == 1)
	// Color transformation, sRgb to YCoCg
	// https://en.wikipedia.org/wiki/YCoCg

	DevBenchmarkStart("Transform - Color");
	if (channels == 3 || channels == 4)
	{
		for (size_t i = 0; i < (dimension * dimension); i++)
		{
			const uint8_t r = out[i];
			const uint8_t g = out[(dimension * dimension * 1) + i];
			const uint8_t b = out[(dimension * dimension * 2) + i];

			const uint8_t y = r / 4 + g / 2 + b / 4;
			const uint8_t co = 128 + r / 2 - b / 2;
			const uint8_t cg = 128 - (r / 4) + g / 2 - b / 4;

			out[i] = y;
			out[(dimension * dimension * 1) + i] = co;
			out[(dimension * dimension * 2) + i] = cg;
		}
	}
	DevBenchmarkStop();
#endif
}


#if (AKO_WAVELET_TRANSFORMATION == 1)
static void sEncoUnLift1(const uint8_t* in, uint8_t* out)
{
	// Apply differences
	const uint8_t a = in[0] - in[3] + in[1];
	const uint8_t b = in[0] - in[3] - in[1];
	const uint8_t c = in[0] + in[3] + in[2];
	const uint8_t d = in[0] + in[3] - in[2];

	// Output
	out[0] = a;
	out[1] = b;
	out[2] = c;
	out[3] = d;
}

static void sEncoUnLift4(size_t dimension, size_t total_len, const uint8_t* diffs, uint8_t* in, uint8_t* out)
{
	const size_t len = (dimension * dimension) / 4;
	const size_t hdimension = dimension / 2; // Useless, Clang is more intelligent than this

	for (size_t i = 0; i < len; i++)
	{
		// Apply differences
		const uint8_t a = in[i] - diffs[len * 3 + i] + diffs[len * 1 + i];
		const uint8_t b = in[i] - diffs[len * 3 + i] - diffs[len * 1 + i];
		const uint8_t c = in[i] + diffs[len * 3 + i] + diffs[len * 2 + i];
		const uint8_t d = in[i] + diffs[len * 3 + i] - diffs[len * 2 + i];

		// Output
		const size_t col = (i % hdimension) * 2;
		const size_t row = (i / hdimension) * 2;

		out[(row * dimension) + col] = a;
		out[(row * dimension) + col + 1] = b;
		out[((row + 1) * dimension) + col] = c;
		out[((row + 1) * dimension) + col + 1] = d;
	}

	if (total_len != (dimension * dimension))
		memcpy(in, out, (dimension * dimension)); // Output is the next unlift input
}


static void sLift(const struct LiftSettings* s, size_t dimension, size_t final_dimension, const uint8_t* lp,
                  const uint8_t* reference, uint8_t* out)
{
	const size_t len = (dimension * dimension) / 4;
	const size_t hdimension = dimension / 2;

	for (size_t i = 0; i < len; i++)
	{
		const size_t col = (i % hdimension) * 2;
		const size_t row = (i / hdimension) * 2;

		// Reference
		const int16_t ra = reference[(row * dimension) + col];
		const int16_t rb = reference[(row * dimension) + col + 1];
		const int16_t rc = reference[((row + 1) * dimension) + col];
		const int16_t rd = reference[((row + 1) * dimension) + col + 1];

		// Calculate differences, 2d Haar wavelet differences
		const int16_t diff_a = ra - lp[i];
		const int16_t diff_b = rb - lp[i];
		const int16_t mid_ab = (diff_a + diff_b) / 2;

		const int16_t diff_c = rc - lp[i];
		const int16_t diff_d = rd - lp[i];
		const int16_t mid_cd = (diff_c + diff_d) / 2;

		int16_t diff_1 = ((diff_a - mid_ab) + (mid_ab - diff_b)) / 2; // The average is to reduce round errors
		int16_t diff_2 = ((diff_c - mid_cd) + (mid_cd - diff_d)) / 2; // Ditto
		int16_t diff_3 = ((lp[i] - mid_ab) + (mid_cd - lp[i])) / 2;   // Ditto

		// Gates
		int16_t gate = (int16_t)truncf(s->uniform_gate);
		gate = (gate > 0) ? (gate < 255) ? gate : 255 : 0;

		if (diff_1 < gate && diff_1 > -gate)
			diff_1 = 0;
		if (diff_2 < gate && diff_2 > -gate)
			diff_2 = 0;
		if (diff_3 < gate && diff_3 > -gate)
			diff_3 = 0;

		gate = (int16_t)truncf(s->detail_gate * ((float)dimension / (float)final_dimension));
		gate = (gate > 0) ? (gate < 255) ? gate : 255 : 0;

		if (diff_1 < gate && diff_1 > -gate)
			diff_1 = 0;
		if (diff_2 < gate && diff_2 > -gate)
			diff_2 = 0;
		if (diff_3 < gate && diff_3 > -gate)
			diff_3 = 0;

		// Cath rounding/over/underflow errors
		// We apply the differences like the decoder do, then with
		// gently applied brute force we find the 'best' value for
		// such differences, one between ideal ranges
		// - TODO: think on something better
		// - TODO: there are cases where we want this to overflow!
		while (1)
		{
			int its_fine = 0;
			int16_t a = (uint16_t)lp[i] - diff_3 + diff_1;
			int16_t b = (uint16_t)lp[i] - diff_3 - diff_1;
			int16_t c = (uint16_t)lp[i] + diff_3 + diff_2;
			int16_t d = (uint16_t)lp[i] + diff_3 - diff_2;

			if (a < 0 || a > 255 || b < 0 || b > 255)
			{
				if (diff_1 != 0)
				{
					if (diff_1 < 0)
						diff_1++;
					else
						diff_1--;
				}
			}
			else
				its_fine++;

			if (c < 0 || c > 255 || d < 0 || d > 255)
			{
				if (diff_2 != 0)
				{
					if (diff_2 < 0)
						diff_2++;
					else
						diff_2--;
				}
			}
			else
				its_fine++;

			// If the last condition is meet, the pixel ends up like no
			// transformation was done on it (this process is even more lossy)
			if (its_fine == 2 || (diff_3 == 0 && diff_2 == 0 && diff_1 == 0))
				break;

			// This one is our last resource, and the most dangerous
			// since 'diff_3' affects the four pixels at once
			if (diff_3 != 0)
			{
				if (diff_3 < 0)
					diff_3++;
				else
					diff_3--;
			}
		}

		// Output
		out[len * 1 + i] = (uint8_t)diff_1;
		out[len * 2 + i] = (uint8_t)diff_2;
		out[len * 3 + i] = (uint8_t)diff_3;
	}
}


static void sLiftPlane(const struct LiftSettings* s, size_t dimension, void* aux_buffer1, void* aux_buffer2,
                       const uint8_t* mipmaps, void* out)
{
	// First lift
	{
		*(uint8_t*)out = mipmaps[0]; // Lowpass

		// Mipmap zero as lowpass (1x1 px)
		// Mipmap one as reference (2x2 px)
		sLift(s, 2, dimension, mipmaps + 0, mipmaps + 1, out);
		sEncoUnLift1(out, aux_buffer1);
	}

	// Further lifts
	size_t current_dimension = 4; // In the previous lift counts as 2
	size_t reference = 5;         // In the previous lift count as 1

	size_t lp = 1;

	while (current_dimension <= dimension)
	{
		// Here all lowpass are unlifted from the new created (lifted) differences,
		// similar to what the decoder do (only the first lift use a mipmap as lowpass)

		DevSaveGrayPgm(current_dimension / 2, aux_buffer1, "lp");

		sLift(s, current_dimension, dimension, aux_buffer1, mipmaps + reference, out);
		sEncoUnLift4(current_dimension, (dimension * dimension), out, aux_buffer1, aux_buffer2);

		lp = lp + (current_dimension * current_dimension) / 4;
		current_dimension = current_dimension * 2;
		reference = reference + (current_dimension * current_dimension) / 4;
	}

	DevSaveGrayPgm(dimension, out, "grad");
}
#endif


static void sMipize(size_t in_dimension, const uint8_t* in, uint8_t* out)
{
	// Box filter, this only works because is analog on how Haar
	// wavelets average four pixels into one
	const size_t len = (in_dimension * in_dimension) / 4;

	for (size_t i = 0; i < len; i++)
	{
		const size_t row = (i / (in_dimension / 2)) * 2;
		const size_t col = (i % (in_dimension / 2)) * 2;

		const uint8_t a = in[(row * in_dimension) + col];
		const uint8_t b = in[(row * in_dimension) + col + 1];
		const uint8_t c = in[((row + 1) * in_dimension) + col];
		const uint8_t d = in[((row + 1) * in_dimension) + col + 1];

		out[i] = (a + b + c + d) / 4;
	}
}


static void sMipizePlane(size_t dimension, const uint8_t* in, uint8_t* out)
{
	uint8_t* prev = NULL;
	size_t temp = dimension;

	// Move output pointer to the end
	while (temp > 1)
	{
		out = out + (temp * temp) / 4;
		temp = temp / 2;
	}

	// Copy original input as last mip
	memcpy(out, in, dimension * dimension);

	// Step back in output pointer and process mip, repeat
	// This way the 2x2 mip ends up at the beginning
	while (dimension > 1)
	{
		out = out - (dimension * dimension) / 4;
		sMipize(dimension, (prev != NULL) ? prev : in, out);

		prev = out;
		dimension = dimension / 2;
	}
}


size_t AkoEncode(size_t dimension, size_t channels, const struct AkoSettings* settings, const uint8_t* input,
                 uint8_t** output)
{
	assert(input != NULL);
	assert(channels <= 4);
	assert(sEncodeDimension(dimension) != UINT16_MAX);

	size_t data_len = dimension * dimension * channels;
	size_t compressed_size = 0;

	uint8_t* main_buffer = malloc(sizeof(struct AkoHead) + sizeof(uint8_t) * (dimension * dimension * channels));
	uint8_t* single_plane_a = malloc(sizeof(uint8_t) * (dimension * dimension));
	uint8_t* single_plane_b = malloc(sizeof(uint8_t) * (dimension * dimension));
	uint8_t* mipmaps =
	    malloc(sizeof(uint8_t) * ((dimension * dimension) +
	                              (dimension * dimension) / 2)); // Mipmaps use 33% of memory, but I'm assuming 50%

	assert(main_buffer != NULL);
	assert(single_plane_a != NULL);
	assert(single_plane_b != NULL);
	assert(mipmaps != NULL);

	uint8_t* data = main_buffer + sizeof(struct AkoHead);

	// Transform color-format
	sTransformColorFormat(dimension, channels, input, data);

	// Lift
#if (AKO_WAVELET_TRANSFORMATION != 0)
	struct LiftSettings s = {0};
	DevBenchmarkStart("Mipmaps/Lift");
	switch (channels)
	{
	case 4:
		s.uniform_gate = settings->uniform_gate[3];
		s.detail_gate = settings->detail_gate[3];

		sMipizePlane(dimension, data + (dimension * dimension * 3), mipmaps);
		sLiftPlane(&s, dimension, single_plane_a, single_plane_b, mipmaps, data + (dimension * dimension * 3));
	case 3:
		s.uniform_gate = settings->uniform_gate[2];
		s.detail_gate = settings->detail_gate[2];

		sMipizePlane(dimension, data + (dimension * dimension * 2), mipmaps);
		sLiftPlane(&s, dimension, single_plane_a, single_plane_b, mipmaps, data + (dimension * dimension * 2));
	case 2:
		s.uniform_gate = settings->uniform_gate[1];
		s.detail_gate = settings->detail_gate[1];

		sMipizePlane(dimension, data + (dimension * dimension * 1), mipmaps);
		sLiftPlane(&s, dimension, single_plane_a, single_plane_b, mipmaps, data + (dimension * dimension * 1));
	case 1:
		s.uniform_gate = settings->uniform_gate[0];
		s.detail_gate = settings->detail_gate[0];

		sMipizePlane(dimension, data, mipmaps);
		sLiftPlane(&s, dimension, single_plane_a, single_plane_b, mipmaps, data);
		break;
	}
	DevBenchmarkStop();
#endif

	// Compress
#if (AKO_COMPRESSION == 1)
	DevBenchmarkStart("CompressLZ4");
	compressed_size =
	    sCompressLZ4(data, (void**)&single_plane_a, sizeof(uint8_t) * data_len); // Operates in place (in 'data')
	DevBenchmarkStop();

	main_buffer = realloc(main_buffer, (sizeof(struct AkoHead) + compressed_size));
	assert(main_buffer != NULL);
#else
	compressed_size = sizeof(uint8_t) * data_len;
#endif

	// Bye!
	sWriteHead(main_buffer, dimension, channels, compressed_size);
	DevBenchmarkTotal();

	if (output != NULL)
		*output = main_buffer;
	else
		free(main_buffer);

	free(single_plane_a);
	free(single_plane_b);
	free(mipmaps);
	return (sizeof(struct AkoHead) + compressed_size);
}
