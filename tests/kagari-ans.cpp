

#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ako-private.hpp"
#include "ako.hpp"

#include "decode/compression-kagari.hpp"
#include "encode/compression-kagari.hpp"


int main(int argc, const char* argv[])
{
	(void)argc;
	(void)argv;

	printf("# Kagari Ans Test (Ako v%i.%i.%i, %s)\n", ako::VersionMajor(), ako::VersionMinor(), ako::VersionPatch(),
	       (ako::SystemEndianness() == ako::Endianness::Little) ? "little-endian" : "big-endian");


	// const auto length = 7;
	// uint16_t buffer_a[length] = {73, 54, 1, 500, 1024, 300, 96};
	const auto length = 28;
	uint16_t buffer_a[length] = {0, 0, 1, 0, 2, 0, 1, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0};
	uint32_t buffer_b[length] = {};
	uint16_t buffer_c[length] = {};

	ako::KagariAnsEncode(length, buffer_a, buffer_b);
	assert(ako::KagariAnsDecode(length, buffer_b, buffer_c) != 0);

	for (int i = 0; i < length; i += 1)
	{
		if (buffer_a[i] != buffer_c[i])
			fprintf(stderr, "At %i\n", i);

		assert(buffer_a[i] == buffer_c[i]);
	}

	return EXIT_SUCCESS;
}
