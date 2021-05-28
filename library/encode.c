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

 [encode.c]
 - Alexander Brandt 2021
-----------------------------*/

#include <assert.h>
#include <string.h>

#include "ako.h"

#include "dwt.h"
#include "entropy.h"
#include "format.h"
#include "frame.h"


extern void DevBenchmarkStart(const char* name);
extern void DevBenchmarkStop();
extern void DevBenchmarkTotal();
extern void DevPrintf(const char* format, ...);


#define DUMP_RAW 0
#if (DUMP_RAW == 1)
#include <stdio.h>
#endif


static inline size_t sTilesNo(size_t width, size_t height, size_t tile_size)
{
	size_t w = (width / tile_size);
	size_t h = (height / tile_size);
	w = (width % tile_size != 0) ? (w + 1) : w;
	h = (height % tile_size != 0) ? (h + 1) : h;

	return (w * h);
}


size_t AkoEncode(size_t width, size_t height, size_t channels, const struct AkoSettings* settings, const uint8_t* in,
                 void** out)
{
	size_t main_buffer_size = sizeof(struct AkoHead);
	void* main_buffer = calloc(1, main_buffer_size);

	DevPrintf("### [%zux%zu px , %zu channels, %zu px tiles size]\n", width, height, channels, settings->tiles_size);
	DevPrintf("### [%zu tiles]\n", sTilesNo(width, height, settings->tiles_size));
	FrameWrite(width, height, channels, settings->tiles_size, main_buffer);

	// Do the thing
	// TODO
	(void)in;

	// Bye!
	*out = main_buffer;
	return main_buffer_size;
}


#if 0
size_t AkoEncode(size_t dimension, size_t channels, const struct AkoSettings* settings, const uint8_t* in, void** out)
{
	assert(in != NULL);
	assert(channels <= 4);

	size_t data_len = dimension * dimension * channels;
	size_t compressed_data_size = 0;

	int16_t* main_buffer = malloc(sizeof(struct AkoHead) + sizeof(int16_t) * (dimension * dimension * channels) * 4);
	int16_t* aux_buffer = malloc(sizeof(int16_t) * (dimension * dimension));
	assert(main_buffer != NULL);
	assert(aux_buffer != NULL);

	int16_t* data = (int16_t*)((uint8_t*)main_buffer + sizeof(struct AkoHead));

	// To internal format
	DevBenchmarkStart("Format");
	FormatToPlanarI16YUV(dimension, channels, in, data);
	DevBenchmarkStop();

	// Lift
	DevBenchmarkStart("Lift");
	{
		struct DwtLiftSettings s = {0};
		switch (channels)
		{
		case 4:
			s.detail_gate = settings->detail_gate[3];
			s.limit = (settings->limit[3] < dimension) ? settings->limit[3] : dimension;
			DwtLiftPlane(&s, dimension, (void**)&aux_buffer, data + (dimension * dimension * 3));
			// fallthrough
		case 3:
			s.detail_gate = settings->detail_gate[2];
			s.limit = (settings->limit[2] < dimension) ? settings->limit[2] : dimension;
			DwtLiftPlane(&s, dimension, (void**)&aux_buffer, data + (dimension * dimension * 2));
			// fallthrough
		case 2:
			s.detail_gate = settings->detail_gate[1];
			s.limit = (settings->limit[1] < dimension) ? settings->limit[1] : dimension;
			DwtLiftPlane(&s, dimension, (void**)&aux_buffer, data + (dimension * dimension * 1));
			// fallthrough
		case 1:
			s.detail_gate = settings->detail_gate[0];
			s.limit = (settings->limit[0] < dimension) ? settings->limit[0] : dimension;
			DwtLiftPlane(&s, dimension, (void**)&aux_buffer, data);
		}
	}
	DevBenchmarkStop();

	// Pack
	DevBenchmarkStart("Pack");
	DwtPackImage(dimension, channels, data);
	DevBenchmarkStop();

#if (DUMP_RAW == 1)
	FILE* fp = fopen("/tmp/ako.raw", "wb");
	assert(fp != NULL);
	fwrite(data, sizeof(int16_t), data_len, fp);
	fclose(fp);
#endif

	// Compress
	DevBenchmarkStart("Compress");
	{
		compressed_data_size = EntropyCompress(data_len, (void**)&aux_buffer, data);
		main_buffer = realloc(main_buffer, (sizeof(struct AkoHead) + compressed_data_size));
		assert(main_buffer != NULL);
	}
	DevBenchmarkStop();

	// Bye!
	FrameWrite(dimension, channels, compressed_data_size, main_buffer);
	DevBenchmarkTotal();

	if (out != NULL)
		*out = main_buffer;
	else
		free(main_buffer);

	free(aux_buffer);
	return (sizeof(struct AkoHead) + compressed_data_size);
}
#endif
