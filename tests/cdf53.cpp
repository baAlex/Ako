

#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ako-private.hpp"
#include "ako.hpp"


const unsigned PRINT_LENGTH = 24;


static void sPrintGuide(unsigned length)
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


template <typename T> static void sPrintValues(unsigned length, const T* input)
{
	if (length > PRINT_LENGTH)
		return;

	for (unsigned i = 0; i < length - 1; i += 1)
		printf("%li\t", static_cast<long int>(input[i]));

	printf("%li\n", static_cast<long int>(input[length - 1]));
}


template <typename T> static void sHorizontalTest(unsigned width, bool print, T (*data_generator)(unsigned, uint32_t&))
{
	const unsigned depth = sizeof(T) * 8; // Implicit

	if (print == true)
		printf(" - Horizontal, length: %u, %u bits (coefficients)\n", width, depth);

	// Allocate data
	T* buffer_a;
	T* buffer_b;
	{
		const auto data_size = static_cast<size_t>(width) * sizeof(T);

		buffer_a = reinterpret_cast<T*>(malloc(data_size));
		assert(buffer_a != nullptr);

		buffer_b = reinterpret_cast<T*>(malloc(data_size));
		assert(buffer_b != nullptr);
	}

	// Generate data
	{
		uint32_t generator_state = 1;
		for (unsigned i = 0; i < width; i += 1)
			buffer_a[i] = data_generator(i, generator_state);

		if (print == true)
		{
			sPrintGuide(width);
			sPrintValues(width, buffer_a);
		}
	}

	// Forward
	ako::Cdf53HorizontalForward(width, 1, 0, 0, buffer_a, buffer_b);

	if (print == true)
		sPrintValues(width, buffer_b);

	// Inverse
	ako::Cdf53HorizontalInverse(1, ako::HalfPlusOneRule(width), ako::Half(width), 0, buffer_b,
	                            buffer_b + ako::HalfPlusOneRule(width), buffer_a);

	if (print == true)
		sPrintValues(width, buffer_a);

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


template <typename T> static T sDataGenerator(unsigned i, uint32_t& inout_state)
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


int main()
{
	printf("# Cdf53 Test (Ako v%i.%i.%i, %s)\n", ako::VersionMajor(), ako::VersionMinor(), ako::VersionPatch(),
	       (ako::SystemEndianness() == ako::Endianness::Little) ? "little-endian" : "big-endian");

	sHorizontalTest(24, true, sDataGenerator<int16_t>); // Int16 because the test operates on wavelet coefficients,
	sHorizontalTest(23, true, sDataGenerator<int16_t>); // not image data (such case being unsigned ints)

	sHorizontalTest(12, true, sDataGenerator<int32_t>);
	sHorizontalTest(11, true, sDataGenerator<int32_t>);

	sHorizontalTest(5, true, sDataGenerator<int16_t>);
	sHorizontalTest(4, true, sDataGenerator<int16_t>); // Minimum length

	for (unsigned i = 640; i < 1920; i += 1)
		sHorizontalTest(i, false, sDataGenerator<int16_t>);

	return EXIT_SUCCESS;
}
