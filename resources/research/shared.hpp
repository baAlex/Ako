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

const size_t PRINT_LENGTH_LIMIT = 24;


size_t Half(const size_t length)
{
	return (length >> 1);
}


size_t HalfPlusOneRule(const size_t length)
{
	return (length + (length & 1)) >> 1;
}


size_t TotalLifts(const size_t wavelet_min_length, size_t length)
{
	size_t i = 0;
	for (; length >= wavelet_min_length; i++, length = HalfPlusOneRule(length)) {}

	return i;
}


size_t UnliftNoOutputLength(const size_t wavelet_min_length, size_t final_length, const size_t unlift_no)
{
	const size_t total = TotalLifts(wavelet_min_length, final_length);
	for (size_t i = 0; i < total - unlift_no - 1; i++, final_length = HalfPlusOneRule(final_length)) {}

	return final_length;
}


template <typename T> void PrintGuide(const std::vector<T>& input)
{
	if (input.size() > PRINT_LENGTH_LIMIT)
		return;

	const size_t rule = HalfPlusOneRule(input.size());
	const size_t half = Half(input.size());

	std::cout << "\n";
	for (size_t i = 0; i < input.size(); i++)
		std::cout << (((i & 1) == 0) ? "O/" : "E/") << (i + 1) << "\t"; // Using math notation

	std::cout << "\n";
	for (size_t i = 0; i < rule; i++)
		std::cout << "----\t";
	for (size_t i = 0; i < half; i++)
		std::cout << "====\t";

	std::cout << "\n";
}


template <typename T> void PrintValues(const std::vector<T>& input)
{
	if (input.size() > PRINT_LENGTH_LIMIT)
		return;

	for (size_t i = 0; i < input.size() - 1; i++)
		std::cout << input[i] << ",\t";

	std::cout << input[input.size() - 1] << "\n";
}


template <typename T> T DataGenerator(const size_t i, uint32_t& inout_state)
{
	// https://en.wikipedia.org/wiki/Xorshift#Example_implementation
	uint32_t x = inout_state;
	x ^= static_cast<uint32_t>(x << 13);
	x ^= static_cast<uint32_t>(x >> 17);
	x ^= static_cast<uint32_t>(x << 5);
	inout_state = x;

	// return -1;
	// return (T)(x);
	return static_cast<T>((i + 1) * 10 + (inout_state & 16));
	// return static_cast<T>((i + 1) * 10);

	// return INT16_MAX;
	// return INT16_MIN;
}


std::vector<int16_t> a; // Oldest trick in the book
std::vector<int16_t> b;

int Test16(const std::function<size_t(const size_t, const std::vector<int16_t>&, std::vector<int16_t>&)>& forward,
           const std::function<void(const size_t, const std::vector<int16_t>&, std::vector<int16_t>&)>& inverse,
           const size_t wavelet_minimum_length, const std::function<int16_t(const size_t, uint32_t&)>& generator,
           const size_t length)
{
	// std::vector<int16_t> a{};
	// std::vector<int16_t> b{};

	// Generate data
	{
		a.resize(length);
		b.resize(length);

		uint32_t random_state = 1;
		for (size_t i = 0; i < length; i++)
			a[i] = generator(i, random_state);

		if (length <= PRINT_LENGTH_LIMIT)
		{
			PrintGuide(a);
			std::cout << " |Original, length: " << length << "|\n";
			PrintValues(a);
			std::cout << "\n";
		}
	}

	// Forward
	size_t lowpass_length = a.size();
	for (size_t lift_no = 0; lift_no < TotalLifts(wavelet_minimum_length, a.size()); lift_no++)
	{
		lowpass_length = forward(lowpass_length, a, b);

		if (length <= PRINT_LENGTH_LIMIT)
		{
			std::cout << " |Lift, output lowpass length: " << lowpass_length << "|\n";
			PrintValues(b);
		}

		a = b;
	}

	if (length <= PRINT_LENGTH_LIMIT)
		std::cout << "\n";

	// Inverse
	for (size_t unlift_no = 0; unlift_no < TotalLifts(wavelet_minimum_length, a.size()); unlift_no++)
	{
		const size_t output_length = UnliftNoOutputLength(wavelet_minimum_length, a.size(), unlift_no);
		inverse(output_length, a, b);

		if (length <= PRINT_LENGTH_LIMIT)
		{
			std::cout << " |Unlift, output length: " << output_length << "|\n";
			PrintValues(b);
		}

		a = b;
	}

	// Check data
	uint32_t random_state = 1;
	for (size_t i = 0; i < length; i++)
	{
		if (a[i] != generator(i, random_state))
		{
			std::cout << "\n**** Length of " << length << " yield to an error! ****\n";
			return 1;
		}
	}

	if (length <= PRINT_LENGTH_LIMIT)
		std::cout << "\n";

	return 0;
}
