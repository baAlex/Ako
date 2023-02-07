

#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ako-private.hpp"
#include "ako.hpp"


static void sFixedTest()
{
	const uint32_t BUFFER_LEN = 64;
	uint32_t buffer[BUFFER_LEN] = {};

	printf(" - Fixed Test\n");

	// Encode
	uint32_t encoded_length = 0;
	{
		auto writer = ako::BitWriter(BUFFER_LEN, buffer);

		for (uint32_t i = 0; i < 17; i += 1)
		{
			const auto ret = writer.WriteRice(i);
			assert(ret == 0);
		}

		{
			const auto ret = writer.WriteRice(ako::RICE_MAX_VALUE);
			assert(ret == 0);
		}

		for (uint32_t i = 0; i < 16; i += 1)
		{
			const auto ret = writer.WriteRice(ako::RICE_MAX_VALUE - i);
			assert(ret == 0);
		}

		encoded_length = writer.Finish();
		assert(encoded_length != 0);
	}

	// Decode
#if 1
	{
		auto reader = ako::BitReader(encoded_length, buffer);

		uint32_t v;
		for (uint32_t i = 0; i < 17; i += 1)
		{
			const auto ret = reader.ReadRice(v);
			assert(ret == 0);
			assert(v == i);
		}

		{
			const auto ret = reader.ReadRice(v);
			assert(ret == 0);
			assert(v == ako::RICE_MAX_VALUE);
		}

		for (uint32_t i = 0; i < 16; i += 1)
		{
			const auto ret = reader.ReadRice(v);
			assert(ret == 0);
			assert(v == ako::RICE_MAX_VALUE - i);
		}

		const auto decoded_length = reader.Finish(buffer);
		assert(decoded_length == encoded_length);
	}
#endif
}


#if 1
static uint32_t sRng(uint32_t& inout_state)
{
	uint32_t x = inout_state;
	x ^= static_cast<uint32_t>(x << 13);
	x ^= static_cast<uint32_t>(x >> 17);
	x ^= static_cast<uint32_t>(x << 5);
	inout_state = x;
	return x;
}

static void sRandomTest(uint32_t length)
{
	auto buffer_a = reinterpret_cast<uint32_t*>(malloc(sizeof(uint32_t) * length));
	auto buffer_b = reinterpret_cast<uint32_t*>(malloc(sizeof(uint32_t) * length));
	assert(buffer_a != nullptr);
	assert(buffer_b != nullptr);

	printf(" - Random Test, len: %u\n", length);

	// Generate values
	{
		uint32_t s = 123;
		for (uint32_t i = 0; i < length; i += 1)
		{
			const auto v = sRng(s) % (ako::RICE_MAX_VALUE + 1);
			buffer_a[i] = v;
			// printf("E%u : %u\n", i, v);
		}
	}

	// Encode
	uint32_t encoded_length = 0;
	{
		auto writer = ako::BitWriter(length, buffer_b);

		for (uint32_t i = 0; i < length; i += 1)
		{
			const auto ret = writer.WriteRice(buffer_a[i]);
			assert(ret == 0);
		}

		encoded_length = writer.Finish();
		assert(encoded_length != 0);
	}

	// Decode
	{
		auto reader = ako::BitReader(encoded_length, buffer_b);

		uint32_t v;
		for (uint32_t i = 0; i < length; i += 1)
		{
			const auto ret = reader.ReadRice(v);
			// printf("D%u : %u\n", i, v);
			// if (ret != 0)
			//	printf("error code: %u\n", ret);

			assert(ret == 0);
			assert(v == buffer_a[i]);
		}

		const auto decoded_length = reader.Finish(buffer_b);
		assert(decoded_length == encoded_length);
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

	printf("# Rice Test (Ako v%i.%i.%i, %s)\n", ako::VersionMajor(), ako::VersionMinor(), ako::VersionPatch(),
	       (ako::SystemEndianness() == ako::Endianness::Little) ? "little-endian" : "big-endian");

	printf("Max = %u (0x%X)\n", ako::RICE_MAX_VALUE, ako::RICE_MAX_VALUE);

	sFixedTest();
	sRandomTest(16);
	sRandomTest(256);
	sRandomTest(2048);

	return EXIT_SUCCESS;
}
