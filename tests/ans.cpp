

#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ako-private.hpp"
#include "ako.hpp"


#if 1
static void sFixedTest(const uint16_t* values, unsigned input_length)
{
	printf(" - Fixed Test, len: %u\n", input_length);

	const auto output_buffer_length = input_length * 4; // For inflation, we are not testing that here

	auto buffer_a = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * output_buffer_length));
	auto buffer_b = static_cast<uint16_t*>(malloc(sizeof(uint16_t) * input_length));
	assert(buffer_a != nullptr);
	assert(buffer_b != nullptr);

	// Encode
	uint32_t write_size;   // In bits
	uint32_t write_length; // In 'accumulators' (what BitWriter() returns)
	{
		auto writer = ako::BitWriter(output_buffer_length, buffer_a);
		auto encoder = ako::AnsEncoder();

		write_size = encoder.Encode(ako::g_cdf_c, input_length, values);
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
		read_size = ako::AnsDecode(reader, ako::g_cdf_c, input_length, buffer_b);
		read_length = reader.Finish(buffer_a);

		assert(read_size == write_size);
		assert(read_length == write_length);
	}

	// Check
	for (unsigned i = 0; i < input_length; i += 1)
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


#if 1
static void sLongUniformTest(uint16_t value, unsigned input_length)
{
	printf(" - Long Uniform Test, v: %u, len: %u\n", value, input_length);

	const auto output_buffer_length = input_length * 4; // For inflation, we are not testing that here

	auto values = static_cast<uint16_t*>(malloc(sizeof(uint16_t) * input_length));
	auto buffer_a = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * output_buffer_length));
	auto buffer_b = static_cast<uint16_t*>(malloc(sizeof(uint16_t) * input_length));
	assert(values != nullptr);
	assert(buffer_a != nullptr);
	assert(buffer_b != nullptr);

	// Make values
	for (unsigned i = 0; i < input_length; i += 1)
		values[i] = value;

	// Encode
	uint32_t write_size;   // In bits
	uint32_t write_length; // In 'accumulators' (what BitWriter() returns)
	{
		auto writer = ako::BitWriter(output_buffer_length, buffer_a);
		auto encoder = ako::AnsEncoder();

		write_size = encoder.Encode(ako::g_cdf_c, input_length, values);
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
		read_size = ako::AnsDecode(reader, ako::g_cdf_c, input_length, buffer_b);
		read_length = reader.Finish(buffer_a);

		assert(read_size == write_size);
		assert(read_length == write_length);
	}

	// Check
	for (unsigned i = 0; i < input_length; i += 1)
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
#endif


int main(int argc, const char* argv[])
{
	(void)argc;
	(void)argv;

	printf("# Ans Test (Ako v%i.%i.%i, %s)\n", ako::VersionMajor(), ako::VersionMinor(), ako::VersionPatch(),
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

	sLongUniformTest(0, 32);
	sLongUniformTest(0xFFFF, 32);

	sLongUniformTest(0, 0xFFFF);      // Maximum length
	sLongUniformTest(0xFFFF, 0xFFFF); // Ditto, but now with inflation

	return EXIT_SUCCESS;
}
