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

 [frame.c]
 - Alexander Brandt 2021
-----------------------------*/

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "ako.h"
#include "frame.h"


#define VALID_TILES_DIMENSIONS_NO 16 // Only 16 values to encode them in 4 bits
static size_t s_valid_tiles_dimensions[VALID_TILES_DIMENSIONS_NO] = {
    16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288};


void FrameWrite(size_t width, size_t height, size_t channels, size_t tiles_dimension, void* output)
{
	assert(width > 0 && width <= UINT32_MAX);
	assert(height > 0 && height <= UINT32_MAX);
	assert(channels > 0 && channels <= 4);

	int dimension = -1; // Invalid value
	for (int i = 0; i < VALID_TILES_DIMENSIONS_NO; i++)
	{
		if (s_valid_tiles_dimensions[i] == tiles_dimension)
			dimension = i;
	}
	assert(dimension != -1);

	struct AkoHead* head = output;

	head->magic[0] = 'A';
	head->magic[1] = 'k';
	head->magic[2] = 'o';
	head->magic[3] = 'I';

	head->version = AKO_FORMAT_VERSION;
	head->unused1 = 0;
	head->unused2 = 0;

	head->format = (uint8_t)((uint8_t)dimension | ((channels - 1) << 4));

	head->width = (uint32_t)width;
	head->height = (uint32_t)height;
}


void FrameRead(const void* input, size_t input_size, size_t* out_width, size_t* out_height, size_t* out_channels,
               size_t* out_tiles_dimensions)
{
	assert(input_size >= sizeof(struct AkoHead));

	const struct AkoHead* head = input;

	assert(memcmp(head->magic, "AkoI", 4) == 0);
	assert(head->version == AKO_FORMAT_VERSION);

	*out_width = (size_t)head->width;
	*out_height = (size_t)head->height;
	*out_channels = (size_t)(head->format >> 4) + 1;
	*out_tiles_dimensions = s_valid_tiles_dimensions[head->format & 0b00001111];

	assert(*out_width != 0);
	assert(*out_height != 0);
	assert(*out_channels != 0);
}
