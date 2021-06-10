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
#include "misc.h"


extern void DevBenchmarkStart(const char* name);
extern void DevBenchmarkStop();
extern void DevBenchmarkTotal();
extern void DevPrintf(const char* format, ...);
extern void DevSaveGrayPgm(size_t width, size_t height, const int16_t* data, const char* filename_format, ...);


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
	void* blob_memory = malloc(blob_memory_size);
	assert(blob_memory != NULL);

	const size_t tiles_no = TilesNo(width, height, s->tiles_dimension);
	const size_t tile_memory_size = TileWorstLength(width, height, s->tiles_dimension) * sizeof(int16_t) * channels;
	DevPrintf("###\t[%zux%zu px , %zu channels, %zu px tiles, %zu tiles, reserved %zu bytes]\n", width, height,
	          channels, s->tiles_dimension, tiles_no, tile_memory_size);

	FrameWrite(width, height, channels, s->tiles_dimension, blob_memory);

	// Proccess tiles
	{
		void* aux_memory = malloc(sizeof(int16_t) * s->tiles_dimension * 2); // Two scanlines
		assert(aux_memory != NULL);

		int16_t* tile_memory_a = malloc(tile_memory_size);
		int16_t* tile_memory_b = malloc(tile_memory_size);
		assert(tile_memory_a != NULL);
		assert(tile_memory_b != NULL);

		memset(tile_memory_a, 0, tile_memory_size);
		memset(tile_memory_b, 0, tile_memory_size);

		size_t col = 0;
		size_t row = 0;
		size_t tile_width = 0;
		size_t tile_height = 0;
		size_t tile_size = 0;

		for (size_t i = 0; i < tiles_no; i++)
		{
			// Tile dimensions, border tiles not always are square
			tile_width = s->tiles_dimension;
			tile_height = s->tiles_dimension;

			if ((col + s->tiles_dimension) > width)
				tile_width = width - col;
			if ((row + s->tiles_dimension) > height)
				tile_height = height - row;

			tile_size = TileLength(tile_width, tile_height) * sizeof(int16_t) * channels;
			DevPrintf("###\t[Tile %zu, x: %zu, y: %zu, %zux%zu px]\n", i, col, row, tile_width, tile_height);

			// Proccess
			FormatToPlanarI16YUV(tile_width, tile_height, channels, width, in + (width * row + col) * channels,
			                     tile_memory_a);
			DwtTransform(tile_width, tile_height, channels, aux_memory, tile_memory_a, tile_memory_b);

			// Resize blob
			{
				blob_memory_size = blob_memory_size + tile_size;
				blob_memory = realloc(blob_memory, blob_memory_size); // TODO, use a exponential-growth buffer thing
				assert(blob_memory != NULL);
			}

			// """Compress"""
			{
				memcpy((uint8_t*)blob_memory + (blob_memory_size - tile_size), tile_memory_b, tile_size);
			}

			// Developers, developers, developers
			sDevDumpTiles(tile_width, tile_height, channels, i, tile_memory_b);

			// Next tile
			col = col + tile_width;
			if (col >= width)
			{
				col = 0;
				row = row + tile_height;
			}
		}

		free(aux_memory);
		free(tile_memory_a);
		free(tile_memory_b);
	}

	// Bye!
	*out = blob_memory;
	return blob_memory_size;
}
