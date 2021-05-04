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
extern void DevSaveGrayPgm(size_t dimension, const int16_t* data, const char* filename_format, ...);


struct LiftSettings
{
	float uniform_gate;
	float detail_gate;
};


#if (AKO_COMPRESSION == 1)
#include "lz4.h"
#include "lz4hc.h"

static size_t sCompressLZ4(size_t size, void** aux_buffer, void* inout)
{
	assert(size <= LZ4_MAX_INPUT_SIZE);
	const int worst_size = LZ4_compressBound((int)size);

	*aux_buffer = realloc(*aux_buffer, (size_t)worst_size);
	assert(*aux_buffer != NULL);

	const int cdata_size = LZ4_compress_HC(inout, *aux_buffer, (int)size, worst_size, LZ4HC_CLEVEL_MAX);
	assert(cdata_size != 0);

	memcpy(inout, *aux_buffer, (size_t)cdata_size);
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


static void sWriteHead(void* blob, size_t dimension, size_t channels, size_t compressed_data_size)
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

	assert(compressed_data_size <= UINT32_MAX);

	head->compressed_data_size = (uint32_t)compressed_data_size;
	head->info = sEncodeDimension(dimension) | (uint32_t)(channels << 4);
}


static void sTransformColorFormat(size_t dimension, size_t channels, const uint8_t* input, int16_t* out)
{
	// De-interlace into planes
	// If there is an alpha channel, the pixels are copied (or not) accordingly
	DevBenchmarkStart("Transform - Format");
	switch (channels)
	{
	case 4: // Alpha channel
		for (size_t i = 0; i < (dimension * dimension); i++)
			out[(dimension * dimension * 3) + i] = (int16_t)input[i * channels + 3];
	case 3:
		for (size_t i = 0; i < (dimension * dimension); i++)
		{
			if (channels < 4 || input[i * channels + 3] != 0)
				out[(dimension * dimension * 2) + i] = (int16_t)input[i * channels + 2];
			else
				out[(dimension * dimension * 2) + i] = 0;
		}
	case 2: // Alpha channel if 'channels == 2'
		for (size_t i = 0; i < (dimension * dimension); i++)
		{
			if (channels < 4 || input[i * channels + 3] != 0)
				out[(dimension * dimension * 1) + i] = (int16_t)input[i * channels + 1];
			else
				out[(dimension * dimension * 1) + i] = 0;
		}
	case 1:
		for (size_t i = 0; i < (dimension * dimension); i++)
		{
			if (channels == 1 || (channels == 2 && input[i * channels + 1] != 0))
				out[i] = (int16_t)input[i * channels];
			else if (channels < 4 || input[i * channels + 3] != 0)
				out[i] = (int16_t)input[i * channels];
			else
				out[i] = 0;
		}
		break;
	}
	DevBenchmarkStop();

#if (AKO_ENCODER_COLOR_TRANSFORMATION == 1)
	// Color transformation, sRgb to YCoCg
	// https://en.wikipedia.org/wiki/YCoCg

	DevBenchmarkStart("Transform - Color");
	if (channels == 3 || channels == 4)
	{
		for (size_t i = 0; i < (dimension * dimension); i++)
		{
			const int16_t r = out[i];
			const int16_t g = out[(dimension * dimension * 1) + i];
			const int16_t b = out[(dimension * dimension * 2) + i];

			const int16_t y = r / 4 + g / 2 + b / 4;
			const int16_t co = 128 + r / 2 - b / 2;
			const int16_t cg = 128 - (r / 4) + g / 2 - b / 4;

			out[i] = y;
			out[(dimension * dimension * 1) + i] = co;
			out[(dimension * dimension * 2) + i] = cg;
		}
	}
	DevBenchmarkStop();
#endif
}


#define AKO_DEV_EMPIRICAL_OBSERVATION 0

#if (AKO_DEV_EMPIRICAL_OBSERVATION == 1)
#include <stdio.h>
static int16_t s_min_hp = INT16_MAX;
static int16_t s_max_hp = INT16_MIN;
#endif

