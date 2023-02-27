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


static CdfEntry s_cdf[255 + 1];


#define GIESEN_TABLE
#ifdef GIESEN_TABLE
static uint8_t s_ryg[ANS_MAX_M + 1];
#endif


uint32_t AnsDecode(BitReader& reader, uint32_t output_length, uint16_t* output)
{
	uint32_t state = 0;
	uint32_t input_size = 0; // In bits

	uint32_t cdf_len = 0;
	uint32_t m = 0;
	uint32_t m_exp = 0;
	uint32_t m_mask = 0;

	if (output_length < 1)
		return 0;

	// Read Cdf
	{
		// Entries
		if ((input_size += reader.ReadRice(cdf_len)) == 0)
			return 0;

		uint32_t prev_f = 0;
		uint32_t cumulative = 0;
		for (uint32_t i = 0; i < cdf_len; i += 1)
		{
			uint32_t code;
			uint32_t transmitted_f;

			if ((input_size += reader.ReadRice(code)) == 0)
				return 0;
			if ((input_size += reader.ReadRice(transmitted_f)) == 0)
				return 0;

			s_cdf[i].root = sRoot(static_cast<uint8_t>(code));
			s_cdf[i].suffix_length = sSuffixLength(static_cast<uint8_t>(code));
			s_cdf[i].frequency = static_cast<uint16_t>((i != 0) ? (prev_f - transmitted_f) : transmitted_f);
			s_cdf[i].cumulative = static_cast<uint16_t>(cumulative);

			cumulative += s_cdf[i].frequency;
			prev_f = s_cdf[i].frequency;

			// printf("! %u-%u-%u (size: %u)\n", s_cdf[i].root, s_cdf[i].frequency, transmitted_f,
			//       RiceLength(s_cdf[i].root) + RiceLength(transmitted_f));
		}

		// M
		m = Max(ANS_MIN_M, Min(NearPowerOfTwo(cumulative), ANS_MAX_M));
		m_exp = Ctz(m);
		m_mask = ~(0xFFFFFFFF << m_exp);

		// Developers, developers, developers
		// for (uint32_t i = 0; i < cdf_len; i += 1)
		//	printf("\tCdf %u: root: %u, sl: %u, f: %u, c: %u\n", i, s_cdf[i].root, s_cdf[i].suffix_length,
		//	       s_cdf[i].frequency, s_cdf[i].cumulative);
		// printf("\tCdf length: %u, m: %u\n", cdf_len, m);
	}

#ifdef GIESEN_TABLE
	// Fill a look-up table
	{
		uint32_t modulo = 0;

		for (uint32_t u = 0; u < cdf_len - 1; u += 1)
		{
			for (; modulo < s_cdf[u + 1].cumulative; modulo += 1)
				s_ryg[modulo] = static_cast<uint8_t>(u);
		}

		for (; modulo < m + 1; modulo += 1)
			s_ryg[modulo] = static_cast<uint8_t>(cdf_len - 1);
	}
#endif

	// Decode
	for (uint32_t i = 0; i < output_length; i += 1)
	{
		// Normalize state (fill it)
		while (state < ANS_L)
		{
			uint32_t word;
			if (reader.Read(ANS_B_EXP, word) == 0)
				return 0;

			state = (state << ANS_B_EXP) | word;
			input_size += ANS_B_EXP;
		}

		// Retrieve root Cdf entry
		const auto modulo = state & m_mask;

#ifdef GIESEN_TABLE
		const auto e = s_cdf[s_ryg[modulo]];
#else
		auto e = s_cdf[cdf_len - 1];
		for (uint32_t u = 1; u < cdf_len; u += 1)
		{
			if (s_cdf[u].cumulative > modulo)
			{
				e = s_cdf[u - 1];
				break;
			}
		}
#endif

		// Output value
		{
			// Suffix raw from bitstream
			uint32_t suffix = 0;
			reader.Read(e.suffix_length, suffix); // Do not check, let it fail
			input_size += e.suffix_length;

			// Value is 'root + suffix'
			const auto value = static_cast<uint16_t>(e.root + suffix);
			*output++ = value; // TODO, check?

			// Developers, developers, developers
			// printf("\tAns: %u -> %u\n", state, e.cumulative);
		}

		// Update state
		state = e.frequency * (state >> m_exp) + modulo - e.cumulative;
	}

	// Normalize state
	// We want the bit reader to be in sync, and happens that the
	// encoder can normalize before processing the first value
	while (state < ANS_L)
	{
		uint32_t word;
		if (reader.Read(ANS_B_EXP, word) == 0)
			return 0;

		state = (state << ANS_B_EXP) | word;
		input_size += ANS_B_EXP;
	}

	// A final check
	if (state != ANS_INITIAL_STATE)
		return 0;

	// Bye!
	// printf("\tAns decoded size: %u bits\n\n", input_size);
	return input_size;
}

} // namespace ako
