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

/*static uint8_t sEncode(uint16_t value)
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
}*/


AnsEncoder::AnsEncoder()
{
	m_queue = reinterpret_cast<QueueToWrite*>(std::malloc(sizeof(QueueToWrite) * QUEUE_LENGTH));
	m_queue_cursor = 0;

	m_cdf = reinterpret_cast<NewCdfEntry*>(std::malloc(sizeof(NewCdfEntry) * (65535 + 1)));
	m_cdf_len = 0;
}

AnsEncoder::~AnsEncoder()
{
	std::free(m_queue);
	std::free(m_cdf);
}


static size_t sQuicksortPartition(NewCdfEntry* cdf, const size_t lo, const size_t hi)
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

static void sQuicksort(NewCdfEntry* cdf, const size_t lo, const size_t hi)
{
	if (lo >= 0 && hi >= 0 && lo < hi)
	{
		const auto p = sQuicksortPartition(cdf, lo, hi);
		sQuicksort(cdf, lo, p);
		sQuicksort(cdf, p + 1, hi);
	}
}


uint32_t AnsEncoder::Encode(const CdfEntry* hardcoded_cdf, uint32_t input_length, const uint16_t* input)
{
	(void)hardcoded_cdf;

	uint32_t state = ANS_INITIAL_STATE;
	uint32_t output_size = 0;
	uint32_t m = 0;

	// Create Cdf
	{
		uint32_t unique_values = 0;
		uint16_t largest_value = 0;

		Memset(m_cdf, 0, sizeof(NewCdfEntry) * (65535 + 1));

		// Count frequencies
		for (uint32_t i = 0; i < input_length; i += 1)
		{
			const auto value = input[i];
			m_cdf[value].frequency += 1;

			if (m_cdf[value].frequency == 1)
			{
				m_cdf[value].value = value; // As silly it looks
				unique_values += 1;
				largest_value = Max(largest_value, value);
			}
		}

		m_cdf_len = unique_values;

		// Sort them
		sQuicksort(m_cdf, 0, largest_value);

		// Normalize them
		m = Max(static_cast<uint32_t>(256), Min(NearPowerOfTwo(input_length), ANS_M));
		m_cdf_m_len = Ctz(m);

		const auto scale = static_cast<float>(m) / static_cast<float>(input_length);
		{
			uint32_t summation = 0;
			uint32_t last_up_one = 0;

			for (uint32_t i = 0; i < unique_values; i += 1)
			{
				m_cdf[i].frequency = static_cast<uint16_t>(std::ceil(static_cast<float>(m_cdf[i].frequency) * scale));

				if (m_cdf[i].frequency == 0)
					m_cdf[i].frequency = 1;

				if (m_cdf[i].frequency > 1)
					last_up_one = i;

				summation += m_cdf[i].frequency;
			}

			while (summation >= m)
			{
				summation -= 1;
				m_cdf[last_up_one].frequency -= 1;
				while (m_cdf[last_up_one].frequency == 1)
					last_up_one -= 1;
			}
		}

		// Accumulate them
		uint16_t summation = 0;
		for (uint32_t i = 0; i < unique_values; i += 1)
		{
			m_cdf[i].cumulative = summation;
			summation += m_cdf[i].frequency;
		}

		// for (uint32_t i = 0; i < unique_values; i += 1)
		//{
		//	output_size += 16;
		//	output_size += 16;
		//}

		// Developers, developers, developers
		// uint32_t prev_f = m_cdf[0].frequency;
		// uint16_t fir[4] = {};
		// uint32_t fir_i = 0;

		/*for (uint32_t i = 0; i < unique_values; i += 1)
		{
		    const auto emit_f = (i != 0) ? (prev_f - m_cdf[i].frequency) : m_cdf[i].frequency;
		    prev_f = m_cdf[i].frequency;

		    printf("%2u [v: %u, f: %u, c: %u]\t->\t[%i, %u]\n", i, m_cdf[i].value, m_cdf[i].frequency,
		           m_cdf[i].cumulative, m_cdf[i].value, emit_f);
		}*/

		// printf(" - Cdf length: %u, largest value: %u, summation: %u (from: %u, x%.2f)\n", m_cdf_len, largest_value,
		//       summation, input_length, scale);
	}

	// Reset
	m_queue_cursor = 0;

	// Iterate input
	for (uint32_t i = input_length - 1; i < input_length; i -= 1) // Underflows, Ans operates in reverse
	{
		// Find Cdf entry
		auto e = m_cdf[m_cdf_len - 1];
		{
			const auto value = input[i];
			for (uint32_t u = 0; u < m_cdf_len; u += 1)
			{
				if (m_cdf[u].value == value)
				{
					e = m_cdf[u];
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
			if ((state / e.frequency) > (1 << (ANS_STATE_LEN - m_cdf_m_len)) - 1)
				goto proceed_with_normalization_as_check_will_wrap; // Gotos are bad ÒwÓ

			// Check
			if (((state / e.frequency) << m_cdf_m_len) + (state % e.frequency) + e.cumulative <= ANS_L * ANS_B - 1)
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
		state = ((state / e.frequency) << m_cdf_m_len) + (state % e.frequency) + e.cumulative;

		// Encode suffix, raw in bitstream
		/*if (e.suffix_length != 0)
		{
		    if (m_queue_cursor == QUEUE_LENGTH)
		        return 0;

		    m_queue[m_queue_cursor].v = static_cast<uint16_t>(input[i] - e.root);
		    m_queue[m_queue_cursor].l = e.suffix_length;
		    output_size += e.suffix_length;
		    m_queue_cursor += 1;
		}*/

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
	uint32_t prev_f = m_cdf[0].frequency;

	// printf("%u\n", m_cdf_m_len);
	// printf("%u\n", m_cdf_len);

	if (writer->WriteRice(m_cdf_m_len) != 0)
		return 1;

	if (writer->WriteRice(m_cdf_len) != 0)
		return 1;

	for (uint32_t i = 0; i < m_cdf_len; i += 1)
	{
		const auto emit_f = (i != 0) ? (prev_f - m_cdf[i].frequency) : m_cdf[i].frequency;
		prev_f = m_cdf[i].frequency;

		if (writer->WriteRice(m_cdf[i].value) != 0)
			return 1;
		if (writer->WriteRice(emit_f) != 0)
			return 1;

		// if (i < 5)
		//	printf(" %u (%u) -> %u\n", m_cdf[i].frequency, m_cdf[i].cumulative, emit_f);
	}

	// Write bits to output in reverse order
	while (m_queue_cursor != 0)
	{
		m_queue_cursor -= 1;
		if (writer->Write(m_queue[m_queue_cursor].v, m_queue[m_queue_cursor].l) != 0)
			return 1;
	}

	return 0;
}

} // namespace ako
