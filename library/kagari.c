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


// FIXME, nothing here will work on big-endian machines
// =====


#define RLE_TRIGGER_LEN 2 // Number of identical consecutive values to activate RLE


struct akoEliasStateE akoEliasEncodeSet(void* buffer, size_t size)
{
	struct akoEliasStateE e;

	e.out_end = ((uint8_t*)buffer + size);
	e.out_cursor = buffer;
	e.accumulator = 0;
	e.accumulator_usage = 0;

	return e;
}

struct akoEliasStateD akoEliasDecodeSet(const void* buffer, size_t size)
{
	struct akoEliasStateD d;

	d.input_end = ((uint8_t*)buffer + size);
	d.input_cursor = buffer;
	d.accumulator = 0;
	d.accumulator_usage = 0;

	return d;
}


static inline int sBitsLen(uint16_t v)
{
	int len = 0;
	for (; v > 1; v >>= 1)
		len++;

	return len;
}

int akoEliasEncodeStep(struct akoEliasStateE* s, uint16_t v)
{
	const int binary_bits = sBitsLen(v);
	const int total_bits = binary_bits * 2 + 1;

	// Make space
	if (s->accumulator_usage + total_bits > AKO_ELIAS_ACCUMULATOR_LEN)
	{
		if (s->out_cursor + (s->accumulator_usage / 8) >= s->out_end)
			return 0;

		do
		{
			s->accumulator_usage -= 8;
			*s->out_cursor++ = (uint8_t)((s->accumulator >> s->accumulator_usage) & 0xFF);
			// printf("E\t0x%02X\n", (uint8_t)((s->accumulator >> s->accumulator_usage) & 0xFF));
		} while (s->accumulator_usage + total_bits > AKO_ELIAS_ACCUMULATOR_LEN);
	}

	s->accumulator_usage += total_bits;

	// Encode
	s->accumulator <<= total_bits; // Unary part
	s->accumulator |= (uint64_t)v; // Binary part (implicit stop mark)

	// Bye!
	return total_bits;
}


size_t akoEliasEncodeEnd(struct akoEliasStateE* s, void* out_start)
{
	// Empty accumulator
	while (s->accumulator_usage / 8 != 0)
	{
		if (s->out_cursor + 1 >= s->out_end)
			return 0;

		s->accumulator_usage -= 8;
		*s->out_cursor++ = (uint8_t)((s->accumulator >> s->accumulator_usage) & 0xFF);
		// printf("E\t0x%02X\n", (uint8_t)((s->accumulator >> s->accumulator_usage) & 0xFF));
	}

	if (s->accumulator_usage != 0)
	{
		if (s->out_cursor + 1 >= s->out_end)
			return 0;

		*s->out_cursor++ = (uint8_t)((s->accumulator << (8 - s->accumulator_usage)) & 0xFF);
		// printf("E\t0x%02X\n", (uint8_t)((s->accumulator << (8 - s->accumulator_usage)) & 0xFF));
	}

	// Bye!
	return (size_t)((uint8_t*)s->out_cursor - (uint8_t*)out_start);
}


static inline int sLeadingZeros(uint64_t v)
{
	int len = 0;
	for (; (v & (1L << 63)) == 0; v <<= 1)
		len++;

	return len;
}

int akoEliasDecodeStep(struct akoEliasStateD* s, uint16_t* v_out)
{
	// Fill accumulator
	if (s->accumulator_usage < (AKO_ELIAS_ACCUMULATOR_LEN - 32))
	{
		if (s->input_cursor + ((AKO_ELIAS_ACCUMULATOR_LEN - s->accumulator_usage) / 8) < s->input_end)
		{
			do
			{
				s->accumulator_usage += 8;
				s->accumulator |= (uint64_t)(*s->input_cursor++) << (AKO_ELIAS_ACCUMULATOR_LEN - s->accumulator_usage);
			} while (s->accumulator_usage < (AKO_ELIAS_ACCUMULATOR_LEN - 8));
		}
		else
		{
			// We are near the input end, read with care
			while (s->accumulator_usage < (AKO_ELIAS_ACCUMULATOR_LEN - 8) && s->input_cursor < s->input_end)
			{
				s->accumulator_usage += 8;
				s->accumulator |= (uint64_t)(*s->input_cursor++) << (AKO_ELIAS_ACCUMULATOR_LEN - s->accumulator_usage);
			}
		}
	}

	// Decode
	if (s->accumulator == 0)
		return 0;

	const int unary_bits = sLeadingZeros(s->accumulator);
	const int total_bits = unary_bits * 2 + 1;

	if (total_bits > s->accumulator_usage)
		return 0;

	*v_out = (uint16_t)(s->accumulator >> (AKO_ELIAS_ACCUMULATOR_LEN - total_bits));

	s->accumulator <<= total_bits;
	s->accumulator_usage -= total_bits;

	// Bye!
	return total_bits;
}


//


static inline uint16_t sZigZagEncode(int16_t in)
{
	// https://developers.google.com/protocol-buffers/docs/encoding#signed_integers
	return (uint16_t)((in << 1) ^ (in >> 15));
}

