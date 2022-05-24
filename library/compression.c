/*

MIT License

Copyright (c) 2021-2022 Alexander Brandt

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


static size_t sManbavaranCompress(const struct akoSettings* s, size_t channels, size_t tile_w, size_t tile_h,
                                  coeff_t* input, void* out);


size_t akoCompress(const struct akoSettings* s, size_t channels, size_t tile_w, size_t tile_h, coeff_t* input,
                   void* output)
{
	if (s->compression == AKO_COMPRESSION_MANBAVARAN)
		return sManbavaranCompress(s, channels, tile_w, tile_h, input, output);

	const size_t input_size = akoTileDataSize(tile_w, tile_h) * channels;
	const size_t output_size = input_size;

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


//-----------------------------


struct akoCompressCallbackData
{
	void* out;
	size_t out_size;
	size_t out_capacity;
};


static void sCompressLp(const struct akoSettings* s, size_t ch, size_t tile_w, size_t tile_h, size_t target_w,
                        size_t target_h, coeff_t* input, void* raw_data)
{
	struct akoCompressCallbackData* data = raw_data;
}

static void sCompressHp(const struct akoSettings* s, size_t ch, size_t tile_w, size_t tile_h,
                        const struct akoLiftHead* head, size_t current_w, size_t current_h, size_t target_w,
                        size_t target_h, coeff_t* aux, coeff_t* hp_c, coeff_t* hp_b, coeff_t* hp_d, void* raw_data)
{
	struct akoCompressCallbackData* data = raw_data;
}

static size_t sManbavaranCompress(const struct akoSettings* s, size_t channels, size_t tile_w, size_t tile_h,
                                  coeff_t* input, void* out)
{
	struct akoCompressCallbackData data = {0};
	data.out = out;
	data.out_capacity = akoTileDataSize(tile_w, tile_h) * channels;

	akoIterateLifts(s, channels, tile_w, tile_h, input, sCompressLp, sCompressHp, &data);

	return data.out_size;
}


static inline ucoeff_t sZigZagEncode(coeff_t in)
{
	// https://developers.google.com/protocol-buffers/docs/encoding#signed_integers
	return (ucoeff_t)((in << 1) ^ (in >> 15));
}

static inline coeff_t sZigZagDecode(ucoeff_t in)
{
	return (coeff_t)((in >> 1) ^ (~(in & 1) + 1));
}

void akoZigZagToUnsigned(size_t len, void* inout_raw)
{
	union
	{
		ucoeff_t* u;
		coeff_t* s;
	} inout;
	inout.u = inout_raw;

	for (; len != 0; len--, inout.u++)
		*inout.u = sZigZagEncode(*inout.s);
}

void akoZigZagToSigned(size_t len, void* inout_raw)
{
	union
	{
		ucoeff_t* u;
		coeff_t* s;
	} inout;
	inout.s = inout_raw;

	for (; len != 0; len--, inout.s++)
		*inout.s = sZigZagDecode(*inout.u);
}
