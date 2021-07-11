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

 [decode.c]
 - Alexander Brandt 2021
-----------------------------*/

#include <assert.h>
#include <string.h>

#include "ako.h"

#include "developer.h"
#include "dwt.h"
#include "entropy.h"
#include "format.h"
#include "frame.h"
#include "misc.h"


uint8_t* AkoDecode(size_t input_size, const void* in, size_t* out_w, size_t* out_h, size_t* out_channels)
{
	size_t image_w, image_h, channels, tiles_dimension;
	FrameRead(in, input_size, &image_w, &image_h, &channels, &tiles_dimension);

	const size_t tiles_no = TilesNo(image_w, image_h, tiles_dimension);
	DevPrintf("###\t%zux%zu px , %zu channels, %zu px tiles, %zu tiles\n", image_w, image_h, channels, tiles_dimension,
	          tiles_no);

	const size_t workarea_size = WorkareaLength(image_w, image_h, tiles_dimension) * channels * sizeof(int16_t);
	DevPrintf("###\tWorkarea of %.2f kB\n", (float)workarea_size / 500.0f);

	uint8_t* image_memory = malloc(sizeof(uint8_t) * image_w * image_h * channels);
	assert(image_memory != NULL);

	// Proccess tiles
	struct Benchmark bench_decompression = DevBenchmarkCreate("Decompression");
	struct Benchmark bench_color = DevBenchmarkCreate("Color transformation");
	struct Benchmark bench_wavelet = DevBenchmarkCreate("Wavelet transformation");
	struct Benchmark bench_total = DevBenchmarkCreate("Total");
	DevBenchmarkStartResume(&bench_total);
	{
		void* aux_memory = malloc(workarea_size / 2 + sizeof(int16_t) * tiles_dimension * 4); // Four scanlines
		assert(aux_memory != NULL);

		int16_t* workarea_a = malloc(workarea_size);
		int16_t* workarea_b = malloc(workarea_size);
		assert(workarea_a != NULL);
		assert(workarea_b != NULL);

		memset(aux_memory, 0, workarea_size / 2 + sizeof(int16_t) * tiles_dimension * 4);
		memset(workarea_a, 0, workarea_size);
		memset(workarea_b, 0, workarea_size);

		size_t col = 0;
		size_t row = 0;
		size_t tile_w = 0;
		size_t tile_h = 0;
		size_t tile_length = 0;
		size_t planes_space = 0;

		const uint8_t* blob = (const uint8_t*)in + sizeof(struct AkoHead);

		for (size_t i = 0; i < tiles_no; i++)
		{
			// Tile dimensions, border tiles not always are square
			tile_w = tiles_dimension;
			tile_h = tiles_dimension;

			if ((col + tiles_dimension) > image_w)
				tile_w = image_w - col;
			if ((row + tiles_dimension) > image_h)
				tile_h = image_h - row;

			tile_length = TileTotalLength(tile_w, tile_h) * channels;

			planes_space = 0; // Non divisible by two dimensions add an extra row and/or column
			planes_space = (tile_w % 2 == 0) ? planes_space : (planes_space + tile_h);
			planes_space = (tile_h % 2 == 0) ? planes_space : (planes_space + tile_w);

			if (i < 10)
				DevPrintf("###\t[Tile %zu, x: %zu, y: %zu, %zux%zu px, spacing: %zu]\n", i, col, row, tile_w, tile_h,
				          planes_space);
			else if (i == 10)
				DevPrintf("###\t[Tile ...]\n");

			// Decompress
			DevBenchmarkStartResume(&bench_decompression);
			{
				size_t compressed_size = *((uint32_t*)blob);

#if (AKO_COMPRESSION == 1)
				memset(workarea_a, 0, workarea_size); // FIXME, EntropyDecompress() error

				EntropyDecompress(compressed_size, tile_length, blob + sizeof(uint32_t), workarea_a);
				blob = blob + compressed_size + sizeof(uint32_t);
#else
				memcpy(workarea_a, blob + sizeof(uint32_t), compressed_size);
				blob = blob + compressed_size + sizeof(uint32_t);
#endif
			}
			DevBenchmarkPause(&bench_decompression);

			// Proccess
			DevBenchmarkStartResume(&bench_wavelet);
			{
				InverseDwtTransform(tile_w, tile_h, channels, planes_space, aux_memory, workarea_a, workarea_b);
			}
			DevBenchmarkPause(&bench_wavelet);

			DevBenchmarkStartResume(&bench_color);
			{
				FormatToInterlacedU8RGB(tile_w, tile_h, channels, planes_space, image_w, workarea_b,
				                        image_memory + (image_w * row + col) * channels);
			}
			DevBenchmarkPause(&bench_color);

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
	DevBenchmarkPrint(&bench_decompression);
	DevBenchmarkPrint(&bench_wavelet);
	DevBenchmarkPrint(&bench_color);
	DevBenchmarkPrint(&bench_total);

	// Bye!
	*out_w = image_w;
	*out_h = image_h;
	*out_channels = channels;
	return image_memory;
}