static inline int16_t sZigZagDecode(uint16_t in)
{
	return (int16_t)((in >> 1) ^ (~(in & 1) + 1));
}


//


static void sRawWriteValue(int16_t value, int16_t** out)
{
	**out = value;
	*out = *out + 1;
}

static void sRawWriteMultipleValues(int16_t value, uint16_t rle_len, int16_t** out)
{
	for (uint16_t u = 0; u < rle_len; u++)
	{
		**out = value;
		*out = *out + 1;
	}
}


static int sEncodeRle(struct akoEliasStateE* e, uint16_t consecutive_no)
{
	return akoEliasEncodeStep(e, (uint16_t)(consecutive_no - RLE_TRIGGER_LEN + 1)); // +1 as elias can't encode zero
}

static int sDecodeRle(struct akoEliasStateD* d, uint16_t* out_rle)
{
	uint16_t rle = 0;
	const int bits = akoEliasDecodeStep(d, &rle);
	*out_rle = rle - 1; // -1 to compensate that elias can't encode zero

	return bits;
}


static int sEncodeValue(struct akoEliasStateE* e, int16_t value)
{
	const uint16_t v = sZigZagEncode(value) + 1;
	return akoEliasEncodeStep(e, v);
}

static int sDecodeValue(struct akoEliasStateD* d, int16_t* out_value)
{
	uint16_t v = 0;
	const int bits = akoEliasDecodeStep(d, &v);

	*out_value = sZigZagDecode((v - 1));
	return bits;
}


size_t akoKagariEncode(const void* input, size_t input_size, void* output, size_t output_size)
{
	struct akoEliasStateE e = akoEliasEncodeSet(output, output_size);

	const int16_t* input_end = (const int16_t*)((const uint8_t*)input + input_size);
	const int16_t* in = (const int16_t*)input;

	uint16_t consecutive_no = 0;
	int16_t previous_value = 0;

	if (output_size == 0 || input_size == 0)
		return 0;
	if ((output_size % 2) != 0 || (input_size % 2) != 0)
		return 0;

	// First value
	if (sEncodeValue(&e, *in) == 0)
		return 0;

	previous_value = *in;
	in++;

	// All others
	for (; in < input_end; in++)
	{
		if (*in == previous_value)
		{
			consecutive_no++;

			if (consecutive_no <= RLE_TRIGGER_LEN)
			{
				if (sEncodeValue(&e, *in) == 0)
					return 0;
			}
			else if (consecutive_no == AKO_ELIAS_MAX - 1) // Oh no, at this rate we are going to overflow!
			{
				if (sEncodeRle(&e, consecutive_no) == 0)
					return 0;

				consecutive_no = 0;
			}
		}
		else
		{
			if (consecutive_no >= RLE_TRIGGER_LEN)
			{
				if (sEncodeRle(&e, consecutive_no) == 0)
					return 0;
			}

			if (sEncodeValue(&e, *in) == 0)
				return 0;

			previous_value = *in;
			consecutive_no = 0;
		}
	}

	// Maybe the loop finished with a pending Rle length to emit
	if (consecutive_no >= RLE_TRIGGER_LEN)
	{
		if (sEncodeRle(&e, consecutive_no) == 0)
			return 0;
	}

	// Bye!
	return akoEliasEncodeEnd(&e, output);
}


size_t akoKagariDecode(size_t no, const void* input, size_t input_size, void* output, size_t output_size)
{
	struct akoEliasStateD d = akoEliasDecodeSet(input, input_size);

	const int16_t* out_end = (const int16_t*)((const uint8_t*)output + output_size);
	int16_t* out = output;

	uint16_t consecutive_no = 0;
	int16_t previous_value = 0;
	int16_t decoded_v = 0;

	if (output_size == 0 || input_size == 0 || no == 0)
		return 0;
	if ((output_size % 2) != 0 || (input_size % 2) != 0)
		return 0;

	// First value
	if (sDecodeValue(&d, &decoded_v) == 0)
		return 0;

	sRawWriteValue(decoded_v, &out);
	previous_value = decoded_v;
	no--;

	// All others
	for (; no != 0; no--)
	{
		if (out == out_end)
			return 0;

		if (sDecodeValue(&d, &decoded_v) == 0)
			return 0;

		if (decoded_v == previous_value)
		{
			sRawWriteValue(decoded_v, &out);
			consecutive_no++;

			if (consecutive_no == RLE_TRIGGER_LEN)
			{
				if (sDecodeRle(&d, &consecutive_no) == 0)
					return 0;

				const uint16_t rle_len = consecutive_no;
				if ((out + (size_t)rle_len) > out_end)
					return 0;

				sRawWriteMultipleValues(previous_value, rle_len, &out);
				consecutive_no = 0;
				no -= rle_len;
			}
		}
		else
		{
			sRawWriteValue(decoded_v, &out);
			previous_value = decoded_v;
			consecutive_no = 0;
		}
	}

	// Bye!
	return (size_t)((uint8_t*)out - (uint8_t*)output);
}
