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

BitWriter::BitWriter(uint32_t output_length, uint32_t* output)
{
	Reset(output_length, output);
}


void BitWriter::Reset(uint32_t output_length, uint32_t* output)
{
	m_output_start = output;
	m_output_end = output + output_length;
	m_output = output;

	m_wrote_values = 0;

	m_accumulator = 0;
	m_accumulator_usage = 0;
}


int BitWriter::Write(uint32_t value, uint32_t bit_length)
{
	// Accumulator has space, ideal fast path
	if (m_accumulator_usage + bit_length < ACCUMULATOR_LEN)
	{
		const auto mask = ~(0xFFFFFFFF << bit_length);

		m_accumulator |= ((value & mask) << m_accumulator_usage);
		m_accumulator_usage += bit_length;
	}

	// No space in accumulator, ultra super duper slow path
	else
	{
		if (m_output + 1 > m_output_end || bit_length >= ACCUMULATOR_LEN)
			return 1;

		// Accumulate what we can, then write
		const auto min = ACCUMULATOR_LEN - m_accumulator_usage;
		*m_output++ = m_accumulator | (value << m_accumulator_usage);

		// Accumulate remainder
		const auto mask = ~(0xFFFFFFFF << (bit_length - min));

		m_accumulator = (value >> min) & mask;
		m_accumulator_usage = bit_length - min;

		// Developers, developers, developers
		// printf("\tE | \tWrite accumulator [d: 0x%x, min: %u]\n", *(m_output - 1), min);
	}

	// Developers, developers, developers
	// printf("\tE | v: %u,\tl: %u, u: %u\n", value, bit_length, m_accumulator_usage);

	// Bye!
	m_wrote_values += 1;
	return 0;
}


static uint32_t sBitlength(uint32_t v)
{
	uint32_t x = 0;
	while (v >= (1 << x))
		x += 1;

	return x;
}

int BitWriter::WriteRice(uint32_t value)
{
	value += 1;

	const auto length = sBitlength(value);
	const auto unary_length = (length + 1) / 2;
	const auto binary_length = unary_length * 2;

	// Developers, developers, developers
	// printf("\tE | v: %2u -> %u.%u (%u)\n", value - 1, unary_length, binary_length, length);

	return Write(((value << 1) | 1) << (unary_length - 1), unary_length + binary_length);
}


uint32_t BitWriter::Finish()
{
	// Remainder
	if (m_accumulator_usage > 0)
	{
		if (m_output + 1 > m_output_end)
			return 0;

		*m_output++ = m_accumulator;

		// Developers, developers, developers
		// printf("\tE | \tWrite accumulator [d: 0x%x]\n", *(m_output - 1));
	}

	// Bye!
	auto output_length = static_cast<uint32_t>(m_output - m_output_start);

	if (output_length == 0 && m_wrote_values != 0) // Somebody wrote lots of zeros
		output_length = 1;

	// printf("\tEncoded length: %u\n\n", output_length); // Developers, developers, developers
	return output_length;
}

} // namespace ako
