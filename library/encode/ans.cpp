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


// <FIXME>, stack overflow on Windows
const auto QUEUE_LEN = 65536 * 4; // x2 for both roots and suffixes (assuming one
                                  // normalization per value).
BitsToWrite queue[QUEUE_LEN];     // We can't call the bit writer directly as Ans operate in
// </FIXME>


uint32_t AnsEncode(uint32_t input_length, const uint16_t* input, BitWriter* writer, AnsEncoderStatus& out_status)
{
#if ANS_YE_OLDE_OVERFLOW_ERROR == 0
	uint32_t state = ANS_INITIAL_STATE;
#else
	uint64_t state = ANS_INITIAL_STATE;
#endif

	uint32_t output_length = 0;

	// We only encode inputs of 65536 values or less
	// (and I'm not checking that here to let the follow loop fail if that happens)

	// const auto QUEUE_LEN = 65536 * 4; // x2 for both roots and suffixes (assuming one normalization per value).

	// BitsToWrite queue[QUEUE_LEN]; // We can't call the bit writer directly as Ans operate in
	unsigned queue_cursor = 0; // reverse... to then write bits also in reverse... so is
	                           // better to queue the bits and write them later

	// Iterate input
	for (uint32_t i = input_length - 1; i < input_length; i -= 1) // Underflows, Ans operates in reverse
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
			// so yeah, I'm the cause of problems here.

			// However we can check wraps/overflows:
			if ((state / e.frequency) > (1 << (ANS_STATE_LEN - ANS_M_LEN)) - 1)
				goto proceed_with_normalization_as_check_will_wrap; // Gotos are bad ÒwÓ

#if ANS_YE_OLDE_OVERFLOW_ERROR == 1 // Printf debugging (it works criminally well >:D )
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

		proceed_with_normalization_as_check_will_wrap:
			// Extract bits and normalize state
			const auto bits = static_cast<uint16_t>(state & ANS_B_MASK);
			state = state >> ANS_B_LEN;

			// Queue bits
			if (queue_cursor == QUEUE_LEN)
				goto return_input_too_long;

			queue[queue_cursor].v = bits;
			queue[queue_cursor].l = ANS_B_LEN;
			queue_cursor += 1;

			// Developers, developers, developers
			// printf("\tE | %u\n", bits);
		}

		// Encode root, ans-coded in state
		state = ((state / e.frequency) << ANS_M_LEN) + (state % e.frequency) + e.cumulative;

		// Encode suffix, raw in bitstream
		if (queue_cursor == QUEUE_LEN)
			goto return_input_too_long;

		queue[queue_cursor].v = input[i] - e.root;
		queue[queue_cursor].l = e.suffix_length;
		queue_cursor += 1;

		// Developers, developers, developers
		// printf("\tE | 0x%x\t<- Value: %u (r: %u, sl: %u, f: %u, c: %u)\n", state, input[i], e.root, e.suffix_length,
		//       e.frequency, e.cumulative);
	}

	// Normalize remainder
	while (state != 0)
	{
		// Extract bits and normalize state
		const auto bits = static_cast<uint16_t>(state & ANS_B_MASK);
		state = state >> ANS_B_LEN;

		// Queue bits
		if (queue_cursor == QUEUE_LEN)
			goto return_input_too_long;

		queue[queue_cursor].v = bits;
		queue[queue_cursor].l = ANS_B_LEN;
		queue_cursor += 1;

		// Developers, developers, developers
		// printf("\tE | %u\n", bits);
	}

	// Write bits to output in reverse order
	if (writer != nullptr)
	{
		while (queue_cursor != 0)
		{
			queue_cursor -= 1;
			if (writer->Write(queue[queue_cursor].v, queue[queue_cursor].l) != 0)
				goto return_output_too_short;
		}

		// output_length = writer->Finish();
	}

	// ... No, we don't have a writer to write. But
	// we at least can return the output length
	// else
	{
		uint32_t accumulator = 0;
		while (queue_cursor != 0)
		{
			queue_cursor -= 1;
			accumulator += queue[queue_cursor].l;

			if (accumulator > 32)
			{
				output_length += 1;
				accumulator = 0;
			}
		}

		if (accumulator > BitWriter::ACCUMULATOR_LEN) // Mimic BitWriter behaviour
			output_length += 1;
	}

	// Bye!
	out_status = AnsEncoderStatus::Ok;
	return 1; // FIXME
return_input_too_long:
	out_status = AnsEncoderStatus::InputTooLong;
	return 0;
return_output_too_short:
	out_status = AnsEncoderStatus::OutputTooShort;
	return 0;
}

} // namespace ako