#if (AKO_ENCODER_WAVELET_TRANSFORMATION == 1)
static void sLift1d(const struct LiftSettings* s, size_t len, size_t initial_len, const int16_t* in, int16_t* out)
{
	for (size_t i = 0; i < len; i++)
	{
		// Highpass
		// hp[i] = odd[i] - (1.0 / 2.0) * (even[i] + even[i + 1])
		if (i < len - 2)
			out[len + i] = in[(i * 2) + 1] - (in[(i * 2)] + in[(i * 2) + 2]) / 2;
		else
			out[len + i] = in[(i * 2) + 1] - (in[(i * 2)]); // Fake last value

		// Lowpass
		// lp[i] = even[i] + (1.0 / 4.0) * (hp[i] + hp[i - 1])
		if (i > 0)
			out[i] = in[(i * 2)] + (out[len + i] + out[len + i - 1]) / 4;
		else
			out[i] = in[(i * 2)] + (out[len + i]) / 2; // Fake first value

#if (AKO_DEV_EMPIRICAL_OBSERVATION == 1)
		if (out[len + i] < s_min_hp)
			s_min_hp = out[len + i];
		if (out[len + i] > s_max_hp)
			s_max_hp = out[len + i];
#endif
	}

	// Degrade highpass
	const int16_t gate = (int16_t)(s->detail_gate * 2.0f * ((float)len / (float)initial_len)); // Type A
	// const int16_t gate = (int16_t)(s->detail_gate * 2.0f * sqrtf((float)len / (float)initial_len)); // Type B
	// const int16_t gate =
	//    (int16_t)(s->detail_gate * 2.0f *
	//              ((sqrtf((float)len / (float)initial_len) + (float)len / (float)initial_len) / 2.0f)); // Type (A +
	//              B) / 2

	// const int16_t gate = (int16_t)(s->detail_gate * 2.0f * log2f((float)len * (1024.0f / (float)initial_len))); //
	// Type C

	for (size_t i = 0; i < len; i++)
	{
		// Gate
		if (out[len + i] > -gate && out[len + i] < gate)
			out[len + i] = 0;
	}
}

static void sLift2d(const struct LiftSettings* s, size_t dimension, size_t initial_dimension, void* aux_buffer,
                    int16_t* inout)
{
	int16_t* buffer_a = (int16_t*)aux_buffer;
	int16_t* buffer_b = (int16_t*)aux_buffer + initial_dimension;
	int16_t* temp = inout;

	// Rows
	for (size_t i = 0; i < dimension; i++)
	{
		memcpy(buffer_a, temp, sizeof(int16_t) * dimension);
		sLift1d(s, dimension / 2, initial_dimension, buffer_a, temp);

		temp = temp + initial_dimension;
	}

	// Columns
	for (size_t i = 0; i < dimension; i++)
	{
		temp = inout + i;
		for (size_t u = 0; u < dimension; u++)
		{
			buffer_a[u] = *temp;
			temp = temp + initial_dimension;
		}

		sLift1d(s, dimension / 2, initial_dimension, buffer_a, buffer_b);

		temp = inout + i;
		for (size_t u = 0; u < dimension; u++)
		{
			*temp = buffer_b[u];
			temp = temp + initial_dimension;
		}
	}
}

static void sLiftPlane(const struct LiftSettings* s, size_t dimension, void** aux_buffer, void* inout)
{
	size_t current_dimension = dimension;
	size_t initial_dimension = dimension;

	*aux_buffer = realloc(*aux_buffer, sizeof(int16_t) * dimension * 2);
	assert(*aux_buffer != NULL);

	while (current_dimension != 2)
	{
		sLift2d(s, current_dimension, initial_dimension, *aux_buffer, inout);
		current_dimension = current_dimension / 2;

#if (AKO_DEV_EMPIRICAL_OBSERVATION == 1)
		printf("%i\t%i\n", s_min_hp, s_max_hp);
		s_min_hp = INT16_MAX;
		s_max_hp = INT16_MIN;
#endif
	}

#if (AKO_DEV_EMPIRICAL_OBSERVATION == 1)
	printf("\n");
#endif
}
#endif


#define MAX_STEPS 99 // TODO

static void s2dToLinearH(size_t w, size_t h, size_t pitch, const int16_t* in, int16_t* out)
{
	for (size_t r = 0; r < h; r++)
	{
		for (size_t c = 0; c < w; c++)
		{
			*out = in[(r * pitch) + c];
			out = out + 1;
		}
	}
}

static void s2dToLinearV(size_t w, size_t h, size_t pitch, const int16_t* in, int16_t* out)
{
	for (size_t c = 0; c < w; c++)
	{
		for (size_t r = 0; r < h; r++)
		{
			*out = in[c + (r * pitch)];
			out = out + 1;
		}
	}
}

