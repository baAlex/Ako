

#undef NDEBUG
#include "wavelet.hpp"


#define FORWARD ako::Cdf53HorizontalForward
#define INVERSE ako::Cdf53HorizontalInverse

int main()
{
	printf("# WaveletHaar Test (Ako v%i.%i.%i, %s)\n", ako::VersionMajor(), ako::VersionMinor(), ako::VersionPatch(),
	       (ako::SystemEndianness() == ako::Endianness::Little) ? "little-endian" : "big-endian");

	// Int16 because the test operates on wavelet coefficients,
	// not image data (such case being unsigned ints)

	HorizontalTest(24, true, FORWARD, INVERSE, DataGeneratorRandom<int16_t>);
	HorizontalTest(23, true, FORWARD, INVERSE, DataGeneratorRandom<int16_t>);

	HorizontalTest(12, true, FORWARD, INVERSE, DataGeneratorRandom<int32_t>);
	HorizontalTest(11, true, FORWARD, INVERSE, DataGeneratorRandom<int32_t>);

	HorizontalTest(5, true, FORWARD, INVERSE, DataGeneratorRandom<int16_t>);
	HorizontalTest(4, true, FORWARD, INVERSE, DataGeneratorRandom<int16_t>); // Minimum length

	for (unsigned i = 640; i < 1920; i += 1)
		HorizontalTest(i, false, FORWARD, INVERSE, DataGeneratorRandom<int16_t>);

	return EXIT_SUCCESS;
}
