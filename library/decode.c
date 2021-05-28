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

#include "dwt.h"
#include "entropy.h"
#include "format.h"
#include "frame.h"


extern void DevBenchmarkStart(const char* name);
extern void DevBenchmarkStop();
extern void DevBenchmarkTotal();
extern void DevPrintf(const char* format, ...);


static inline size_t sTilesNo(size_t width, size_t height, size_t tile_size)
{
	size_t w = (width / tile_size);
	size_t h = (height / tile_size);
	w = (width % tile_size != 0) ? (w + 1) : w;
	h = (height % tile_size != 0) ? (h + 1) : h;

	return (w * h);
}


uint8_t* AkoDecode(size_t input_size, const void* in, size_t* out_width, size_t* out_height, size_t* out_channels)
{
	size_t width = 0;
	size_t height = 0;
	size_t channels = 0;
	size_t tiles_size = 0;

	FrameRead(in, input_size, &width, &height, &channels, &tiles_size);
	DevPrintf("### [%zux%zu px , %zu channels, %zu px tiles size]\n", width, height, channels, tiles_size);
	DevPrintf("### [%zu tiles]\n", sTilesNo(width, height, tiles_size));

	size_t image_buffer_size = sizeof(uint8_t) * width * height * channels;
	void* image_buffer = calloc(1, image_buffer_size);

	// Do the thing
	// TODO

	// Bye!
	*out_width = width;
	*out_height = height;
	*out_channels = channels;
	return image_buffer;
}


#if 0
uint8_t* AkoDecode(size_t input_size, const void* in, size_t* out_dimension, size_t* out_channels)
{
	size_t dimension = 0;
	size_t channels = 0;
	size_t data_len = 0;
	size_t compressed_data_size = 0;

	int16_t* buffer_a = NULL;
	int16_t* buffer_b = NULL;
	int16_t* aux_buffer = NULL;

	assert(input_size >= sizeof(struct AkoHead));
	FrameRead(in, &dimension, &channels, &compressed_data_size);

	data_len = dimension * dimension * channels;
	buffer_a = calloc(1, sizeof(int16_t) * data_len);
	buffer_b = calloc(1, sizeof(int16_t) * data_len);
	aux_buffer = calloc(1, sizeof(int16_t) * dimension * 2);

	assert(buffer_a != NULL);
	assert(buffer_b != NULL);
	assert(aux_buffer != NULL);

	// Decompress, Unlift, toRgb
	{
		const int16_t* skip_head = (int16_t*)((uint8_t*)in + sizeof(struct AkoHead));

		DevBenchmarkStart("Decompress");
		EntropyDecompress(compressed_data_size, data_len, skip_head, buffer_a);
		DevBenchmarkStop();

		DevBenchmarkStart("Unpack/Unlift");
		DwtUnpackUnliftImage(dimension, channels, aux_buffer, buffer_a, buffer_b);
		DevBenchmarkStop();

		DevBenchmarkStart("Format");
		FormatToInterlacedU8RGB(dimension, channels, buffer_b, (uint8_t*)buffer_a); // HACK!
		DevBenchmarkStop();
	}

	// Bye!
	DevBenchmarkTotal();

	if (out_dimension != NULL)
		*out_dimension = dimension;
	if (out_channels != NULL)
		*out_channels = channels;

	free(aux_buffer);
	free(buffer_b);
	return (uint8_t*)buffer_a; // TODO, realloc!
}
#endif
