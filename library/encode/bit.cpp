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
	this->Reset(output_length, output);
}

void BitWriter::Reset(uint32_t output_length, uint32_t* output)
{
	this->output_start = output;
	this->output_end = this->output_start + output_length;
	this->output = this->output_start;

	this->wrote_values = 0;

	this->accumulator = 0;
	this->accumulator_usage = 0;
}

int BitWriter::Write(uint32_t value, uint32_t bit_length)
{
	// Accumulator has space, ideal fast path
	if (this->accumulator_usage + bit_length < ACCUMULATOR_LEN)
	{
		const auto mask = ~(0xFFFFFFFF << bit_length);

		this->accumulator |= ((value & mask) << this->accumulator_usage);
		this->accumulator_usage += bit_length;
	}

	// No space in accumulator, ultra super duper slow path
	else
	{
		if (this->output + 1 > this->output_end || bit_length >= ACCUMULATOR_LEN)
			return 1;

		// Accumulate what we can, then write
		const auto min = ACCUMULATOR_LEN - this->accumulator_usage;
		*this->output++ = this->accumulator | (value << this->accumulator_usage);

		// Accumulate remainder
		const auto mask = ~(0xFFFFFFFF << (bit_length - min));

		this->accumulator = (value >> min) & mask;
		this->accumulator_usage = bit_length - min;

		// Developers, developers, developers
		// printf("\tE | \tWrite accumulator [d: 0x%x, min: %u]\n", *(this->output - 1), min);
	}

	// Developers, developers, developers
	// printf("\tE | v: %u,\tl: %u, u: %u\n", value, bit_length, this->accumulator_usage);

	// Bye!
	this->wrote_values += 1;
	return 0;
}

uint32_t BitWriter::Finish()
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
	auto output_length = static_cast<uint32_t>(this->output - this->output_start);

	if (output_length == 0 && this->wrote_values != 0) // Somebody wrote lots of zeros
		output_length = 1;

	// printf("\tEncoded length: %u\n\n", output_length); // Developers, developers, developers
	return output_length;
}

} // namespace ako
