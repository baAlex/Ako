

#undef NDEBUG
#include "wavelet.hpp"


#define HFORWARD ako::Cdf53HorizontalForward
#define HINVERSE ako::Cdf53HorizontalInverse
#define VFORWARD ako::Cdf53VerticalForward
#define VINVERSE ako::Cdf53InPlaceishVerticalInverse

int main()
{
	printf("# WaveletCdf53 Test (Ako v%i.%i.%i, %s)\n", ako::VersionMajor(), ako::VersionMinor(), ako::VersionPatch(),
	       (ako::SystemEndianness() == ako::Endianness::Little) ? "little-endian" : "big-endian");

	// Signed integers because the test operates on wavelet coefficients,
	// not image data (such case being unsigned integers)

	HorizontalTest(24, true, HFORWARD, HINVERSE, DataGeneratorMostlyRandom<int16_t>);
	VerticalTest(24, true, VFORWARD, VINVERSE, DataGeneratorMostlyRandom<int16_t>);
	HorizontalTest(23, true, HFORWARD, HINVERSE, DataGeneratorMostlyRandom<int16_t>);
	VerticalTest(23, true, VFORWARD, VINVERSE, DataGeneratorMostlyRandom<int16_t>);

	HorizontalTest(12, true, HFORWARD, HINVERSE, DataGeneratorMostlyRandom<int32_t>);
	VerticalTest(12, true, VFORWARD, VINVERSE, DataGeneratorMostlyRandom<int32_t>);
	HorizontalTest(11, true, HFORWARD, HINVERSE, DataGeneratorMostlyRandom<int32_t>);
	VerticalTest(11, true, VFORWARD, VINVERSE, DataGeneratorMostlyRandom<int32_t>);

	HorizontalTest(5, true, HFORWARD, HINVERSE, DataGeneratorMostlyRandom<int16_t>);
	VerticalTest(5, true, VFORWARD, VINVERSE, DataGeneratorMostlyRandom<int16_t>);
	HorizontalTest(4, true, HFORWARD, HINVERSE, DataGeneratorMostlyRandom<int16_t>); // Minimum length
	VerticalTest(4, true, VFORWARD, VINVERSE, DataGeneratorMostlyRandom<int16_t>);   // Ditto

	for (unsigned i = 640; i < 1920; i += 1)
	{
		HorizontalTest(i, false, HFORWARD, HINVERSE, DataGeneratorTrulyRandom<int16_t>);
		VerticalTest(i, false, VFORWARD, VINVERSE, DataGeneratorTrulyRandom<int16_t>);
	}

	return EXIT_SUCCESS;
}
