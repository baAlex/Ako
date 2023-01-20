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

static uint8_t sEncode(uint16_t value)
{
	if (value < 247)
		return static_cast<uint8_t>(value);

	int e = 0;
	while (value >= (1 << e))
		e += 1;

	return static_cast<uint8_t>(247 + e - 8);
}

static uint16_t sRoot(uint8_t code)
{
	if (code < 247)
		return static_cast<uint8_t>(code);

	return 0;
}

static uint16_t sSuffixLength(uint8_t code)
{
	if (code < 247)
		return 0;

	return static_cast<uint8_t>(code - 247 + 8);
}


AnsEncoder::AnsEncoder()
{
	m_queue = reinterpret_cast<QueueToWrite*>(std::malloc(sizeof(QueueToWrite) * QUEUE_LENGTH));
	m_queue_cursor = 0;
}

AnsEncoder::~AnsEncoder()
{
	std::free(m_queue);
}


uint32_t AnsEncoder::Encode(uint32_t input_length, const uint16_t* input)
{
	uint32_t state = ANS_INITIAL_STATE;
	uint32_t output_size = 0;

	// Reset
	m_queue_cursor = 0;

	// Iterate input
	for (uint32_t i = input_length - 1; i < input_length; i -= 1) // Underflows, Ans operates in reverse
	{
		// Find root Cdf entry
		auto e = g_cdf1[255];
		{
			const auto code = sEncode(input[i]);
			const auto root = sRoot(code);       // Cdf is decoder-centric, encoding
			const auto sl = sSuffixLength(code); // requires extra steps

			for (uint32_t u = 0; u < G_CDF1_LEN; u += 1)
			{
				if (g_cdf1[u].root == root && g_cdf1[u].suffix_length == sl)
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

			// Check
			if (((state / e.frequency) << ANS_M_LEN) + (state % e.frequency) + e.cumulative <= ANS_L * ANS_B - 1)
				break;

		proceed_with_normalization_as_check_will_wrap:
			// Extract bits and normalize state
			const auto bits = static_cast<uint16_t>(state & ANS_B_MASK);
			state = state >> ANS_B_LEN;

			// Queue bits
			if (m_queue_cursor == QUEUE_LENGTH)
				return 0;

			m_queue[m_queue_cursor].v = bits;
			m_queue[m_queue_cursor].l = ANS_B_LEN;
			output_size += ANS_B_LEN;
			m_queue_cursor += 1;

			// Developers, developers, developers
			// printf("\tE | %u\n", bits);
		}

		// Encode root, ans-coded in state
		state = ((state / e.frequency) << ANS_M_LEN) + (state % e.frequency) + e.cumulative;

		// Encode suffix, raw in bitstream
		if (e.suffix_length != 0)
		{
			if (m_queue_cursor == QUEUE_LENGTH)
				return 0;

			m_queue[m_queue_cursor].v = input[i] - e.root;
			m_queue[m_queue_cursor].l = e.suffix_length;
			output_size += e.suffix_length;
			m_queue_cursor += 1;
		}

		// Developers, developers, developers
		// printf("\tE | 0x%x\t<- Value: %u (r: %u, sl: %u, f: %u, c: %u)\n", state, input[i], e.root,
		// e.suffix_length,
		//       e.frequency, e.cumulative);
	}

	// Normalize remainder
	while (state != 0)
	{
		// Extract bits and normalize state
		const auto bits = static_cast<uint16_t>(state & ANS_B_MASK);
		state = state >> ANS_B_LEN;

		// Queue bits
		if (m_queue_cursor == QUEUE_LENGTH)
			return 0;

		m_queue[m_queue_cursor].v = bits;
		m_queue[m_queue_cursor].l = ANS_B_LEN;
		output_size += ANS_B_LEN;
		m_queue_cursor += 1;

		// Developers, developers, developers
		// printf("\tE | %u\n", bits);
	}

	// Bye!
	return output_size;
}


uint32_t AnsEncoder::Write(BitWriter* writer)
{
	// Write bits to output in reverse order
	if (writer != nullptr)
	{
		while (m_queue_cursor != 0)
		{
			m_queue_cursor -= 1;
			if (writer->Write(m_queue[m_queue_cursor].v, m_queue[m_queue_cursor].l) != 0)
				return 1;
		}
	}

	return 0;
}


} // namespace ako