static void sPack(size_t dimension, size_t channels, int16_t* inout)
{
	int16_t* lp = NULL;
	int16_t* step[MAX_STEPS] = {NULL};
	size_t s = 0;

	// Initial lowpass (2x2 px)
	lp = calloc(1, sizeof(int16_t) * 2 * 2 * channels);

	for (size_t ch = 0; ch < channels; ch++)
	{
		const int16_t* in_plane = inout + dimension * dimension * ch;
		s2dToLinearH(2, 2, dimension, in_plane, lp + 2 * 2 * ch);
	}

	// Highpasses (from 4x4 to 8x8, to 16x16, to 32x32...)
	for (size_t current = 2; current < dimension; current = current * 2)
	{
		const size_t length = current * current * channels * 3; // Space for B, C and D
		step[s] = calloc(1, sizeof(int16_t) * length);

#if 1
		for (size_t ch = 0; ch < channels; ch++)
		{
			const int16_t* in_plane = inout + dimension * dimension * ch;

			s2dToLinearV(current, current, dimension,
			             in_plane + dimension * current, // Higpass C
			             (step[s] + current * current * 3 * ch) + (current * current * 0));
			s2dToLinearV(current, current * 2, dimension,
			             in_plane + current, // Highpasses B and D
			             (step[s] + current * current * 3 * ch) + (current * current * 1));
		}
#else
		const int16_t color = s * 32;
		for (size_t i = 0; i < length; i++)
			step[s][i] = color; // To inspect it visually
#endif
		s = s + 1;
	}

	// Output!
	memset(inout, 0, sizeof(int16_t) * dimension * dimension * channels);
	memcpy(inout, lp, sizeof(int16_t) * 2 * 2 * channels);

	inout = inout + 2 * 2 * channels;
	s = 0;

	for (size_t current = 2; current < dimension; current = current * 2)
	{
		const size_t length = current * current * channels * 3;
		memcpy(inout, step[s], sizeof(int16_t) * length);
		free(step[s]);

		inout = inout + length;
		s = s + 1;
	}
}


size_t AkoEncode(size_t dimension, size_t channels, const struct AkoSettings* settings, const uint8_t* input,
                 void** output)
{
	assert(input != NULL);
	assert(channels <= 4);
	assert(sEncodeDimension(dimension) != UINT16_MAX);

	size_t data_len = dimension * dimension * channels;
	size_t compressed_data_size = 0;

	int16_t* main_buffer = malloc(sizeof(struct AkoHead) + sizeof(int16_t) * (dimension * dimension * channels));
	int16_t* aux_buffer = malloc(sizeof(int16_t) * (dimension * dimension));
	assert(main_buffer != NULL);
	assert(aux_buffer != NULL);

	int16_t* data = (int16_t*)((uint8_t*)main_buffer + sizeof(struct AkoHead));

	// Transform color-format
	sTransformColorFormat(dimension, channels, input, data);

	// Lift
#if (AKO_ENCODER_WAVELET_TRANSFORMATION != 0)
	struct LiftSettings s = {0};
	DevBenchmarkStart("Lift");
	switch (channels)
	{
	case 4:
		s.uniform_gate = settings->uniform_gate[3];
		s.detail_gate = settings->detail_gate[3];
		sLiftPlane(&s, dimension, (void**)&aux_buffer, data + (dimension * dimension * 3));
	case 3:
		s.uniform_gate = settings->uniform_gate[2];
		s.detail_gate = settings->detail_gate[2];
		sLiftPlane(&s, dimension, (void**)&aux_buffer, data + (dimension * dimension * 2));
	case 2:
		s.uniform_gate = settings->uniform_gate[1];
		s.detail_gate = settings->detail_gate[1];
		sLiftPlane(&s, dimension, (void**)&aux_buffer, data + (dimension * dimension * 1));
	case 1:
		s.uniform_gate = settings->uniform_gate[0];
		s.detail_gate = settings->detail_gate[0];
		sLiftPlane(&s, dimension, (void**)&aux_buffer, data);
		break;
	}
	DevBenchmarkStop();

	DevBenchmarkStart("Pack");
	sPack(dimension, channels, data);
	DevBenchmarkStop();
#else
	(void)settings;
#endif

	// Compress
#if (AKO_COMPRESSION == 1)
	DevBenchmarkStart("CompressLZ4");
	compressed_data_size = sCompressLZ4(sizeof(int16_t) * data_len, (void**)&aux_buffer, data);
	DevBenchmarkStop();

	main_buffer = realloc(main_buffer, (sizeof(struct AkoHead) + compressed_data_size));
	assert(main_buffer != NULL);
#else
	compressed_data_size = sizeof(int16_t) * data_len;
#endif

	// Bye!
	sWriteHead(main_buffer, dimension, channels, compressed_data_size);
	DevBenchmarkTotal();

	if (output != NULL)
		*output = main_buffer;
	else
		free(main_buffer);

	free(aux_buffer);
	return (sizeof(struct AkoHead) + compressed_data_size);
}
