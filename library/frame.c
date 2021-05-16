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


#define VALID_DIMENSIONS_NO 16
static const size_t s_valid_dimensions[VALID_DIMENSIONS_NO] = {
    2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536,
};


static inline uint32_t sEncodeDimension(size_t no)
{
	for (uint32_t i = 0; i < VALID_DIMENSIONS_NO; i++)
	{
		if (s_valid_dimensions[i] == no)
			return i;
	}

	assert(1 == 0);
	return 0;
}


static inline size_t sDecodeDimension(uint32_t no)
{
	assert(no < VALID_DIMENSIONS_NO);
	return s_valid_dimensions[no];
}


void FrameWrite(size_t dimension, size_t channels, size_t compressed_data_size, void* output)
{
	struct AkoHead* head = output;

	head->magic[0] = 'A';
	head->magic[1] = 'k';
	head->magic[2] = 'o';
	head->magic[3] = 'I';
	head->magic[4] = 'm';

	head->major = AKO_VER_MAJOR;
	head->minor = AKO_VER_MINOR;
	head->format = AKO_FORMAT;

	assert(compressed_data_size <= UINT32_MAX); // TODO, a compressor responsibility

	head->compressed_data_size = (uint32_t)compressed_data_size;
	head->info = sEncodeDimension(dimension) | (uint32_t)(channels << 4);
}


void FrameRead(const void* input, size_t* out_dimension, size_t* out_channels, size_t* out_compressed_data_size)
{
	const struct AkoHead* head = input;

	assert(memcmp(head->magic, "AkoIm", 5) == 0);

	assert(head->major == AKO_VER_MAJOR);
	assert(head->minor == AKO_VER_MINOR);
	assert(head->format == AKO_FORMAT);

	*out_dimension = sDecodeDimension((head->info << (32 - 4)) >> (32 - 4));
	*out_channels = (size_t)(head->info >> 4);
	*out_compressed_data_size = (size_t)head->compressed_data_size;
}
