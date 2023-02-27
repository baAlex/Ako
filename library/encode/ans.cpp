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

#define LOOKUP_TABLE
#ifdef LOOKUP_TABLE
static uint8_t s_lookup[255 + 1];
#endif


static size_t sQuicksortPartition(CdfEntry* cdf, const size_t lo, const size_t hi)
{
	const auto pivot = static_cast<size_t>(cdf[(hi + lo) >> 1].frequency);
	auto i = lo - 1;
	auto j = hi + 1;

	while (1)
	{
		// Move the left index to the right at least once and while the element at
		// the left index is less than the pivot
		do
		{
			i = i + 1;
		} while (cdf[i].frequency > pivot);

		// Move the right index to the left at least once and while the element at
		// the right index is greater than the pivot
		do
		{
			j = j - 1;
		} while (cdf[j].frequency < pivot);

		// If the indices crossed, return
		if (i >= j)
			break;

		// Swap the elements at the left and right indices
		const auto temp = cdf[i];
		cdf[i] = cdf[j];
		cdf[j] = temp;
	}

	return j;
}

static void sQuicksort(CdfEntry* cdf, const size_t lo, const size_t hi)
{
	if (lo < hi)
	{
		const auto p = sQuicksortPartition(cdf, lo, hi);
		sQuicksort(cdf, lo, p);
		sQuicksort(cdf, p + 1, hi);
	}
}


static uint8_t sCode(uint16_t value)
{
	if (value < 247)
		return static_cast<uint8_t>(value);

	auto e = 0;
	while (value >= (1 << e))
		e += 1;

	return static_cast<uint8_t>(247 + e - 8);
}

static uint8_t sRoot(uint8_t code)
{
	if (code < 247)
		return code;

	return 0;
}

static uint8_t sSuffixLength(uint8_t code)
{
	if (code < 247)
		return 0;

	return static_cast<uint8_t>(code - 247 + 8);
}


AnsEncoder::AnsEncoder()
{
	m_queue = reinterpret_cast<QueueToWrite*>(std::malloc(sizeof(QueueToWrite) * QUEUE_LENGTH));
}

AnsEncoder::~AnsEncoder()
{
	std::free(m_queue);
}


