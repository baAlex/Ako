

// fuzzer-ans -max_len=131072 -len_control=50
// fuzzer-ans -max_len=131072 -len_control=0


#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "ako-private.hpp"
#include "ako.hpp"


static const auto BUFFER_A_LENGTH = 32768;
static uint32_t s_buffer_a[BUFFER_A_LENGTH];

static const auto BUFFER_B_LENGTH = 65536;
static uint16_t s_buffer_b[BUFFER_B_LENGTH];


static size_t s_refuses;
static size_t s_runs;
static size_t s_largest_last_input;
static time_t s_last_update = time(nullptr) - 10;


static int sTest(const uint16_t* values, size_t input_length)
{
	// « It may be desirable to reject some inputs (...) If the
	//   fuzz target returns -1 on a given input, libFuzzer
	//   will not add that input top the corpus »
	// (https://llvm.org/docs/LibFuzzer.html)

	if (input_length == 0)
		return -1;

	if (input_length > s_largest_last_input)
		s_largest_last_input = input_length;

	// Encode
	uint32_t encode_size;  // In bits
	uint32_t write_length; // In 'accumulators' (what BitWriter() returns)
	{
		auto writer = ako::BitWriter(BUFFER_A_LENGTH, s_buffer_a);
		auto encoder = ako::AnsEncoder();

		encode_size = encoder.Encode(static_cast<uint32_t>(input_length), values);
		const auto ret = encoder.Write(&writer);

		write_length = writer.Finish();

		// Checks
		assert(ret == encode_size);

		if (encode_size != 0)
			assert(write_length != 0);
		if (write_length != 0)
			assert(encode_size != 0);
		if (encode_size == 0)
			assert(write_length == 0);

		// Stats
		if (encode_size == 0)
			s_refuses += 1;
		s_runs += 1;
	}

	// Decode
	{
		auto reader = ako::BitReader(write_length, s_buffer_a);
		const auto decode_size = ako::AnsDecode(reader, static_cast<uint32_t>(input_length), s_buffer_b);
		const auto read_length = reader.Finish(s_buffer_a);

		assert(decode_size == encode_size);
		assert(read_length == write_length);
	}

	// Check
	for (unsigned i = 0; i < input_length; i += 1)
	{
		if (values[i] != s_buffer_b[i])
			fprintf(stderr, "At %i\n", i);

		assert(values[i] == s_buffer_b[i]);
	}

	// Some feedback
	{
		const auto current_time = time(nullptr);
		if (current_time >= s_last_update + 10)
		{
			printf(" - Largest length: %zu (from previous update), runs: %zu, refuses: %zu\n", s_largest_last_input,
			       s_runs, s_refuses);
			s_last_update = current_time;
			s_largest_last_input = 0;
		}
	}

	// Bye!
	return 0;
}


extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	return sTest(reinterpret_cast<const uint16_t*>(data), size / sizeof(uint16_t));
	// return 0; // LLVMFuzzer: "Values other than 0 and -1 are reserved for future use"
}
