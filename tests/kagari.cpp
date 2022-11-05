

#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ako-private.hpp"
#include "ako.hpp"

#include "decode/compression-kagari.hpp"
#include "encode/compression-kagari.hpp"


template <typename T> static inline T sQuantizer(float q, T value)
{
	(void)q;
	return value;
}


template <typename T> static void sTest(const char* input)
{
	const size_t input_length = strlen(input);
	const size_t buffer_size = input_length * sizeof(T);

	printf(" - Rle Test, length: %zu, input: '%s'\n", input_length, input);

	// Allocate buffers
	auto buffer_a = reinterpret_cast<T*>(malloc(buffer_size));
	auto buffer_b = reinterpret_cast<T*>(malloc(buffer_size * 2)); // WARNING, for the moment being
	assert(buffer_a != nullptr);
	assert(buffer_b != nullptr);

	// Convert data
	for (size_t i = 0; i < input_length; i += 1)
		buffer_a[i] = static_cast<T>(static_cast<unsigned char>(input[i]));

	// """"Compress""""
	{
		auto compressor = ako::CompressorKagari<T>(buffer_b, buffer_size * 2); // WARNING, same here

		assert(compressor.Step(sQuantizer, 1.0F, static_cast<unsigned>(input_length), 1, buffer_a) == 0);
		assert(compressor.Finish() != 0);
	}

	// """"Decompress""""
	{
	}

	// Bye!
	free(buffer_a);
	free(buffer_b);
}


int main()
{
	printf("# Kagari Test (Ako v%i.%i.%i, %s)\n", ako::VersionMajor(), ako::VersionMinor(), ako::VersionPatch(),
	       (ako::SystemEndianness() == ako::Endianness::Little) ? "little-endian" : "big-endian");

	sTest<int16_t>("123456");
	sTest<int16_t>("111123456666");
	sTest<int16_t>("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
	sTest<int16_t>("aaaaaaaaaaaabcdefgaaaaaaaaaaaaaa");
	sTest<int16_t>("11111111222222223333333344444444555555556666666677777777");
	sTest<int16_t>("13525465112222221566664441111223333452123456666666111101");

	return EXIT_SUCCESS;
}
