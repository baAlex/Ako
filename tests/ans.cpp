

#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ako-private.hpp"
#include "ako.hpp"


#if 1
static void sFixedTest(const uint16_t* values, unsigned length)
{
	printf(" - Fixed Test, len: %u\n   [", length);
	for (unsigned i = 0; i < length; i += 1)
		printf("%u%s", values[i], (i != length - 1) ? ", " : "]\n");

	auto buffer_a = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * length * 4)); // 'uint32_t' for inflation
	auto buffer_b = static_cast<uint16_t*>(malloc(sizeof(uint16_t) * length));
	assert(buffer_a != nullptr);
	assert(buffer_b != nullptr);

	// Encode
	uint32_t write_size;   // In bits
	uint32_t write_length; // In 'accumulators' (what BitWriter() returns)
	{
		auto encoder = ako::AnsEncoder();
		auto writer = ako::BitWriter(length * 4, buffer_a);

		write_size = encoder.Encode(length, values);
		assert(write_size != 0);

		encoder.Write(&writer);
		write_length = writer.Finish();

		assert(write_size != 0);
		assert(write_length != 0);
	}

	// Decode
	uint32_t read_size;   // Same
	uint32_t read_length; // Same
	{
		auto reader = ako::BitReader(write_length, buffer_a);
		read_size = ako::AnsDecode(reader, length, buffer_b);
		read_length = reader.Finish(buffer_a);

		assert(read_size == write_size);
		assert(read_length == write_length);
	}

	// Check
	for (unsigned i = 0; i < length; i += 1)
	{
		if (values[i] != buffer_b[i])
			fprintf(stderr, "At %i\n", i);

		assert(values[i] == buffer_b[i]);
	}

	// Bye!
	free(buffer_a);
	free(buffer_b);
}
#endif


int main(int argc, const char* argv[])
{
	(void)argc;
	(void)argv;

	printf("# Ans Test (Ako v%i.%i.%i, %s)\n", ako::VersionMajor(), ako::VersionMinor(), ako::VersionPatch(),
	       (ako::SystemEndianness() == ako::Endianness::Little) ? "little-endian" : "big-endian");

	// Tiny lengths to test Cdf creation
	{
		const uint16_t values[1] = {0}; // Something funny to encode in real life
		sFixedTest(values, 1);
	}

	{
		const uint16_t values[2] = {8, 4};
		sFixedTest(values, 2);
	}

	{
		const uint16_t values[3] = {1, 2, 3};
		sFixedTest(values, 3);
	}

	{
		const uint16_t values[4] = {4, 5, 6, 7};
		sFixedTest(values, 4);
	}

	{
		const uint16_t values[5] = {4, 5, 6, 7, 8};
		sFixedTest(values, 5);
	}

	{
		const uint16_t values[3] = {3, 0, 0};
		sFixedTest(values, 3);
	}

	{
		const uint16_t values[4] = {3, 0, 0, 0};
		sFixedTest(values, 4);
	}

	// More tests!
	{
		const uint16_t values[7] = {73, 54, 1, 500, 1024, 300, 96};
		sFixedTest(values, 7);
	}

	{
		const uint16_t values[28] = {0, 0, 1, 0, 2, 0, 1, 0, 0, 0, 0, 0, 1, 2,
		                             3, 4, 5, 6, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0};
		sFixedTest(values, 28);
	}

	{
		const uint16_t values[8] = {0, 0, 0, 0, 0, 0, 0, 0};
		sFixedTest(values, 8);
	}

	{
		const uint16_t values[8] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
		sFixedTest(values, 8);
	}

	return EXIT_SUCCESS;
}
