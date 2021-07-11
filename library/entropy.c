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

 [entropy.c]
 - Alexander Brandt 2021
-----------------------------*/

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "entropy.h"


struct EliasEncodeState
{
	size_t acc_usage;
	uint64_t acc; // Kinda, double of the worst case code (33 bits)

	// Output
	size_t output_offset;

	uint16_t prev;
	size_t rle_len;

	// Stats
	size_t values_write;
	size_t bytes_write;
};

struct EliasDecodeState
{
	size_t acc_usage;
	uint64_t acc;

	// Input
	size_t input_offset;

	uint16_t prev;
	size_t rle_len;

	// Stats
	size_t values_read;
	size_t bytes_read;
	bool eof;
	bool end;
};


static void sEmitByte(struct EliasEncodeState* s, uint8_t byte, uint8_t* output)
{
	if (output != NULL)
		output[s->output_offset] = byte;

	s->output_offset = s->output_offset + 1;
	s->bytes_write = s->bytes_write + 1;
}


static void sEliasEncodeStop(struct EliasEncodeState* s, uint8_t* output)
{
	// Can we output a byte?
	while (s->acc_usage >= 8)
	{
		uint8_t byte = (uint8_t)(s->acc >> (s->acc_usage - 8));
		s->acc_usage = s->acc_usage - 8;
		sEmitByte(s, byte, output);
	}

	// Remainder (less than a byte)
	if (s->acc_usage != 0)
	{
		uint8_t byte = (uint8_t)(s->acc << (8 - s->acc_usage));
		s->acc_usage = 0;
		sEmitByte(s, byte, output);
	}

	// Zero byte
	sEmitByte(s, 0, output);
}


static void sEliasEncodeU(struct EliasEncodeState* s, bool bypass_rle, uint16_t v, uint8_t* output)
{
	assert(v != UINT16_MAX);
	v = v + 1;

	// RLE
	if (bypass_rle == false)
	{
		// Activate
		if (v == s->prev && s->rle_len < (UINT16_MAX - 2)) // 's->rle_len < UINT16_MAX' to limit RLE sequences
		{
			s->rle_len += 1;

			if (s->rle_len > 1) // We send two consecutive values as a signal to the decoder
				return;         // to enter in RLE mode, that is why is not an 'rle_len == 1'
		}

		// Desactivate
		else
		{
			// Emit RLE sequence length using the same elias-coding
			if (s->rle_len != 0)
				sEliasEncodeU(s, true, (uint16_t)s->rle_len - 1, output);

			s->rle_len = 0;
		}

		s->prev = v;
	}

	// Can we output a byte?
	// Aka: empty the accumulator
	while (s->acc_usage >= 8)
	{
		uint8_t byte = (uint8_t)(s->acc >> (s->acc_usage - 8));
		s->acc_usage = s->acc_usage - 8;
		sEmitByte(s, byte, output);
	}

	// Encode
	size_t len = 0;
	for (uint16_t temp = v; temp > 1; temp = temp >> 1)
	{
		len = len + 1;
		s->acc = s->acc << 1; // Length in unary
	}

	s->acc = s->acc << (len + 1); // Mark + value in binary
	s->acc = s->acc | (0x01 << len) | (uint64_t)v;

	// Bye!
	s->acc_usage = s->acc_usage + (len * 2 + 1);
	s->values_write = s->values_write + 1;
}


static inline uint16_t sEliasDecodeU(struct EliasDecodeState* s, bool bypass_rle, size_t input_size,
                                     const uint8_t* input)
{
	// RLE!
	if (bypass_rle == false && s->rle_len != 0)
	{
		size_t temp = s->prev;
		s->rle_len -= 1;

		if (s->rle_len == 0)
			s->prev = 666; // FIXMEEE!!!!... or no, this entire module is horrible

		s->values_read = s->values_read + 1;
		return temp;
	}

	// Can we input?
	// Aka: fill the accumulator
	while (s->acc_usage <= (64 - 8))
	{
		uint8_t byte = 0;

		if (s->input_offset != input_size)
		{
			byte = input[s->input_offset];
			s->input_offset = s->input_offset + 1;
		}
		else
		{
			s->eof = true;
			break;
		}

		s->acc = s->acc | ((uint64_t)byte << (64 - 8 - s->acc_usage));
		s->acc_usage = s->acc_usage + 8;
		s->bytes_read = s->bytes_read + 1;
	}

	if (s->eof == true && s->acc == 0)
	{
		s->end = true;
		return 0;
	}

	// Decode
	size_t len = 0;
	for (; (s->acc >> 63) == 0; s->acc = s->acc << 1) // Length in unary
		len = len + 1;

	uint16_t v = (uint16_t)(s->acc >> (64 - (len + 1))) - 1; // Mark + value in binary
	s->acc = s->acc << (len + 1);
	s->acc_usage = s->acc_usage - (len * 2 + 1);

	// RLE?
	if (bypass_rle == false)
	{
		if (v == s->prev)
			s->rle_len = sEliasDecodeU(s, true, input_size, input);

		s->prev = v;
	}

	// Bye!
	s->values_read = s->values_read + 1;
	return v;
}


static inline uint16_t sZigZagEncode(int16_t in)
{
	// https://developers.google.com/protocol-buffers/docs/encoding#signed_integers
	return (uint16_t)((in << 1) ^ (in >> 15));
}

static inline int16_t sZigZagDecode(uint16_t in)
{
	return (int16_t)((in >> 1) ^ (~(in & 1) + 1));
}


size_t EntropyCompress(size_t input_length, const int16_t* in, void* out)
{
	struct EliasEncodeState s = {0};

	// Pre-pass with NULL as output, this returns the final compressed size
	for (size_t i = 0; i < input_length; i++)
		sEliasEncodeU(&s, false, sZigZagEncode(in[i]), NULL);

	sEliasEncodeStop(&s, NULL);
	if (out == NULL)
		return s.bytes_write;

	// Actual compression
	memset(&s, 0, sizeof(struct EliasEncodeState));

	for (size_t i = 0; i < input_length; i++)
		sEliasEncodeU(&s, false, sZigZagEncode(in[i]), out);

	sEliasEncodeStop(&s, out);
	return s.bytes_write;
}


void EntropyDecompress(size_t input_size, size_t output_length, const void* in, int16_t* out)
{
	// FIXME, all the functions here are horribly insecure!!!
	struct EliasDecodeState s = {0};

	for (size_t i = 0; i < output_length; i++)
	{
		int16_t value = sZigZagDecode(sEliasDecodeU(&s, false, input_size, in));

		if (s.end == false)
			out[i] = value;
	}
}
