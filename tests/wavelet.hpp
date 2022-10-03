

#ifndef WAVELET_HPP
#define WAVELET_HPP

#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ako-private.hpp"
#include "ako.hpp"


const unsigned PRINT_LENGTH = 24;


inline void PrintGuide(unsigned length)
{
	if (length > PRINT_LENGTH)
		return;

	const auto rule = ako::HalfPlusOneRule(length);
	const auto half = ako::Half(length);

	printf("\n");
	for (unsigned i = 0; i < length; i += 1)
		printf("%s%u\t", ((i & 1) == 1) ? "O/" : "E/", i);

	printf("\n");
	for (size_t i = 0; i < rule; i += 1)
		printf("----\t");
	for (size_t i = 0; i < half; i += 1)
		printf("====\t");

	printf("\n");
}


template <typename T> inline void PrintValues(unsigned length, unsigned input_stride, const T* input)
{
	if (length > PRINT_LENGTH)
		return;

	for (unsigned i = 0; i < length - 1; i += 1)
		printf("%li\t", static_cast<long int>(input[i * input_stride]));

	printf("%li\n", static_cast<long int>(input[(length - 1) * input_stride]));
}


template <typename T> inline T DataGeneratorMostlyRandom(unsigned i, uint32_t& inout_state)
{
	// https://en.wikipedia.org/wiki/Xorshift#Example_implementation
	uint32_t x = inout_state;
	x ^= static_cast<uint32_t>(x << 13);
	x ^= static_cast<uint32_t>(x >> 17);
	x ^= static_cast<uint32_t>(x << 5);
	inout_state = x;

	// return static_cast<T>((i + 1) * 10);
	return static_cast<T>((i + 1) * 10 + (i % 10) + (inout_state & 16));
}


template <typename T> inline T DataGeneratorTrulyRandom(unsigned i, uint32_t& inout_state)
{
	(void)i;

	uint32_t x = inout_state;
	x ^= static_cast<uint32_t>(x << 13);
	x ^= static_cast<uint32_t>(x >> 17);
	x ^= static_cast<uint32_t>(x << 5);
	inout_state = x;

	return static_cast<T>(x);
}


template <typename T>
// clang-format off
inline void HorizontalTest(unsigned width, bool print,
                           void (*wavelet_transformation_forward)(unsigned, unsigned, unsigned, unsigned, const T* in, T* out),
                           void (*wavelet_transformation_inverse)(unsigned, unsigned, unsigned, unsigned, const T* lp, const T* hp, T* out),
                           T (*data_generator)(unsigned, uint32_t&))
// clang-format on
{
	const unsigned depth = sizeof(T) * 8; // Implicit

	if (print == true)
		printf(" - Horizontal, length: %u, %u bits\n", width, depth);

	// Allocate data
	T* buffer_a;
	T* buffer_b;
	{
		const auto buffer_size = static_cast<size_t>(width) * sizeof(T);

		buffer_a = reinterpret_cast<T*>(malloc(buffer_size));
		assert(buffer_a != nullptr);

		buffer_b = reinterpret_cast<T*>(malloc(buffer_size));
		assert(buffer_b != nullptr);
	}

	// Generate data
	{
		uint32_t generator_state = 1;
		for (unsigned i = 0; i < width; i += 1)
			buffer_a[i] = data_generator(i, generator_state);

		if (print == true)
		{
			PrintGuide(width);
			PrintValues(width, 1, buffer_a);
		}
	}

	// Forward
	wavelet_transformation_forward(width, 1, 0, 0, buffer_a, buffer_b);

	if (print == true)
		PrintValues(width, 1, buffer_b);

	// Inverse
	wavelet_transformation_inverse(1, ako::HalfPlusOneRule(width), ako::Half(width), 0, buffer_b,
	                               buffer_b + ako::HalfPlusOneRule(width), buffer_a);

	if (print == true)
		PrintValues(width, 1, buffer_a);

	// Check data
	{
		uint32_t generator_state = 1;
		for (unsigned i = 0; i < width; i += 1)
			assert(buffer_a[i] == data_generator(i, generator_state));
	}

	// Bye!
	if (print == true)
		printf("\n");

	free(buffer_a);
	free(buffer_b);
}


