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
	// Accumulator
	size_t acc_usage;
	uint64_t acc; // Kinda, double of the worst case code (33 bits)

	// Output
	size_t output_offset;

	// Stats
	size_t values_write;
	size_t bytes_write;
};

struct EliasDecodeState
{
	// Accumulator
	size_t acc_usage;
	uint64_t acc;

	// Input
	size_t input_offset;

	// Stats
	size_t values_read;
	size_t bytes_read;
	bool eof;
	bool end;
};


static void sEmitByte(struct EliasEncodeState* s, uint8_t byte, uint8_t* output)
{
	s->acc_usage = s->acc_usage - 8;
	s->bytes_write = s->bytes_write + 1;

	if (output != NULL)
		output[s->output_offset] = byte;

	s->output_offset = s->output_offset + 1;
}


static void sEliasEncodeStop(struct EliasEncodeState* s, uint8_t* output)
{
	// Can we output a byte?
	while (s->acc_usage >= 8)
	{
		uint8_t byte = (uint8_t)(s->acc >> (s->acc_usage - 8));
		sEmitByte(s, byte, output);
	}

	// Remainder (less than a byte)
	if (s->acc_usage != 0)
	{
		uint8_t byte = (uint8_t)(s->acc << (8 - s->acc_usage));
		sEmitByte(s, byte, output);
		s->acc_usage = 0;
	}

	// Zero byte
	sEmitByte(s, 0, output);
}


static void sEliasEncodeU(struct EliasEncodeState* s, uint16_t v, uint8_t* output)
{
	assert(v != UINT16_MAX);
	v = v + 1;

	// Can we output a byte?
	while (s->acc_usage >= 8)
	{
		uint8_t byte = (uint8_t)(s->acc >> (s->acc_usage - 8));
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


static uint16_t sEliasDecodeU(struct EliasDecodeState* s, size_t input_size, const uint8_t* input)
{
	// Can we input?
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

	uint16_t v = (uint16_t)(s->acc >> (64 - (len + 1))); // Mark + value in binary
	s->acc = s->acc << (len + 1);

	// Bye!
	s->acc_usage = s->acc_usage - (len * 2 + 1);
	s->values_read = s->values_read + 1;
	return v - 1;
}


static uint16_t sZigZagEncode(int16_t in)
{
	// https://developers.google.com/protocol-buffers/docs/encoding#signed_integers
	return (uint16_t)((in << 1) ^ (in >> 15));
}

static int16_t sZigZagDecode(uint16_t in)
{
	return (int16_t)((in >> 1) ^ (~(in & 1) + 1));
}


size_t EntropyCompress(size_t input_length, void** aux_buffer, int16_t* inout)
{
	// TODO, the 'inout' style of API is ugly :(
	// (remainder of the early days of the project)

	struct EliasEncodeState s = {0};

	// Pre-pass with NULL as output, this return the final compressed size
	for (size_t i = 0; i < input_length; i++)
		sEliasEncodeU(&s, sZigZagEncode(inout[i]), NULL);

	sEliasEncodeStop(&s, NULL);

	// Realloc 'aux_buffer'
	*aux_buffer = realloc(*aux_buffer, s.bytes_write);
	assert(*aux_buffer != NULL);

	// Actual compression into 'aux_buffer'
	memset(&s, 0, sizeof(struct EliasEncodeState));

	for (size_t i = 0; i < input_length; i++)
		sEliasEncodeU(&s, sZigZagEncode(inout[i]), *aux_buffer);

	sEliasEncodeStop(&s, *aux_buffer);

	// Bye!, copy back to 'inout'
	memcpy(inout, *aux_buffer, s.bytes_write);
	return s.bytes_write;
}


void EntropyDecompress(size_t input_size, size_t output_length, const void* in, int16_t* out)
{
	struct EliasDecodeState s = {0};

	for (size_t i = 0; i < output_length; i++)
	{
		int16_t value = sZigZagDecode(sEliasDecodeU(&s, input_size, in));

		if (s.end == false)
			out[i] = value;
		else
			assert(1 == 0); // This shouldn't happen if 'output_length' is valid
	}
}
