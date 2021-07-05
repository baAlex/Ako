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
extern void DevSaveGrayPgm(size_t width, size_t height, size_t in_pitch, const int16_t* data,
                           const char* filename_format, ...);


static void sDevDumpTiles(size_t width, size_t height, size_t channels, size_t tile_no, const int16_t* in)
{
	switch (channels)
	{
	case 4:
		DevSaveGrayPgm(width, height, width, in + (width * height) * 3, "/tmp/tile-%02zu-a.pgm",
		               tile_no); // Fallthrough
	case 3:
		DevSaveGrayPgm(width, height, width, in + (width * height) * 2, "/tmp/tile-%02zu-v.pgm",
		               tile_no); // Fallthrough
	case 2:
		DevSaveGrayPgm(width, height, width, in + (width * height) * 1, "/tmp/tile-%02zu-u.pgm",
		               tile_no); // Fallthrough
	case 1:
		DevSaveGrayPgm(width, height, width, in + (width * height) * 0, "/tmp/tile-%02zu-y.pgm",
		               tile_no); // Fallthrough
	}
}


size_t AkoEncode(size_t image_w, size_t image_h, size_t channels, const struct AkoSettings* s, const uint8_t* in,
                 void** out)
{
	size_t blob_size = sizeof(struct AkoHead);
	void* blob = malloc(blob_size);
	assert(blob != NULL);

	const size_t tiles_no = TilesNo(image_w, image_h, s->tiles_dimension);
	DevPrintf("###\t[%zux%zu px , %zu channels, %zu px tiles, %zu tiles]\n", image_w, image_h, channels,
	          s->tiles_dimension, tiles_no);

	const size_t workarea_size = WorkareaLength(image_w, image_h, s->tiles_dimension) * channels * sizeof(int16_t);
	DevPrintf("###\t[Workarea of %.2f kB]\n", (float)workarea_size / 500.0f);

	FrameWrite(image_w, image_h, channels, s->tiles_dimension, blob);

	// Proccess tiles
	{
		void* aux_memory = malloc(sizeof(int16_t) * s->tiles_dimension * 4); // Four scanlines
		assert(aux_memory != NULL);

		int16_t* workarea_a = malloc(workarea_size);
		int16_t* workarea_b = malloc(workarea_size);
		assert(workarea_a != NULL);
		assert(workarea_b != NULL);

		size_t col = 0;
		size_t row = 0;
		size_t tile_w = 0;
		size_t tile_h = 0;
		size_t tile_length = 0;
		size_t planes_space = 0;

		for (size_t i = 0; i < tiles_no; i++)
		{
			// Tile dimensions, border tiles not always are square
			tile_w = s->tiles_dimension;
			tile_h = s->tiles_dimension;

			if ((col + s->tiles_dimension) > image_w)
				tile_w = image_w - col;
			if ((row + s->tiles_dimension) > image_h)
				tile_h = image_h - row;

			tile_length = TileTotalLength(tile_w, tile_h, NULL, NULL) * channels;

			planes_space = 0; // Non divisible by two dimensions add an extra row and/or column
			planes_space = (tile_w % 2 == 0) ? planes_space : (planes_space + tile_h);
			planes_space = (tile_h % 2 == 0) ? planes_space : (planes_space + tile_w);

			DevPrintf("###\t[Tile %zu, x: %zu, y: %zu, %zux%zu px, spacing: %zu]\n", i, col, row, tile_w, tile_h,
			          planes_space);

			// Proccess
			FormatToPlanarI16YUV(tile_w, tile_h, channels, planes_space, image_w, in + (image_w * row + col) * channels,
			                     workarea_a);
			DwtTransform(s, tile_w, tile_h, channels, planes_space, aux_memory, workarea_a, workarea_b);

			// Compress
			{
#if (AKO_COMPRESSION == 1)
				size_t compressed_size = EntropyCompress(tile_length, workarea_b, NULL);
				assert(compressed_size <= UINT32_MAX);
#else
				size_t compressed_size = tile_length * sizeof(uint16_t);
#endif

				blob_size = blob_size + compressed_size + sizeof(uint32_t);
				blob = realloc(blob, blob_size); // TODO, use a exponential-growth buffer thing
				assert(blob != NULL);

#if (AKO_COMPRESSION == 1)
				EntropyCompress(tile_length, workarea_b, (uint8_t*)blob + (blob_size - compressed_size));
#else
				memcpy((uint8_t*)blob + (blob_size - compressed_size), workarea_b, compressed_size);
#endif

				uint32_t* temp = (uint32_t*)((uint8_t*)blob + (blob_size - compressed_size - sizeof(uint32_t)));
				*temp = (uint32_t)compressed_size;
			}

			// Developers, developers, developers
			sDevDumpTiles(tile_w, tile_h, channels, i, workarea_b);

			// Next tile
			col = col + tile_w;
			if (col >= image_w)
			{
				col = 0;
				row = row + tile_h;
			}
		}

		free(aux_memory);
		free(workarea_a);
		free(workarea_b);
	}

	// Bye!
	*out = blob;
	return blob_size;
}
