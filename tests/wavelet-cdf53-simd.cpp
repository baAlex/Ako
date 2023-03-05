

#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ako-private.hpp"
#include "ako.hpp"


int main(int argc, const char* argv[])
{
	(void)argc;
	(void)argv;

	printf("# WaveletCdf53Simd Test (Ako v%i.%i.%i, %s)\n", ako::VersionMajor(), ako::VersionMinor(),
	       ako::VersionPatch(), (ako::SystemEndianness() == ako::Endianness::Little) ? "little-endian" : "big-endian");

	return EXIT_SUCCESS;
}
