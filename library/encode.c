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
extern void DevSaveGrayPgm(size_t width, size_t height, const int16_t* data, const char* filename_format, ...);


static inline size_t sTilesNo(size_t width, size_t height, size_t tile_size)
{
	size_t w = (width / tile_size);
	size_t h = (height / tile_size);
	w = (width % tile_size != 0) ? (w + 1) : w;
	h = (height % tile_size != 0) ? (h + 1) : h;

	return (w * h);
}


static void sDevDumpTiles(size_t width, size_t height, size_t channels, size_t tile_no, const int16_t* in)
{
	switch (channels)
	{
	case 4: DevSaveGrayPgm(width, height, in + (width * height) * 3, "/tmp/tile-%02zu-a.pgm", tile_no);
	case 3: DevSaveGrayPgm(width, height, in + (width * height) * 2, "/tmp/tile-%02zu-v.pgm", tile_no);
	case 2: DevSaveGrayPgm(width, height, in + (width * height) * 1, "/tmp/tile-%02zu-u.pgm", tile_no);
	case 1: DevSaveGrayPgm(width, height, in + (width * height) * 0, "/tmp/tile-%02zu-y.pgm", tile_no);
	}
}


size_t AkoEncode(size_t width, size_t height, size_t channels, const struct AkoSettings* s, const uint8_t* in,
                 void** out)
{
	size_t blob_memory_size = sizeof(struct AkoHead);
	void* blob_memory = calloc(1, blob_memory_size);
	assert(blob_memory != NULL);

	size_t tiles_no = sTilesNo(width, height, s->tiles_size);
	DevPrintf("###\t[%zux%zu px , %zu channels, %zu px tiles size]\n", width, height, channels, s->tiles_size);
	DevPrintf("###\t[%zu tiles]\n", tiles_no);

	FrameWrite(width, height, channels, s->tiles_size, blob_memory);

	// Proccess tiles
	{
		void* aux_memory = calloc(1, sizeof(int16_t) * s->tiles_size * 2); // Two scanlines
		int16_t* tile_memory = calloc(1, sizeof(int16_t) * s->tiles_size * s->tiles_size * channels);
		assert(aux_memory != NULL);
		assert(tile_memory != NULL);

		size_t col = 0;
		size_t row = 0;
		size_t tile_width = 0;
		size_t tile_height = 0;
		size_t tile_size = 0;

		for (size_t i = 0; i < tiles_no; i++)
		{
			// Tiles size, border tiles not always are square
			tile_width = s->tiles_size;
			tile_height = s->tiles_size;

			if ((col + s->tiles_size) > width)
				tile_width = width - col;

			if ((row + s->tiles_size) > height)
				tile_height = height - row;

			tile_size = sizeof(int16_t) * tile_width * tile_height * channels;

			// Color transform
			FormatToPlanarI16YUV(tile_width, tile_height, channels, width, in + (width * row + col) * channels,
			                     tile_memory);

#if (AKO_WAVELET != 0)

			// Lift
			struct DwtLiftSettings dwt_s = {0};
			switch (channels)
			{
			case 4:
				dwt_s.detail_gate = s->detail_gate[3];
				DwtLiftPlane(&dwt_s, tile_width, aux_memory, tile_memory + (tile_width * tile_height * 3));
			case 3:
				dwt_s.detail_gate = s->detail_gate[2];
				DwtLiftPlane(&dwt_s, tile_width, aux_memory, tile_memory + (tile_width * tile_height * 2));
			case 2:
				dwt_s.detail_gate = s->detail_gate[1];
				DwtLiftPlane(&dwt_s, tile_width, aux_memory, tile_memory + (tile_width * tile_height * 1));
			case 1: dwt_s.detail_gate = s->detail_gate[0]; DwtLiftPlane(&dwt_s, tile_width, aux_memory, tile_memory);
			}

			// Pack
			// Aka: convert 2d memory to linear memory
			DwtPackImage(tile_width, channels, tile_memory); // FIXME, allocates lot of memory :(
#endif

			// Resize blob
			{
				blob_memory_size = blob_memory_size + tile_size;
				blob_memory = realloc(blob_memory, blob_memory_size); // TODO, use a exponential-growth buffer thing
				assert(blob_memory != NULL);
			}

			// """Compress"""
			{
				memcpy((uint8_t*)blob_memory + (blob_memory_size - tile_size), tile_memory, tile_size);
			}

			// Developers, developers, developers
			DevPrintf("###\t%4zu, %4zu\n", col, row);
			sDevDumpTiles(tile_width, tile_height, channels, i, tile_memory);

			// Next tile
		next_tile:
			col = col + tile_width;
			if (col >= width)
			{
				col = 0;
				row = row + tile_height;
			}
		}

		free(aux_memory);
		free(tile_memory);
	}

	// Bye!
	*out = blob_memory;
	return blob_memory_size;
}