uint32_t AnsEncoder::Encode(uint32_t input_length, const uint16_t* input)
{
	uint32_t state = ANS_INITIAL_STATE;
	uint32_t output_size = 0;  // In bits
	uint32_t output_size2 = 0; // Ditto
	uint32_t m = 0;
	uint32_t m_exp = 0;

	uint8_t largest_code = 0;

	if (input_length < 1)
		return 0;

	// Reset
	m_cdf_len = 0;
	m_queue_cursor = 0;

	// Create Cdf
	{
		Memset(m_cdf, 0, sizeof(CdfEntry) * (255 + 1));

		// Count frequencies
		for (uint32_t i = input_length - 1; i < input_length; i -= 1) // Underflows, Ans operates in reverse
		{
			const auto code = sCode(input[i]);

			if (m_cdf[code].frequency < 0xFFFF) // Saturate
				m_cdf[code].frequency = static_cast<uint16_t>(m_cdf[code].frequency + 1);

			if (m_cdf[code].frequency == 1)
			{
				m_cdf[code].code = code;
				m_cdf[code].root = sRoot(code);
				m_cdf[code].suffix_length = sSuffixLength(code);

				m_cdf_len += 1;
				largest_code = Max(largest_code, code);
			}
		}

		// Sort them
		sQuicksort(m_cdf, 0, largest_code);

		// Normalize them
		m = Max(ANS_MIN_M, Min(NearPowerOfTwo(input_length), ANS_MAX_M));
		m_exp = Ctz(m);
		{
			const auto scale = static_cast<float>(m) / static_cast<float>(input_length);
			uint32_t summation = 0;
			uint32_t last_up_one = 0;

			for (uint32_t i = 0; i < m_cdf_len; i += 1)
			{
				m_cdf[i].frequency = static_cast<uint16_t>(std::ceil(static_cast<float>(m_cdf[i].frequency) * scale));

				if (m_cdf[i].frequency == 0)
					m_cdf[i].frequency = 1;
				if (m_cdf[i].frequency > 1)
					last_up_one = i;

				summation += m_cdf[i].frequency;
			}

			while (summation > m)
			{
				summation -= 1;

				if (m_cdf[last_up_one].frequency <= 1) // Fail
					return 0;

				m_cdf[last_up_one].frequency = static_cast<uint16_t>(m_cdf[last_up_one].frequency - 1);

				if (m_cdf[last_up_one].frequency == 1)
					last_up_one -= 1;
			}
		}

		// Accumulate them
		uint32_t summation = 0;
		uint32_t prev_f = m_cdf[0].frequency;
		for (uint32_t i = 0; i < m_cdf_len; i += 1)
		{
			// Accumulate
			m_cdf[i].cumulative = static_cast<uint16_t>(summation);
			summation += m_cdf[i].frequency;

			// Count Cdf size
			const auto f_to_transmit = (i != 0) ? (prev_f - m_cdf[i].frequency) : m_cdf[i].frequency;
			prev_f = m_cdf[i].frequency;

			output_size += BitWriter::RiceLength(m_cdf[i].code) + BitWriter::RiceLength(f_to_transmit);
			output_size2 += BitWriter::RiceLength(m_cdf[i].code) + BitWriter::RiceLength(f_to_transmit);

			// printf("! %u-%u-%u (size: %u)\n", m_cdf[i].root, m_cdf[i].frequency, f_to_transmit,
			//       BitWriter::RiceLength(m_cdf[i].root) + BitWriter::RiceLength(f_to_transmit));
		}

		// One more thing to count in ouput size
		output_size += BitWriter::RiceLength(m_cdf_len);
		output_size2 += BitWriter::RiceLength(m_cdf_len);

		// Developers, developers, developers
		// for (uint32_t i = 0; i < m_cdf_len; i += 1)
		//	printf("\tCdf %u: root: %u, sl: %u, f: %u, c: %u\n", i, m_cdf[i].root, m_cdf[i].suffix_length,
		//	       m_cdf[i].frequency, m_cdf[i].cumulative);
		// printf("\tCdf length: %u, m: %u\n", m_cdf_len, m);
	}

#ifdef LOOKUP_TABLE
	// Fill a look-up table
	{
		uint8_t code = largest_code;
		do
		{
			for (uint32_t u = 0; u < m_cdf_len; u += 1)
			{
				if (m_cdf[u].code == code)
				{
					s_lookup[code] = static_cast<uint8_t>(u);
					break;
				}
			}
		} while (code-- != 0);
	}
#endif

	// Encode
	for (uint32_t i = input_length - 1; i < input_length; i -= 1) // Underflows, Ans operates in reverse
	{
#ifdef LOOKUP_TABLE
		const auto e = m_cdf[s_lookup[sCode(input[i])]];
#else
		CdfEntry e;
		{
			const auto code = sCode(input[i]);
			for (uint32_t u = 0; u < m_cdf_len; u += 1)
			{
				if (m_cdf[u].code == code)
				{
					e = m_cdf[u];
					break;
				}
			}
		}
#endif

		// Normalize state (make space)
		while (1) // while (C(state, e) > ANS_L * ANS_B - 1)
		{
			// Paper's method is to update the state and check if the
			// result is larger than a threshold, problem is that such
			// procedure requires a large state (something like u64)

			// Of course, the paper also suggest to use B as '1 << 8',
			// so yeah, I'm the cause of problems here.

			// However we can check wraps/overflows:
			if ((state / e.frequency) > static_cast<uint32_t>((1 << (ANS_STATE_LEN - m_exp)) - 1))
				goto proceed_with_normalization_as_check_will_wrap; // Gotos are bad ÒwÓ

			// Check
			if (((state / e.frequency) << m_exp) + (state % e.frequency) + e.cumulative <= ANS_L * ANS_B - 1)
				break;

		proceed_with_normalization_as_check_will_wrap:
			// Extract bits and normalize state
			const auto bits = static_cast<uint16_t>(state & ANS_B_MASK);
			state = state >> ANS_B_EXP;

			// Queue bits
			if (m_queue_cursor == QUEUE_LENGTH)
				return 0;

			m_queue[m_queue_cursor].v = bits;
			m_queue[m_queue_cursor].l = ANS_B_EXP;
			output_size += ANS_B_EXP;
			m_queue_cursor += 1;
		}

		// Encode root, Ans-coded in state
		state = ((state / e.frequency) << m_exp) + (state % e.frequency) + e.cumulative;

		// Encode suffix, raw in bitstream
		if (e.suffix_length != 0)
		{
			if (m_queue_cursor == QUEUE_LENGTH)
				return 0;

			m_queue[m_queue_cursor].v = static_cast<uint16_t>(input[i] - e.root);
			m_queue[m_queue_cursor].l = e.suffix_length;
			output_size += e.suffix_length;
			m_queue_cursor += 1;
		}

		// Developers, developers, developers
		// printf("\tAns: %u -> %u\n", e.cumulative, state);
	}

	// Encode remainder
	while (state != 0)
	{
		// Developers, developers, developers
		// printf("\tAns: %u\n", state);

		// Extract bits and normalize state
		const auto bits = static_cast<uint16_t>(state & ANS_B_MASK);
		state = state >> ANS_B_EXP;

		// Queue bits
		if (m_queue_cursor == QUEUE_LENGTH)
			return 0;

		m_queue[m_queue_cursor].v = bits;
		m_queue[m_queue_cursor].l = ANS_B_EXP;
		output_size += ANS_B_EXP;
		m_queue_cursor += 1;
	}

	// Bye!
	// printf("\tAns encode size: %u bits (%u the Cdf)\n\n", output_size, output_size2);
	return output_size;
}


uint32_t AnsEncoder::Write(BitWriter* writer)
{
	uint32_t output_size = 0; // In bits

	if (m_cdf_len < 1)
		return 0;

	// Write Cdf
	if ((output_size += writer->WriteRice(m_cdf_len)) == 0)
		return 0;

	uint32_t prev_f = m_cdf[0].frequency;
	for (uint32_t i = 0; i < m_cdf_len; i += 1)
	{
		const auto f_to_transmit = (i != 0) ? (prev_f - m_cdf[i].frequency) : m_cdf[i].frequency;
		prev_f = m_cdf[i].frequency;

		if ((output_size += writer->WriteRice(m_cdf[i].code)) == 0)
			return 0;
		if ((output_size += writer->WriteRice(f_to_transmit)) == 0)
			return 0;
	}

	// Write bitstream (in reverse order)
	while (m_queue_cursor != 0)
	{
		m_queue_cursor -= 1;
		if ((output_size += writer->Write(m_queue[m_queue_cursor].v, m_queue[m_queue_cursor].l)) == 0)
			return 0;
	}

	return output_size;
}

} // namespace ako
