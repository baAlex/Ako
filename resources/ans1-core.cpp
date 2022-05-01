/*

Released into the public domain, or under BSD Zero Clause License:

Copyright (c) 2022 Alexander Brandt

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
USE OR PERFORMANCE OF THIS SOFTWARE.
*/

// BIBLIOGRAPHY:

// - DUDA Jarek (2014). Asymmetric numeral systems: entropy coding combining speed of Huffman coding with compression
//   rate of arithmetic coding. Center for Science of Information, Purdue University. arXiv:1311.2540.
// - PASCO Richard Clark (1976). Source coding algorithms for fast data compression. Standford University.

// ----


// rANS CORE
// =========

// > clang++ ans1-core.cpp -std=c++20 -Wall -Wextra -Wconversion -pedantic -Werror -o ans
// > clang-tidy -checks="-*,cppcoreguidelines-*,bugprone-*,clang-analyzer-*,cert-*,misc-*,performance-*,portability-*"
// ans1-core.cpp


#include <iostream>
#include <vector>

#include "ans-cdf.hpp"


typedef uint64_t state_t;
const state_t INITIAL_STATE = 123;


// (En)coding:
// « C(s, x) = m * floor(x / l[s]) + b[s] + mod(x, l[s]) » (Duda 2014, p.8)

// Decoding:
// D(x) = (s <- s[mod(x, m)])
// D(x, s) = (x <- l[s] * floor(x / m) + mod(x, m) - b[s])

// Own nomenclature on Duda (2014, ibid.)

// Where 'l[s]' and 'b[s]' are, respectively, the probabilities of symbol 's' and its
// cumulative frequency. 'x' the state/accumulator and 'm' a number greater than the
// cumulative frequency of the last symbol in our lexicographic ordered cumulative-table.
// This last one can be also a power of two (helps in replace multiplications and
// divisions with shifts).


state_t C(state_t state, uint32_t frequency, uint32_t cumulative, uint32_t m)
{
	return m * (state / frequency) + (state % frequency) + cumulative;
}

state_t D(state_t state, uint32_t frequency, uint32_t cumulative, uint32_t m, uint32_t modulo_point)
{
	return frequency * (state / m) + modulo_point - cumulative;
}


template <typename T> state_t AnsEncode(const Cdf<T>& cdf, const std::vector<T>& message)
{
	std::cout << "\nEncode:\n";
	state_t state = INITIAL_STATE;

	for (const auto& i : message)
	{
		// Encode
		const CdfEntry<T>& e = cdf.of_symbol(i);
		state = C(state, e.frequency, e.cumulative, cdf.m());

		// Developers, developers, developers
		std::cout << " - '" << e.symbol << "' (f: " << e.frequency << ", c: " << e.cumulative << ")\t->\t" << state
		          << "\n";
	}

	return state;
}


template <typename T> void AnsDecode(const Cdf<T>& cdf, state_t state)
{
	std::cout << "\nDecode:\n";

	while (state > INITIAL_STATE)
	{
		// Decode
		const uint32_t modulo_point = (state % cdf.m());

		const CdfEntry<T>& e = cdf.of_point(modulo_point);
		state = D(state, e.frequency, e.cumulative, cdf.m(), modulo_point);

		// Developers, developers, developers
		std::cout << " - '" << e.symbol << "' (p: " << modulo_point << ", f: " << e.frequency << ", c: " << e.cumulative
		          << ")\t->\t" << state << "\n";
	}
}


int main()
{
	std::cout << "rANS Test 1\n\n";

	// Using Uint8 (char)
	{
		// const std::string s = "hello";
		const std::string s = "hello there";
		// const std::string s = "abracadabra";
		// const std::string s = "111111111112";
		// const std::string s = "211111111111";

		// const std::string s = "hello there, come here my little friend"; // Should fail
		// const std::string s = "1111111"; // Should fail

		try
		{
			const auto message = std::vector<char>(s.begin(), s.end());
			const auto cdf = Cdf<char>(message.data(), message.size());
			const auto state = AnsEncode(cdf, message);
			AnsDecode(cdf, state);
		}
		catch (const std::exception& e)
		{
			std::cout << e.what() << "\n";
			return 1;
		}
	}

	// Using Int16
	{
		std::cout << "\n";
		const std::vector<uint16_t> message = {1, 9, 2, 1, 6, 8, 0, 1};

		try
		{
			const auto cdf = Cdf<uint16_t>(message.data(), message.size());
			const auto state = AnsEncode(cdf, message);
			AnsDecode(cdf, state);
		}
		catch (const std::exception& e)
		{
			std::cout << e.what() << "\n";
			return 1;
		}
	}

	return 0;
}
