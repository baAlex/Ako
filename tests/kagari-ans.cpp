

#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ako-private.hpp"
#include "ako.hpp"

#include "decode/compression-kagari.hpp"
#include "encode/compression-kagari.hpp"


static void sFixedTest(const uint16_t* values, unsigned length)
{
	printf(" - Fixed Test, len: %u\n", length);

	auto buffer_a =
	    static_cast<uint32_t*>(malloc(sizeof(uint32_t) * length * 2)); // FIXME, encoder output is incomplete
	auto buffer_b = static_cast<uint16_t*>(malloc(sizeof(uint16_t) * length));
	assert(buffer_a != nullptr);
	assert(buffer_b != nullptr);

	// Encode
	const auto bitstream_length = ako::KagariAnsEncode(length, values, buffer_a);
	assert(bitstream_length != 0); // TODO, measured in length (accumulators) not bytes

	// Decode
	const auto read_length = ako::KagariAnsDecode(length, buffer_a, buffer_b); // TODO, length is confusing as is
	                                                                           // used in both output and input

	assert(read_length == bitstream_length); // We do not want to go out of sync

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


static void sLongTest(uint16_t value, unsigned length)
{
	printf(" - Long Test, v: %u, len: %u\n", value, length);

	auto values = static_cast<uint16_t*>(malloc(sizeof(uint16_t) * length));
	auto buffer_a =
	    static_cast<uint32_t*>(malloc(sizeof(uint32_t) * length * 2)); // FIXME, encoder output is incomplete
	auto buffer_b = static_cast<uint16_t*>(malloc(sizeof(uint16_t) * length));

	assert(values != nullptr);
	assert(buffer_a != nullptr);
	assert(buffer_b != nullptr);

	// Make values
	for (unsigned i = 0; i < length; i += 1)
		values[i] = value;

	// Encode
	const auto bitstream_length = ako::KagariAnsEncode(length, values, buffer_a);
	assert(bitstream_length != 0); // TODO, measured in length (accumulators) not bytes

	// Decode
	const auto read_length = ako::KagariAnsDecode(length, buffer_a, buffer_b); // TODO, length is confusing as is
	                                                                           // used in both output and input

	assert(read_length == bitstream_length); // We do not want to go out of sync

	// Check
	for (unsigned i = 0; i < length; i += 1)
	{
		if (values[i] != buffer_b[i])
			fprintf(stderr, "At %i\n", i);

		assert(values[i] == buffer_b[i]);
	}

	// Bye!
	free(values);
	free(buffer_a);
	free(buffer_b);
}


int main(int argc, const char* argv[])
{
	(void)argc;
	(void)argv;

	printf("# Kagari Ans Test (Ako v%i.%i.%i, %s)\n", ako::VersionMajor(), ako::VersionMinor(), ako::VersionPatch(),
	       (ako::SystemEndianness() == ako::Endianness::Little) ? "little-endian" : "big-endian");

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
		const uint16_t values[1] = {0};
		sFixedTest(values, 1);
	}
	{
		const uint16_t values[8] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
		sFixedTest(values, 8);
	}
	{
		const uint16_t values[1] = {0xFFFF};
		sFixedTest(values, 1);
	}

	sLongTest(0, 32);
	sLongTest(0xFFFF, 32);

	return EXIT_SUCCESS;
}
