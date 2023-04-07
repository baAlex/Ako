

#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ako-private.hpp"
#include "ako.hpp"


static uint16_t sLfsr16(uint16_t* x)
{
	*x ^= static_cast<uint16_t>(*x << 7);
	*x ^= static_cast<uint16_t>(*x >> 9);
	*x ^= static_cast<uint16_t>(*x << 8);
	return *x;
}

static void sPrint(unsigned w, unsigned h, const int16_t* data)
{
	for (unsigned row = 0; row < h; row += 1)
	{
		for (unsigned col = 0; col < w; col += 1)
			printf("%3i ", data[w * row + col]);
		printf("\n");
	}
	printf("\n");
}

static void sTest(unsigned width, unsigned height = 6)
{
	printf(" - Horizontal, length: %u\n", width);

	const auto rule = ako::HalfPlusOneRule(width);
	const auto half = ako::Half(width);

	auto buffer_a = static_cast<int16_t*>(malloc(sizeof(int16_t) * width * height));
	auto buffer_b = static_cast<int16_t*>(malloc(sizeof(int16_t) * width * height));
	auto buffer_lp = static_cast<int16_t*>(malloc(sizeof(int16_t) * rule * height));
	auto buffer_hp = static_cast<int16_t*>(malloc(sizeof(int16_t) * half * height));
	assert(buffer_a != nullptr);
	assert(buffer_b != nullptr);
	assert(buffer_lp != nullptr);
	assert(buffer_hp != nullptr);

	// Generate data
	uint16_t s = 1;
	for (unsigned row = 0; row < height; row += 1)
	{
		for (unsigned col = 0; col < width; col += 1)
			buffer_a[width * row + col] = static_cast<int16_t>(col + row + (sLfsr16(&s) & 15));
	}

	sPrint(width, height, buffer_a);

	// Lift
	ako::Cdf53HorizontalForward<int16_t>(width, height, width, width, buffer_a, buffer_b);
	// sPrint(width, height, buffer_b);

	// Some memory manoeuvres
	// lift() outputs the highpass and lowpass in a 2d fashion as an image (like the encoder works), while
	// unlift() expects two different objects (or images) (as it works by receiving one highpass at time)
	ako::Memcpy2d(rule, height, width, rule, buffer_b, buffer_lp);
	ako::Memcpy2d(half, height, width, half, buffer_b + rule, buffer_hp);

	// sPrint(rule, height, buffer_lp);
	// sPrint(half, height, buffer_hp);

	// Unlift
	ako::Memset(buffer_b, 0, sizeof(int16_t) * width * height);
	ako::Cdf53HorizontalInverse(height, rule, half, width, buffer_lp, buffer_hp, buffer_b);
	sPrint(width, height, buffer_b);

	// Check
	for (unsigned i = 0; i < (width * height); i += 1)
	{
		assert(buffer_b[i] == buffer_a[i]);
	}

	// Bye!
	free(buffer_a);
	free(buffer_b);
	free(buffer_lp);
	free(buffer_hp);
}


int main(int argc, const char* argv[])
{
	(void)argc;
	(void)argv;

	printf("# WaveletCdf53 Test 2 (Ako v%i.%i.%i, %s)\n", ako::VersionMajor(), ako::VersionMinor(), ako::VersionPatch(),
	       (ako::SystemEndianness() == ako::Endianness::Little) ? "little-endian" : "big-endian");

	sTest(48);
	sTest(47);

	sTest(24);
	sTest(23);

	sTest(4);

	return EXIT_SUCCESS;
}
