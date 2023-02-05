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

uint32_t AnsDecode(BitReader& reader, uint32_t output_length, uint16_t* output)
{
	uint32_t state = 0;
	uint32_t read_size = 0;

	for (uint32_t i = 0; i < output_length; i += 1)
	{
		// Normalize state (fill it)
		while (state < ANS_L /*&& state != ANS_INITIAL_STATE*/)
		{
			uint32_t word;
			if (reader.Read(ANS_B_LEN, word) != 0)
				return 0;

			state = (state << ANS_B_LEN) | word;
			read_size += ANS_B_LEN;
			// printf("\tD | %u\n", word); // Developers, developers, developers
		}

		// Find root Cdf entry
		const auto modulo = state & ANS_M_MASK;
		auto e = g_cdf1[G_CDF1_LEN - 1];

		for (uint32_t u = 1; u < G_CDF1_LEN; u += 1)
		{
			if (g_cdf1[u].cumulative > modulo)
			{
				e = g_cdf1[u - 1];
				break;
			}
		}

		// Output value
		{
			// Suffix raw from bitstream
			uint32_t suffix = 0;
			reader.Read(e.suffix_length, suffix); // Do not check, let it fail
			read_size += e.suffix_length;

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
		read_size += ANS_B_LEN;
		// printf("\tD | %u\n", word); // Developers, developers, developers
	}

	// A final check
	if (state != ANS_INITIAL_STATE)
		return 0;

	// Bye!
	return read_size;
}

} // namespace ako
