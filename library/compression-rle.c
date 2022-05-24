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
#define DEV_MAGIC_NUMBER 0


enum RleEncoderMode
{
	MODE_LITERAL,
	MODE_RLE
};

struct RleEncoderState
{
	ucoeff_t* out;
	ucoeff_t* out_end;

	const ucoeff_t* in_cursor1; // Always behind cursor 2
	const ucoeff_t* in_cursor2;

	enum RleEncoderMode mode;
};


static inline int sSwitchToRle(size_t blocks_len, struct RleEncoderState* s)
{
	ucoeff_t last_literal_len;

	// Emit literal
	const ucoeff_t* temp = s->in_cursor1;
	do
	{
		// Process blocks of 'blocks_len' or less
		// aka: Overflow mechanism
		temp += blocks_len;
		if (temp > s->in_cursor2)
			temp = s->in_cursor2;

		// Emit
		const ucoeff_t length = (ucoeff_t)(temp - s->in_cursor1); // TODO: check
		last_literal_len = length;

		if (s->out + length >= s->out_end)
			return 1;

		*(s->out++) = (ucoeff_t)(length) + DEV_MAGIC_NUMBER; // Length

		do
		{
			*(s->out++) = *(s->in_cursor1++); // Data
		} while (s->in_cursor1 != temp);

	} while (temp != s->in_cursor2);

	// Emit zero, if needed
	if (last_literal_len == blocks_len)
	{
		// In normal operation the decoder continuously switch between literal and rle modes.
		// Only exception being when a literal block of length 'blocks_len' appears, if so,
		// it will assume that the following block is also a literal.

		// To break this behaviour a literal of length zero do the work.

		if (s->out == s->out_end)
			return 1;

		*(s->out++) = 0 + DEV_MAGIC_NUMBER; // Zero
	}

	// Set rle mode
	s->mode = MODE_RLE;

	return 0;
}


static inline int sSwitchToLiteral(size_t blocks_len, struct RleEncoderState* s)
{
	// Emit rle, with same caveats as before, and solution too
	const ucoeff_t* temp;
	do
	{
		temp = s->in_cursor1 + blocks_len + 1; // Note this +1. We do not use rle sequences
		if (temp > s->in_cursor2)              // of length zero, so we start from one
			temp = s->in_cursor2;

		if (s->out == s->out_end)
			return 1;

		*(s->out++) = (ucoeff_t)(temp - s->in_cursor1 - 1) + DEV_MAGIC_NUMBER; // Length. Note the -1, TODO: check

		s->in_cursor1 += blocks_len + 1; // :)
		if (s->in_cursor1 < s->in_cursor2)
		{
			if (s->out == s->out_end)
				return 1;

			*(s->out++) = 0 + DEV_MAGIC_NUMBER; // Zero
		}

	} while (temp != s->in_cursor2);

	// Set literal mode
	s->in_cursor1 = s->in_cursor2;
	s->mode = MODE_LITERAL;

	return 0;
}


size_t akoRleEncode(size_t blocks_len, size_t trigger_delay, size_t input_size, size_t output_size,
                    const ucoeff_t* input, void* output)
{
	if ((input_size % (sizeof(ucoeff_t))) != 0 || input_size < sizeof(ucoeff_t) ||
	    (output_size % (sizeof(ucoeff_t))) != 0 || output_size < sizeof(ucoeff_t))
		return 0;

	struct RleEncoderState s = {
	    .out = output,
	    .out_end = (ucoeff_t*)output + (output_size / sizeof(ucoeff_t)),
	    .in_cursor1 = input,
	    .in_cursor2 = input,
	    .mode = MODE_LITERAL,
	};

	const ucoeff_t* in_end = input + (input_size / sizeof(ucoeff_t));
	ucoeff_t previous_value = 0;
	size_t consecutives = 0;

	if (*s.in_cursor2 == previous_value) // Always start with a literal
		s.in_cursor2++;                  // Magic trick to force conditionals below

	// Main loop
	do
	{
		if (*s.in_cursor2 == previous_value)
		{
			consecutives++;

			if (s.mode == MODE_LITERAL && consecutives > trigger_delay)
			{
				if (sSwitchToRle(blocks_len, &s) != 0)
					return 0;

				consecutives = 0;
			}
		}
		else
		{
			if (s.mode == MODE_RLE)
			{
				if (sSwitchToLiteral(blocks_len, &s) != 0)
					return 0;
			}

			previous_value = *s.in_cursor2;
		}

	} while (++s.in_cursor2 != in_end);

	// Remainder
	if (s.mode == MODE_LITERAL)
	{
		if (sSwitchToRle(blocks_len, &s) != 0)
			return 0;
	}
	else
	{
		if (sSwitchToLiteral(blocks_len, &s) != 0)
			return 0;
	}

	// Bye!
	return (size_t)(s.out - (ucoeff_t*)output) * sizeof(ucoeff_t);
}


