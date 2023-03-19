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

#define _GLIBCXX_DEBUG
#define LIBCXX_ENABLE_DEBUG_MODE

#include <functional>
#include <iostream>
#include <vector>

#include "shared.hpp"


// clang++ cdf53.cpp -Wall -Wextra -Wconversion -pedantic -O0 -g -o cdf53
// clang-tidy -checks="-*,bugprone-*,clang-analyzer-*,cert-*,misc-*,performance-*,portability-*" cdf53.cpp


// #############################################
// THERE IS A GLITCH HERE, "NEW-CDF53.PY" FIX IT
// #############################################


namespace Cdf53
{
static size_t MINIMUM_LENGTH = 4;

template <typename T> size_t Forward(const size_t input_length, const std::vector<T>& input, std::vector<T>& output)
{
	const size_t rule = HalfPlusOneRule(input_length);
	const size_t half = Half(input_length);

	// Highpass (length of 'half')
	for (size_t i = 0; i < half; i++)
	{
		const T even = input[(i << 1) + 0];
		const T odd = input[(i << 1) + 1];
		const T even_p1 = (i < half - 1) ? input[(i << 1) + 2] : even; // Extends

		output[rule + i] = odd - (even + even_p1) / 2;
	}

	// Lowpass (length of 'rule')
	for (size_t i = 0; i < half; i++)
	{
		const T even = input[(i << 1) + 0];
		const T hp = output[rule + i + 0];
		const T hp_l1 = (i > 0) ? output[rule + i - 1] : hp; // Extends

		output[i] = even + (hp_l1 + hp) / 4;
	}

	if (half != rule) // If length wasn't divisible by two, complete it
	{
		const size_t i = half;

		const T even = input[(i << 1) + 0];
		const T hp = output[half + i + 0];
		const T hp_l1 = output[half + i - 1]; // Extends

		output[i] = even + (hp_l1 + hp) / 4;
	}

	return rule;
}

template <typename T> void Inverse(const size_t input_length, const std::vector<T>& input, std::vector<T>& output)
{
	const size_t rule = HalfPlusOneRule(input_length);
	const size_t half = Half(input_length);

	// Evens (length of 'rule')
	for (size_t i = 0; i < half; i++)
	{
		const T lp = input[i];
		const T hp = input[rule + i + 0];
		const T hp_l1 = (i > 0) ? input[rule + i - 1] : hp; // Extends

		output[(i << 1) + 0] = lp - (hp_l1 + hp) / 4;
	}

	if (rule != half) // Complete it
	{
		const size_t i = half;

		const T lp = input[i];
		const T hp = input[half + i + 0];
		const T hp_l1 = input[half + i - 1]; // Extends

		output[(i << 1) + 0] = lp - (hp_l1 + hp) / 4;
	}

	// Odds (length of 'half')
	for (size_t i = 0; i < half; i++)
	{
		const T hp = input[rule + i + 0];
		const T even = output[(i << 1) + 0];
		const T even_p1 = (i < half - 1) ? output[(i << 1) + 2] : even; // Extends

		output[(i << 1) + 1] = hp + (even + even_p1) / 2;
	}
}
}; // namespace Cdf53


int main()
{
	int error = 0;

	for (size_t i = 1920 * 4; i > 24; i--)
		error = error | Test16(Cdf53::Forward<int16_t>, Cdf53::Inverse<int16_t>, Cdf53::MINIMUM_LENGTH,
		                       DataGenerator<int16_t>, i);
	for (size_t i = 24; i >= Cdf53::MINIMUM_LENGTH; i--)
		error = error | Test16(Cdf53::Forward<int16_t>, Cdf53::Inverse<int16_t>, Cdf53::MINIMUM_LENGTH,
		                       DataGenerator<int16_t>, i);

	if (error != 0)
	{
		std::cout << "Error!, something wasn't reversible\n";
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
