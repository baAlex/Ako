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

AnsBitReader::AnsBitReader(uint32_t input_length, const uint32_t* input)
{
	this->Reset(input_length, input);
}

void AnsBitReader::Reset(uint32_t input_length, const uint32_t* input)
{
	this->input_end = input + input_length;
	this->input = input;

	this->accumulator = 0;
	this->accumulator_usage = 0;
}

int AnsBitReader::Read(uint32_t bit_length, uint32_t& value)
{
	const auto mask = ~(0xFFFFFFFF << bit_length);

	// Accumulator contains our value, ideal fast path
	if (bit_length <= this->accumulator_usage)
	{
		value = this->accumulator & mask;
		this->accumulator >>= bit_length;
		this->accumulator_usage -= bit_length;
	}

	// Accumulator doesn't have it, at least entirely, ultra super duper slow path
	else
	{
		if (this->input + 1 > this->input_end)
			return 1;

		// Keep what accumulator has
		const auto min = this->accumulator_usage;
		value = this->accumulator;

		// Read, make value with remainder part
		this->accumulator = *this->input++;
		this->accumulator_usage = ACCUMULATOR_LEN - (bit_length - min);

		value = (value | (this->accumulator << min)) & mask;
		this->accumulator >>= (bit_length - min);

		// Developers, developers, developers
		// printf("\tD | \tRead accumulator [d: 0x%x, min: %u]\n", *(this->input - 1), min);
	}

	// Developers, developers, developers
	// printf("\tD | v: %u,\tl: %u, u: %u (%u)\n", value, bit_length, this->accumulator_usage,
	//       ACCUMULATOR_LEN - this->accumulator_usage);

	// Bye!
	return 0;
}

uint32_t AnsBitReader::Finish(const uint32_t* input_start) const
{
	const auto input_length = Max(static_cast<uint32_t>(1), static_cast<uint32_t>(this->input - input_start));
	// printf("\tDecoded length: %u\n\n", input_length); // Developers, developers, developers
	return input_length;
}


uint32_t AnsDecode(uint32_t input_length, uint32_t output_length, const uint32_t* input, uint16_t* output)
{
	AnsBitReader reader(input_length, input);
	uint32_t state = 0;

	for (uint32_t i = 0; i < output_length; i += 1)
	{
		// Normalize state (fill it)
		while (state < ANS_L /*&& state != ANS_INITIAL_STATE*/)
		{
			uint32_t word;
			if (reader.Read(ANS_B_LEN, word) != 0)
				return 0;
			// if (word == 0) // Shouldn't be needed if 'INITIAL_STATE > ANS_L'
			//	break;

			state = (state << ANS_B_LEN) | word;
			// printf("\tD | %u\n", word); // Developers, developers, developers
		}

		// Find root Cdf entry
		const auto modulo = state & ANS_M_MASK;
		auto e = g_cdf1[G_CDF1_LEN - 1];

		for (uint32_t i = 1; i < G_CDF1_LEN; i += 1)
		{
			if (g_cdf1[i].cumulative > modulo)
			{
				e = g_cdf1[i - 1];
				break;
			}
		}

		// Output value
		{
			// Suffix raw from bitstream
			uint32_t suffix = 0;
			reader.Read(e.suffix_length, suffix); // Do not check, let it fail

			// Value is 'root + suffix'
			const auto value = static_cast<uint16_t>(e.root + suffix);
			*output++ = value; // TODO, check?

			// Developers, developers, developers
			// printf("\tD | 0x%x\t-> Value: %u (r: %u, sl: %u, f: %u, c: %u)\n", state, value, e.root, e.suffix_length,
			//       e.frequency, e.cumulative);
		}

		// Update state
		state = e.frequency * (state >> ANS_M_LEN) + modulo - e.cumulative;
	}

	// Normalize state
	// We want the bit reader to be in sync, and happens that the
	// encoder can normalize before processing the first value
	while (state < ANS_L)
	{
		uint32_t word;
		if (reader.Read(ANS_B_LEN, word) != 0)
			return 0;

		state = (state << ANS_B_LEN) | word;
		// printf("\tD | %u\n", word); // Developers, developers, developers
	}

	// A final check
	if (state != ANS_INITIAL_STATE)
		return 0;

	// Bye!
	return reader.Finish(input);
}

} // namespace ako
