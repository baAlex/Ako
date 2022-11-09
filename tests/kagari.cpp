

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
	const size_t buffer_size_a = input_length * sizeof(T);
	const size_t buffer_size_b = (input_length + 1) * sizeof(uint32_t); // WARNING, uint32 is the encoder internal type

	printf(" - Rle Test, length: %zu, input: '%s'\n", input_length, input);

	// Allocate buffers
	auto buffer_a = reinterpret_cast<T*>(malloc(buffer_size_a));
	auto buffer_b = reinterpret_cast<T*>(malloc(buffer_size_b));
	assert(buffer_a != nullptr);
	assert(buffer_b != nullptr);

	// Convert data
	for (size_t i = 0; i < input_length; i += 1)
		buffer_a[i] = static_cast<T>(static_cast<unsigned char>(input[i]));

	// """"Compress""""
	size_t compressed_size = 0;
	{
		auto compressor = ako::CompressorKagari<T>(buffer_b, buffer_size_b);

		assert(compressor.Step(sQuantizer, 1.0F, static_cast<unsigned>(input_length), 1, buffer_a) == 0);
		compressed_size = compressor.Finish();
		assert(compressed_size != 0);

		memset(buffer_a, 0, buffer_size_a);
	}

	// """"Decompress""""
	{
		auto decompressor = ako::DecompressorKagari<T>(buffer_b, compressed_size);

		assert(decompressor.Step(static_cast<unsigned>(input_length), 1, buffer_a) == ako::Status::Ok);
		// decompressor.Finish();
	}

	// Bye!
	free(buffer_a);
	free(buffer_b);
}


int main()
{
	printf("# Kagari Test (Ako v%i.%i.%i, %s)\n", ako::VersionMajor(), ako::VersionMinor(), ako::VersionPatch(),
	       (ako::SystemEndianness() == ako::Endianness::Little) ? "little-endian" : "big-endian");

	sTest<int16_t>("123456");                           // Literal only
	sTest<int16_t>("111123456666");                     // Literal only, since doesn't meets a minimum for a Rle
	sTest<int16_t>("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"); // Rle
	sTest<int16_t>("aaaaaaaaaaaabcdefgaaaaaaaaaaaaaa"); // Both
	sTest<int16_t>("11111111222222223333333344444444555555556666666677777777");
	sTest<int16_t>("13525465112222221566664441111223333452123456666666111101"); // Case ending in a literal

	return EXIT_SUCCESS;
}
