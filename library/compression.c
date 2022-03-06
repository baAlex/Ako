/*

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
*/


#include "ako-private.h"


struct akoBlockHead
{
	uint32_t block_size;
};


size_t akoCompress(enum akoCompression method, size_t input_size, size_t output_size, void* input, void* output)
{
	(void)method;
	struct akoBlockHead* h = output;

	const size_t compressed_size = akoKagariEncode(input_size, output_size - sizeof(struct akoBlockHead), input,
	                                               (uint8_t*)output + sizeof(struct akoBlockHead));

	if (compressed_size == 0)
		return 0;

	h->block_size = compressed_size;
	AKO_DEV_PRINTF("E\tCompressed %zu -> %u bytes\n", input_size, h->block_size);

	return compressed_size + sizeof(struct akoBlockHead);
}


size_t akoDecompress(enum akoCompression method, size_t decompressed_size, size_t output_size, const void* input,
                     void* output)
{
	(void)method;
	const struct akoBlockHead* h = input;

	const size_t compressed_size = akoKagariDecode(decompressed_size / sizeof(int16_t), (size_t)h->block_size,
	                                               output_size, (uint8_t*)input + sizeof(struct akoBlockHead), output);

	AKO_DEV_PRINTF("D\tDecompressed %zu <- %u bytes (%zu)\n", decompressed_size, h->block_size, compressed_size);

	if (compressed_size == 0 || compressed_size != h->block_size)
		return 0;

	return compressed_size + sizeof(struct akoBlockHead);
}
