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

// > clang++ ans1.cpp -std=c++20 -Wall -Wextra -Wconversion -pedantic -Werror -o ans
// > clang-tidy -checks="-*,cppcoreguidelines-*,bugprone-*,clang-analyzer-*,cert-*,misc-*,performance-*,portability-*"
// ans1.cpp


#include <cstring>
#include <iostream>

#include "cdf.hpp"


typedef uint64_t state_t;
const state_t INITIAL_STATE = 123;


template <typename T> state_t Encode(const Cdf<T>& cdf, const T* message, size_t len)
{
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

	std::cout << "\nEncode:\n";
	state_t state = INITIAL_STATE;

	for (const T* i = message; i != message + len; i++)
	{
		// Encode
		const CdfEntry<T>& e = cdf.of_symbol(*i);

		const auto prev_state = state;
		state = cdf.max_cumulative() * (state / e.frequency) + (state % e.frequency) + e.cumulative;

		// Try to decode, a failure means that the state overflow-ed (well... wrap-around)
		{
			const uint32_t point = (state % cdf.max_cumulative()); // "Point" as is inside a range
			const CdfEntry<T>& e = cdf.of_point(point);

			const state_t decode_state = e.frequency * (state / cdf.max_cumulative()) + point - e.cumulative;

			if (decode_state != prev_state)
				throw std::runtime_error("ANS overflow, we don't have a reversible state.");
		}

		// Developers, developers, developers
		std::cout << " - '" << e.symbol << "' (f: " << e.frequency << ", c: " << e.cumulative << ")\t->\t" << state
		          << "\n";
	}

	return state;
}


template <typename T> void Decode(const Cdf<T>& cdf, state_t state)
{
	std::cout << "\nDecode:\n";

	while (state > INITIAL_STATE)
	{
		// Decode
		const uint32_t point = (state % cdf.max_cumulative());
		const CdfEntry<T>& e = cdf.of_point(point);

		state = e.frequency * (state / cdf.max_cumulative()) + point - e.cumulative;

		// Developers, developers, developers
		std::cout << " - '" << e.symbol << "' (p: " << point << ", f: " << e.frequency << ", c: " << e.cumulative
		          << ")\t->\t" << state << "\n";
	}
}


int main()
{
	std::cout << "rANS Test 1\n";

	// Using Uint8 (char)
	{
		// const auto message = "hello";
		const auto message = "hello there";
		// const auto message = "abracadabra";
		// const auto message = "111111111112";
		// const auto message = "211111111111";

		// const auto message = "hello there, come here my little friend"; // Should fail
		// const auto message = "1111111"; // Should fail

		try
		{
			const auto cdf = Cdf<char>(message, strlen(message));
			const auto state = Encode<char>(cdf, message, strlen(message));
			Decode<char>(cdf, state);
		}
		catch (const std::exception& e)
		{
			std::cout << e.what() << "\n";
			return 1;
		}
	}

	// Using Int16
	{
		const size_t length = 8;
		const int16_t message[length] = {1, 9, 2, 1, 6, 8, 0, 1};

		try
		{
			const auto cdf = Cdf<int16_t>(message, length);
			const auto state = Encode<int16_t>(cdf, message, length);
			Decode<int16_t>(cdf, state);
		}
		catch (const std::exception& e)
		{
			std::cout << e.what() << "\n";
			return 1;
		}
	}

	return 0;
}