size_t akoRleDecode(size_t blocks_len, size_t input_size, size_t output_size, const void* input, ucoeff_t* output)
{
	if ((input_size % (sizeof(ucoeff_t))) != 0 || input_size < sizeof(ucoeff_t) ||
	    (output_size % (sizeof(ucoeff_t))) != 0 || output_size < sizeof(ucoeff_t))
		return 0;

	const ucoeff_t* in = (const ucoeff_t*)input;
	const ucoeff_t* in_end = (const ucoeff_t*)input + (input_size / sizeof(ucoeff_t));

	ucoeff_t* out = output;
	ucoeff_t* out_end = out + (output_size / sizeof(ucoeff_t));

	ucoeff_t last;
	ucoeff_t length;
	size_t do_rle = 1;

	do
	{
		// Literal
		length = *in - DEV_MAGIC_NUMBER;
		if (out + length > out_end || in + length >= in_end)
			return 0;

		if (length != 0)
		{
			do_rle = (size_t)length - blocks_len; // do_rle = (length == blocks_len) ? 0 : 1;

			do
			{
				*(out++) = *(++in);
			} while (--length != 0);
			last = *in;
		}
		else
		{
			do_rle = 1;
		}

		// Rle
		if (do_rle != 0)
		{
			if (++in == in_end)
				break;

			length = *in + 1 - DEV_MAGIC_NUMBER;
			if (out + length > out_end)
				return 0;

			do
			{
				*(out++) = last;
			} while (--length != 0);
		}

	} while (++in != in_end);

	return (size_t)(out - output) * sizeof(ucoeff_t);
}


//-----------------------------


#define OLD_TRIGGER_LEN 2


static inline void sOldRawWriteRle(ucoeff_t consecutive_no, ucoeff_t** out)
{
	**out = (ucoeff_t)(consecutive_no - OLD_TRIGGER_LEN);
	*out = *out + 1;
}

static inline void sOldRawWriteValue(ucoeff_t value, ucoeff_t** out)
{
	**out = value;
	*out = *out + 1;
}

static inline void sOldRawWriteMultipleValues(ucoeff_t value, ucoeff_t rle_len, ucoeff_t** out)
{
	for (ucoeff_t u = 0; u < rle_len; u++)
	{
		**out = value;
		*out = *out + 1;
	}
}


size_t akoOldRleEncode(size_t blocks_len, size_t trigger_delay, size_t input_size, size_t output_size,
                       const ucoeff_t* input, void* output)
{
	(void)blocks_len;    // Not a thing in this encoder
	(void)trigger_delay; // Hardcoded as both encoder/decoder use it

	const ucoeff_t* in_end = (const ucoeff_t*)((const uint8_t*)input + input_size);
	const ucoeff_t* in = input;

	const ucoeff_t* out_end = (const ucoeff_t*)((const uint8_t*)output + output_size);
	ucoeff_t* out = output;

	ucoeff_t consecutive_no = 0;
	ucoeff_t previous_value = 0;

	if (output_size == 0 || input_size == 0)
		return 0;
	if ((input_size % 2) != 0)
		return 0;

	// First value
	sOldRawWriteValue(*in, &out);
	previous_value = *in;
	in++;

	// All others
	for (; in < in_end; in++)
	{
		if (out == out_end) // We have space?
			return 0;

		if (*in == previous_value)
		{
			consecutive_no++;

			if (consecutive_no <= OLD_TRIGGER_LEN)
				sOldRawWriteValue(*in, &out);
			else if (consecutive_no == 65535) // Oh no, at this rate we are going to overflow!
			{
				sOldRawWriteRle(consecutive_no, &out);
				consecutive_no = 0;
			}
		}
		else
		{
			if (consecutive_no >= OLD_TRIGGER_LEN)
				sOldRawWriteRle(consecutive_no, &out);

			sOldRawWriteValue(*in, &out);
			previous_value = *in;
			consecutive_no = 0;
		}
	}

	// Maybe the loop finished with a pending Rle length to emit
	if (consecutive_no >= OLD_TRIGGER_LEN)
	{
		if (out == out_end)
			return 0;

		sOldRawWriteRle(consecutive_no, &out);
	}

	// Bye!
	return (size_t)((uint8_t*)out - (uint8_t*)output);
}


size_t akoOldRleDecode(size_t blocks_len, size_t input_size, size_t output_size, const void* input, ucoeff_t* output)
{
	(void)blocks_len; // No configurable in this encoder

	const ucoeff_t* in_end = (const ucoeff_t*)((const uint8_t*)input + input_size);
	const ucoeff_t* in = (const ucoeff_t*)input;

	const ucoeff_t* out_end = (const ucoeff_t*)((const uint8_t*)output + output_size);
	ucoeff_t* out = output;

	ucoeff_t consecutive_no = 0;
	ucoeff_t previous_value = 0;

	if (output_size == 0 || input_size == 0)
		return 0;
	if ((output_size % 2) != 0)
		return 0;

	// First value
	sOldRawWriteValue(*in, &out);
	previous_value = *in;
	in++;

	// All others
	for (; in < in_end; in++)
	{
		if (out == out_end)
			return 0;

		if (*in == previous_value)
		{
			sOldRawWriteValue(*in, &out);
			consecutive_no++;

			if (consecutive_no == OLD_TRIGGER_LEN)
			{
				if (++in == in_end)
					return 0;

				const ucoeff_t rle_len = *in;
				if ((out + (size_t)rle_len) > out_end)
					return 0;

				sOldRawWriteMultipleValues(previous_value, rle_len, &out);
				consecutive_no = 0;
			}
		}
		else
		{
			sOldRawWriteValue(*in, &out);
			previous_value = *in;
			consecutive_no = 0;
		}
	}

	// Bye!
	return (size_t)((uint8_t*)out - (uint8_t*)output);
}
