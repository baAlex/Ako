/*

MIT License

Copyright (c) 2021-2023 Alexander Brandt

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

#include "ako-private.hpp"

namespace ako
{

AnsBitWriter::AnsBitWriter(size_t output_length, uint32_t* output)
{
	this->Reset(output_length, output);
}

void AnsBitWriter::Reset(size_t output_length, uint32_t* output)
{
	this->output_start = output;
	this->output_end = this->output_start + output_length;
	this->output = this->output_start;

	this->wrote_values = 0;

	this->accumulator = 0;
	this->accumulator_usage = 0;
}

int AnsBitWriter::Write(uint32_t value, uint32_t length)
{
	// Accumulator has space, ideal fast path
	if (this->accumulator_usage + length < ACCUMULATOR_LEN)
	{
		const auto mask = ~(0xFFFFFFFF << length);

		this->accumulator |= ((value & mask) << this->accumulator_usage);
		this->accumulator_usage += length;
	}

	// No space in accumulator, ultra super duper slow path
	else
	{
		if (this->output + 1 > this->output_end || length >= ACCUMULATOR_LEN)
			return 1;

		// Accumulate what we can, then write
		const auto min = ACCUMULATOR_LEN - this->accumulator_usage;
		*this->output++ = this->accumulator | (value << this->accumulator_usage);

		// Accumulate remainder
		const auto mask = ~(0xFFFFFFFF << (length - min));

		this->accumulator = (value >> min) & mask;
		this->accumulator_usage = length - min;

		// Developers, developers, developers
		// printf("\tE | \tWrite accumulator [d: 0x%x, min: %u]\n", *(this->output - 1), min);
	}

	// Developers, developers, developers
	// printf("\tE | v: %u,\tl: %u, u: %u\n", value, length, this->accumulator_usage);

	// Bye!
	this->wrote_values += 1;
	return 0;
}

size_t AnsBitWriter::Finish()
{
	// Remainder
	if (this->accumulator_usage > 0)
	{
		if (this->output + 1 > this->output_end)
			return 0;

		*this->output++ = this->accumulator;

		// Developers, developers, developers
		// printf("\tE | \tWrite accumulator [d: 0x%x]\n", *(this->output - 1));
	}

	// Bye!
	auto output_length = static_cast<size_t>(this->output - this->output_start);

	if (output_length == 0 && this->wrote_values != 0) // Somebody wrote lots of zeros
		output_length = 1;

	// printf("\tEncoded length: %zu\n\n", output_length); // Developers, developers, developers
	return output_length;
}


static uint8_t sCode(uint16_t value)
{
	if (value < 64)
		return static_cast<uint8_t>(value);

	value -= 48;

	int e = 0;
	while (value >= (1 << (e + 1)))
		e += 1;

	const auto base = (1 << e);
	const auto m = ((value - base) >> (e - 4));
	return static_cast<uint8_t>((e << 4) | m);
}

static uint16_t sRoot(uint8_t code)
{
	const auto e = code >> 4;
	const auto m = code & 15;

	if (e < 4)
		return static_cast<uint16_t>((e << 4) + m);

	const auto base = (1 << e) + 48;
	const auto add = m << (e - 4);
	return static_cast<uint16_t>(base + add);
}


#define ANS_YE_OLDE_OVERFLOW_ERROR 0
// 'Old' since my first Ans implementation ('resources/research/ans/ans2-normalization.cpp')
// had the same issue. I finally fix it here... I think.

struct BitsToWrite
{
	uint16_t v;
	uint16_t l;
};

size_t AnsEncode(uint32_t length, const uint16_t* input, uint32_t* output)
{
	AnsBitWriter writer(length + 1, output); // FIXME

#if ANS_YE_OLDE_OVERFLOW_ERROR == 0
	uint32_t state = ANS_INITIAL_STATE;
#else
	uint64_t state = ANS_INITIAL_STATE;
#endif

	const auto BUFFER_LEN = 65536;

	BitsToWrite buffer[BUFFER_LEN];
	unsigned buffer_cursor = 0;

	if (length > BUFFER_LEN)
		return 0;

	// Iterate input
	for (uint32_t i = length - 1; i < length; i -= 1) // Underflows, Ans operates in reverse
	{
		// Find root Cdf entry
		CdfEntry e = g_cdf1[255];
		{
			const auto code = sCode(input[i]);
			const auto root = sRoot(code); // Cdf is decoder-centric, encoding
			                               // requires extra steps

			for (uint32_t u = 0; u < G_CDF1_LEN; u += 1)
			{
				if (g_cdf1[u].root == root)
				{
					e = g_cdf1[u];
					break;
				}
			}
		}

		// Normalize state (make space)
		while (1) // while (C(state, e) > ANS_L * ANS_B - 1)
		{
			// Paper's method is to update the state and check if the
			// result is larger than a threshold, problem is that such
			// procedure requires a large state (something like u64)
			// Of course, the paper also suggest to use B as '1 << 8',
			// so yeah, I'm making the problems here

			if ((state / e.frequency) > (1 << (ANS_STATE_LEN - ANS_M_LEN)) - 1)
				goto do_it;

#if ANS_YE_OLDE_OVERFLOW_ERROR == 1
			// Printf debugging (btw, it works criminally well >:D )
			if (((state / e.frequency) << ANS_M_LEN) + (state % e.frequency) + e.cumulative > 0xFFFFFFFF)
				printf("N Wrap4\n");
			if (((state / e.frequency) << ANS_M_LEN) + (state % e.frequency) > 0xFFFFFFFF)
				printf("N Wrap3\n");
			if (((state / e.frequency) << ANS_M_LEN) > 0xFFFFFFFF)
				printf("N Wrap2\n");
			if ((state / e.frequency) > 0xFFFFFFFF)
				printf("N Wrap1\n");
#endif

			// Check
			if (((state / e.frequency) << ANS_M_LEN) + (state % e.frequency) + e.cumulative <= ANS_L * ANS_B - 1)
				break;

		do_it:
			// Normalize
			const auto b_word = static_cast<uint16_t>(state & ANS_B_MASK);
			// printf("\tE | %u\n", b_word); // Developers, developers, developers

			state = state >> ANS_B_LEN;

			if (buffer_cursor == BUFFER_LEN)
				return 0;

			buffer[buffer_cursor].v = b_word;
			buffer[buffer_cursor].l = ANS_B_LEN;
			buffer_cursor += 1;
		}

		// Write root, ans-coded in state
		{
#if ANS_YE_OLDE_OVERFLOW_ERROR == 1
			// Printf debugging
			// If something here wraps means that normalization was wrong
			if (((state / e.frequency) << ANS_M_LEN) + (state % e.frequency) + e.cumulative > 0xFFFFFFFF)
				printf("E Wrap4\n");
			if (((state / e.frequency) << ANS_M_LEN) + (state % e.frequency) > 0xFFFFFFFF)
				printf("E Wrap3\n");
			if (((state / e.frequency) << ANS_M_LEN) > 0xFFFFFFFF)
				printf("E Wrap2\n");
			if ((state / e.frequency) > 0xFFFFFFFF)
				printf("E Wrap1\n");
#endif

			// State update
			state = ((state / e.frequency) << ANS_M_LEN) + (state % e.frequency) + e.cumulative;
		}

		// Write suffix, raw in bitstream
		if (buffer_cursor == BUFFER_LEN)
			return 0;

		buffer[buffer_cursor].v = input[i] - e.root;
		buffer[buffer_cursor].l = e.suffix_length;
		buffer_cursor += 1;

		// Developers, developers, developers
		// printf("\tE | 0x%x\t<- Value: %u (r: %u, sl: %u, f: %u, c: %u)\n", state, input[i], e.root, e.suffix_length,
		//       e.frequency, e.cumulative);
	}

	// Normalize remainder
	while (state != 0)
	{
		const auto b_word = static_cast<uint16_t>(state & ANS_B_MASK);
		// printf("\tE | %u\n", b_word);

		state = state >> ANS_B_LEN;

		buffer[buffer_cursor].v = b_word;
		buffer[buffer_cursor].l = ANS_B_LEN;
		buffer_cursor += 1;
	}

	// Write bits in reverse order
	while (buffer_cursor != 0)
	{
		buffer_cursor -= 1;
		if (writer.Write(buffer[buffer_cursor].v, buffer[buffer_cursor].l) != 0)
			return 0;
	}

	// Bye!
	return writer.Finish();
}

} // namespace ako
