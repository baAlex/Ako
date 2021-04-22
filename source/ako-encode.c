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


static void sTransformColorFormat(size_t dimension, size_t channels, const uint8_t* input, uint16_t* out)
{
	// De-interlace into planes
	// If there is an alpha channel, the pixels are copied (or not) accordingly
	DevBenchmarkStart("Transform - Format");
	switch (channels)
	{
	case 4: // Alpha channel
		for (size_t i = 0; i < (dimension * dimension); i++)
			out[(dimension * dimension * 3) + i] = (uint16_t)input[i * channels + 3];
	case 3:
		for (size_t i = 0; i < (dimension * dimension); i++)
		{
			if (channels < 4 || input[i * channels + 3] != 0)
				out[(dimension * dimension * 2) + i] = (uint16_t)input[i * channels + 2];
			else
				out[(dimension * dimension * 2) + i] = 0;
		}
	case 2: // Alpha channel if 'channels == 2'
		for (size_t i = 0; i < (dimension * dimension); i++)
		{
			if (channels < 4 || input[i * channels + 3] != 0)
				out[(dimension * dimension * 1) + i] = (uint16_t)input[i * channels + 1];
			else
				out[(dimension * dimension * 1) + i] = 0;
		}
	case 1:
		for (size_t i = 0; i < (dimension * dimension); i++)
		{
			if (channels == 1 || (channels == 2 && input[i * channels + 1] != 0))
				out[i] = (uint16_t)input[i * channels];
			else if (channels < 4 || input[i * channels + 3] != 0)
				out[i] = (uint16_t)input[i * channels];
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
			const uint16_t r = out[i];
			const uint16_t g = out[(dimension * dimension * 1) + i];
			const uint16_t b = out[(dimension * dimension * 2) + i];

			const uint16_t y = r / 4 + g / 2 + b / 4;
			const uint16_t co = 128 + r / 2 - b / 2;
			const uint16_t cg = 128 - (r / 4) + g / 2 - b / 4;

			out[i] = y;
			out[(dimension * dimension * 1) + i] = co;
			out[(dimension * dimension * 2) + i] = cg;
		}
	}
	DevBenchmarkStop();
#endif
}


#if (AKO_WAVELET_TRANSFORMATION == 1)
static void sLiftPlane(const struct LiftSettings* s, size_t dimension, void* aux_buffer, void* inout)
{
	(void)s;
	(void)dimension;
	(void)aux_buffer;
	(void)inout;
}
#endif


size_t AkoEncode(size_t dimension, size_t channels, const struct AkoSettings* settings, const uint8_t* input,
                 void** output)
{
	assert(input != NULL);
	assert(channels <= 4);
	assert(sEncodeDimension(dimension) != UINT16_MAX);

	size_t data_len = dimension * dimension * channels;
	size_t compressed_data_size = 0;

	uint16_t* main_buffer = malloc(sizeof(struct AkoHead) + sizeof(uint16_t) * (dimension * dimension * channels));
	uint16_t* aux_buffer = malloc(sizeof(uint16_t) * (dimension * dimension));
	assert(main_buffer != NULL);
	assert(aux_buffer != NULL);

	uint16_t* data = (uint16_t*)((uint8_t*)main_buffer + sizeof(struct AkoHead));

	// Transform color-format
	sTransformColorFormat(dimension, channels, input, data);

	// Lift
#if (AKO_WAVELET_TRANSFORMATION != 0)
	struct LiftSettings s = {0};
	DevBenchmarkStart("Lift");
	switch (channels)
	{
	case 4:
		s.uniform_gate = settings->uniform_gate[3];
		s.detail_gate = settings->detail_gate[3];
		sLiftPlane(&s, dimension, aux_buffer, data + (dimension * dimension * 3));
	case 3:
		s.uniform_gate = settings->uniform_gate[2];
		s.detail_gate = settings->detail_gate[2];
		sLiftPlane(&s, dimension, aux_buffer, data + (dimension * dimension * 2));
	case 2:
		s.uniform_gate = settings->uniform_gate[1];
		s.detail_gate = settings->detail_gate[1];
		sLiftPlane(&s, dimension, aux_buffer, data + (dimension * dimension * 1));
	case 1:
		s.uniform_gate = settings->uniform_gate[0];
		s.detail_gate = settings->detail_gate[0];
		sLiftPlane(&s, dimension, aux_buffer, data);
		break;
	}
	DevBenchmarkStop();
#else
	(void)settings;
#endif

	// Compress
#if (AKO_COMPRESSION == 1)
	DevBenchmarkStart("CompressLZ4");
	compressed_data_size = sCompressLZ4(sizeof(uint16_t) * data_len, (void**)&aux_buffer, data);
	DevBenchmarkStop();

	main_buffer = realloc(main_buffer, (sizeof(struct AkoHead) + compressed_data_size));
	assert(main_buffer != NULL);
#else
	compressed_data_size = sizeof(uint16_t) * data_len;
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
