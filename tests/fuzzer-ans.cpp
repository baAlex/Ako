

// fuzzer-ans -max_len=131072 -len_control=50
// fuzzer-ans -max_len=131072 -len_control=0


#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ako-private.hpp"
#include "ako.hpp"


static const auto BUFFER_A_LENGTH = 32768;
static const auto BUFFER_B_LENGTH = 65536;

static uint32_t s_buffer_a[BUFFER_A_LENGTH];
static uint16_t s_buffer_b[BUFFER_B_LENGTH];


static int sTest(const uint16_t* values, size_t input_length)
{
	// « It may be desirable to reject some inputs (...) If the
	//   fuzz target returns -1 on a given input, libFuzzer
	//   will not add that input top the corpus »
	// (https://llvm.org/docs/LibFuzzer.html)

	if (input_length == 0)
		return -1;

	// Encode
	uint32_t write_size;   // In bits
	uint32_t write_length; // In 'accumulators' (what BitWriter() returns)
	{
		auto writer = ako::BitWriter(BUFFER_A_LENGTH, s_buffer_a);
		auto encoder = ako::AnsEncoder();

		write_size = encoder.Encode(ako::g_cdf_c, static_cast<uint32_t>(input_length), values);
		encoder.Write(&writer);
		write_length = writer.Finish();

		assert(write_size != 0);
		assert(write_length != 0);

		if (write_size != 0) // Zero is not an error, encoder may refuse to encode...
		{
			assert(write_size != 0);
			assert(write_length != 0);
		}
	}

	// Decode
	{
		auto reader = ako::BitReader(write_length, s_buffer_a);
		const auto read_size = ako::AnsDecode(reader, ako::g_cdf_c, static_cast<uint32_t>(input_length), s_buffer_b);
		const auto read_length = reader.Finish(s_buffer_a);

		assert(read_size == write_size);
		assert(read_length == write_length);
	}

	// Check
	for (unsigned i = 0; i < input_length; i += 1)
	{
		if (values[i] != s_buffer_b[i])
			fprintf(stderr, "At %i\n", i);

		assert(values[i] == s_buffer_b[i]);
	}

	// Bye!
	return 0;
}


extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	return sTest(reinterpret_cast<const uint16_t*>(data), size / sizeof(uint16_t));
	// return 0; // LLVMFuzzer: "Values other than 0 and -1 are reserved for future use"
}
