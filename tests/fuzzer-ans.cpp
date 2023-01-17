

// fuzzer-ans -max_len=131072 -len_control=50
// fuzzer-ans -max_len=131072 -len_control=0


#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ako-private.hpp"
#include "ako.hpp"


#if 0

static uint32_t s_buffer_a[32768];
static uint16_t s_buffer_b[65536];


static int sTest(const uint16_t* input, size_t input_length)
{
	// « It may be desirable to reject some inputs (...) If the
	//   fuzz target returns -1 on a given input, libFuzzer
	//   will not add that input top the corpus »
	// (https://llvm.org/docs/LibFuzzer.html)

	if (input_length == 0)
		return -1;
	// if (input_length > 65536) // AnsEncode() handles it
	//	return -1;

	// Encode
	uint32_t encoded_length;
	{
		auto status = ako::AnsEncoderStatus::Error;

		auto writer = ako::BitWriter(32768, s_buffer_a);
		encoded_length = ako::AnsEncode(static_cast<uint32_t>(input_length), input, &writer, status);

		if (encoded_length == 0) // Not an error, encoder may refuse to encode...
		{
			assert(status != ako::AnsEncoderStatus::Ok); // ...but status and length should match
			return 0;
		}
		else
			assert(status == ako::AnsEncoderStatus::Ok);
	}

	// Decode
	{
		const auto read_length =
		    ako::AnsDecode(encoded_length, static_cast<uint32_t>(input_length), s_buffer_a, s_buffer_b);
		assert(read_length == encoded_length);
	}

	// Check
	for (unsigned i = 0; i < input_length; i += 1)
	{
		if (input[i] != s_buffer_b[i])
			fprintf(stderr, "At %i\n", i);

		assert(input[i] == s_buffer_b[i]);
	}

	// Bye!
	return 0;
}
#endif

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	(void)data;
	(void)size;
	return 0;
	// return sTest(reinterpret_cast<const uint16_t*>(data), size / sizeof(uint16_t));
	// return 0; // LLVMFuzzer: "Values other than 0 and -1 are reserved for future use"
}