template <typename T>
// clang-format off
inline void VerticalTest(unsigned height, bool print,
                           void (*wavelet_transformation_forward)(unsigned, unsigned, unsigned, unsigned, const T* in, T* out),
                           void (*wavelet_transformation_inverse)(unsigned, unsigned, unsigned, const T* lp, T* inout_hp, T* out_lp),
                           T (*data_generator)(unsigned, uint32_t&))
// clang-format on
{
	const unsigned depth = sizeof(T) * 8; // Implicit
	const unsigned width = 32;            // We want some width to check that strides are done correctly

	if (print == true)
		printf(" - Vertical, length: %u, %u bits\n", height, depth);

	// Allocate data
	const auto buffer_size = static_cast<size_t>(width) * static_cast<size_t>(height) * sizeof(T);

	T* buffer_a = reinterpret_cast<T*>(malloc(buffer_size));
	assert(buffer_a != nullptr);

	T* buffer_b = reinterpret_cast<T*>(malloc(buffer_size));
	assert(buffer_b != nullptr);

	// Generate data on first column, the other undefined to let Valgrind
	// warning us about it (in case of an undefined read, an error)
	{
		uint32_t generator_state = 1;

		for (unsigned row = 0; row < height; row += 1)
			for (unsigned col = 0; col < width; col += 1)
			{
				if (col == 0)
					buffer_a[row * width + col] = data_generator(row, generator_state);
			}

		if (print == true)
		{
			PrintGuide(height);
			PrintValues(height, width, buffer_a);
		}
	}

	// Poison buffer_b
	{
		for (unsigned row = 0; row < height; row += 1)
			for (unsigned col = 0; col < width; col += 1)
				buffer_b[row * width + col] = 12345;
	}

	// Forward
	wavelet_transformation_forward(1, height, width, width, buffer_a, buffer_b);

	if (print == true)
		PrintValues(height, width, buffer_b);

	// Check poison in buffer_b, except first column
	// (forward() shouldn't touch outside of what was indicated)
	{
		for (unsigned row = 0; row < height; row += 1)
			for (unsigned col = 0; col < width; col += 1)
			{
				if (col != 0)
					assert(buffer_b[row * width + col] == 12345);
			}
	}

	// Inverse (in place, that is why the weird inputs/output)
	wavelet_transformation_inverse(width, ako::HalfPlusOneRule(height), ako::Half(height), buffer_b,
	                               buffer_b + ako::HalfPlusOneRule(height) * width, buffer_b);

	// Interleave values
	memset(buffer_a, 0, buffer_size);

	for (unsigned i = 0; i < ako::HalfPlusOneRule(height); i += 1)
		buffer_a[(i << 1) + 0] = buffer_b[i * width];
	for (unsigned i = 0; i < ako::Half(height); i += 1)
		buffer_a[(i << 1) + 1] = (buffer_b + ako::HalfPlusOneRule(height) * width)[i * width];

	if (print == true)
	{
		// In place (odd and even de-interleaved) values
		// PrintValues(ako::HalfPlusOneRule(height), width, buffer_b);
		// PrintValues(ako::Half(height), width, buffer_b + ako::HalfPlusOneRule(height) * width);

		// Interleaved values
		PrintValues(height, 1, buffer_a);
	}

	// Check data
	{
		uint32_t generator_state = 1;
		for (unsigned i = 0; i < height; i += 1)
			assert(buffer_a[i] == data_generator(i, generator_state));
	}

	// Bye!
	if (print == true)
		printf("\n");

	free(buffer_a);
	free(buffer_b);
}

#endif
