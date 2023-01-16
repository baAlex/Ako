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

BitReader::BitReader(uint32_t input_length, const uint32_t* input)
{
	Reset(input_length, input);
}

void BitReader::Reset(uint32_t input_length, const uint32_t* input)
{
	m_input_end = input + input_length;
	m_input = input;

	m_accumulator = 0;
	m_accumulator_usage = 0;
}

int BitReader::Read(uint32_t bit_length, uint32_t& value)
{
	const auto mask = ~(0xFFFFFFFF << bit_length);

	// Accumulator contains our value, ideal fast path
	if (bit_length <= m_accumulator_usage)
	{
		value = m_accumulator & mask;
		m_accumulator >>= bit_length;
		m_accumulator_usage -= bit_length;
	}

	// Accumulator doesn't have it, at least entirely, ultra super duper slow path
	else
	{
		if (m_input + 1 > m_input_end)
			return 1;

		// Keep what accumulator has
		const auto min = m_accumulator_usage;
		value = m_accumulator;

		// Read, make value with remainder part
		m_accumulator = *m_input++;
		m_accumulator_usage = ACCUMULATOR_LEN - (bit_length - min);

		value = (value | (m_accumulator << min)) & mask;
		m_accumulator >>= (bit_length - min);

		// Developers, developers, developers
		// printf("\tD | \tRead accumulator [d: 0x%x, min: %u]\n", *(m_input - 1), min);
	}

	// Developers, developers, developers
	// printf("\tD | v: %u,\tl: %u, u: %u (%u)\n", value, bit_length, m_accumulator_usage,
	//       ACCUMULATOR_LEN - m_accumulator_usage);

	// Bye!
	return 0;
}

uint32_t BitReader::Finish(const uint32_t* input_start) const
{
	const auto input_length = Max(static_cast<uint32_t>(1), static_cast<uint32_t>(m_input - input_start));
	// printf("\tDecoded length: %u\n\n", input_length); // Developers, developers, developers
	return input_length;
}

} // namespace ako