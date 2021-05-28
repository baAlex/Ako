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


#define VALID_TILES_SIZE_NO 8 // Only 8 values to encode them in 3 bits
static size_t s_valid_tiles_size[VALID_TILES_SIZE_NO] = {8, 16, 32, 64, 128, 256, 512, 1024};


void FrameWrite(size_t width, size_t height, size_t channels, size_t tiles_size, void* output)
{
	assert(width > 0 && width <= UINT32_MAX);
	assert(height > 0 && height <= UINT32_MAX);
	assert(channels > 0 && channels <= 4);

	int tsize = -1; // Invalid value
	for (int i = 0; i < VALID_TILES_SIZE_NO; i++)
	{
		if (s_valid_tiles_size[i] == tiles_size)
			tsize = i;
	}
	assert(tsize != -1);

	struct AkoHead* head = output;

	head->magic[0] = 'A';
	head->magic[1] = 'k';
	head->magic[2] = 'o';
	head->magic[3] = 'I';

	head->version = AKO_FORMAT_VERSION;
	head->unused1 = 0;
	head->unused2 = 0;

	head->format = (uint8_t)((uint8_t)tsize | ((channels - 1) << 3));

	head->width = (uint32_t)width;
	head->height = (uint32_t)height;
}


void FrameRead(const void* input, size_t input_size, size_t* out_width, size_t* out_height, size_t* out_channels,
               size_t* out_tiles_size)
{
	assert(input_size >= sizeof(struct AkoHead));

	const struct AkoHead* head = input;

	assert(memcmp(head->magic, "AkoI", 4) == 0);
	assert(head->version == AKO_FORMAT_VERSION);

	*out_width = (size_t)head->width;
	*out_height = (size_t)head->height;
	*out_channels = (size_t)(head->format >> 3) + 1;
	*out_tiles_size = s_valid_tiles_size[head->format & 0b00000111];

	assert(*out_width != 0);
	assert(*out_height != 0);
	assert(*out_channels != 0);
}
