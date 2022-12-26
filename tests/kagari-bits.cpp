

#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ako-private.hpp"
#include "ako.hpp"

#include "decode/compression-kagari.hpp"
#include "encode/compression-kagari.hpp"


static uint32_t sBitsLength(uint32_t v)
{
	uint32_t len = 1;
	for (; v > 1; v >>= 1)
		len++;

	return len;
}


static void sFixedTest()
{
	printf("FixedTest\n");

	const auto values_no = 10;
	const uint32_t values[values_no] = {0xFFFF, 1023, 15, 13, 0xF123, 2, 4, 5, 0x7FFFFFFF, 3};

	uint32_t buffer[values_no] = {};

	// Write
	size_t encoded_length = 0;
	{
		auto writer = ako::KagariBitWriter(values_no, buffer);
		for (unsigned i = 0; i < values_no; i += 1)
		{
			const auto ret = writer.WriteBits(values[i], sBitsLength(values[i]));
			assert(ret == 0);
		}

		encoded_length = writer.Finish();
		assert(encoded_length != 0);
	}

	// Read
	{
		auto reader = ako::KagariBitReader(encoded_length, buffer);
		for (unsigned i = 0; i < values_no; i += 1)
		{
			uint32_t v = 0;
			const auto ret = reader.ReadBits(sBitsLength(values[i]), v);
			assert(ret == 0);
			assert(v == values[i]);
		}
	}
}


struct CdfEntry
{
	uint16_t value;
	uint16_t frequency;
	uint16_t cumulative;
};

static const CdfEntry s_cdf[255 + 1] = {
    {0, 11240, 0},     {2, 8585, 11240},  {1, 8083, 19825},  {4, 4830, 27908},  {3, 4379, 32738},  {6, 2856, 37117},
    {5, 2681, 39973},  {8, 1927, 42654},  {7, 1812, 44581},  {10, 1412, 46393}, {9, 1329, 47805},  {12, 1093, 49134},
    {11, 1027, 50227}, {14, 873, 51254},  {13, 820, 52127},  {16, 719, 52947},  {15, 674, 53666},  {18, 602, 54340},
    {17, 563, 54942},  {20, 511, 55505},  {19, 479, 56016},  {22, 443, 56495},  {21, 411, 56938},  {24, 383, 57349},
    {23, 356, 57732},  {26, 335, 58088},  {25, 312, 58423},  {28, 295, 58735},  {27, 274, 59030},  {30, 261, 59304},
    {29, 243, 59565},  {32, 233, 59808},  {31, 217, 60041},  {34, 210, 60258},  {33, 194, 60468},  {36, 188, 60662},
    {35, 175, 60850},  {38, 169, 61025},  {37, 158, 61194},  {40, 153, 61352},  {39, 144, 61505},  {42, 139, 61649},
    {41, 132, 61788},  {44, 127, 61920},  {43, 120, 62047},  {46, 116, 62167},  {45, 110, 62283},  {48, 105, 62393},
    {47, 101, 62498},  {50, 96, 62599},   {49, 93, 62695},   {52, 89, 62788},   {51, 86, 62877},   {54, 82, 62963},
    {53, 79, 63045},   {56, 76, 63124},   {55, 74, 63200},   {58, 70, 63274},   {57, 68, 63344},   {60, 64, 63412},
    {59, 63, 63476},   {80, 61, 63539},   {62, 60, 63600},   {61, 59, 63660},   {82, 58, 63719},   {64, 56, 63777},
    {84, 55, 63833},   {63, 54, 63888},   {65, 51, 63942},   {66, 51, 63993},   {86, 51, 64044},   {67, 48, 64095},
    {68, 48, 64143},   {88, 48, 64191},   {112, 46, 64239},  {70, 45, 64285},   {69, 44, 64330},   {90, 44, 64374},
    {71, 42, 64418},   {92, 42, 64460},   {72, 41, 64502},   {116, 41, 64543},  {94, 40, 64584},   {73, 39, 64624},
    {74, 39, 64663},   {75, 37, 64702},   {96, 37, 64739},   {120, 37, 64776},  {76, 36, 64813},   {98, 35, 64849},
    {77, 34, 64884},   {78, 34, 64918},   {100, 33, 64952},  {124, 33, 64985},  {79, 32, 65018},   {102, 31, 65050},
    {104, 30, 65081},  {128, 30, 65111},  {106, 27, 65141},  {108, 27, 65168},  {132, 27, 65195},  {110, 25, 65222},
    {136, 24, 65247},  {140, 22, 65271},  {144, 19, 65293},  {148, 18, 65312},  {176, 18, 65330},  {152, 16, 65348},
    {156, 15, 65364},  {184, 10, 65379},  {160, 1, 65389},   {164, 1, 65390},   {168, 1, 65391},   {172, 1, 65392},
    {192, 1, 65393},   {200, 1, 65394},   {208, 1, 65395},   {216, 1, 65396},   {224, 1, 65397},   {232, 1, 65398},
    {240, 1, 65399},   {248, 1, 65400},   {256, 1, 65401},   {264, 1, 65402},   {272, 1, 65403},   {280, 1, 65404},
    {288, 1, 65405},   {296, 1, 65406},   {304, 1, 65407},   {320, 1, 65408},   {336, 1, 65409},   {352, 1, 65410},
    {368, 1, 65411},   {384, 1, 65412},   {400, 1, 65413},   {416, 1, 65414},   {432, 1, 65415},   {448, 1, 65416},
    {464, 1, 65417},   {480, 1, 65418},   {496, 1, 65419},   {512, 1, 65420},   {528, 1, 65421},   {544, 1, 65422},
    {560, 1, 65423},   {592, 1, 65424},   {624, 1, 65425},   {656, 1, 65426},   {688, 1, 65427},   {720, 1, 65428},
    {752, 1, 65429},   {784, 1, 65430},   {816, 1, 65431},   {848, 1, 65432},   {880, 1, 65433},   {912, 1, 65434},
    {944, 1, 65435},   {976, 1, 65436},   {1008, 1, 65437},  {1040, 1, 65438},  {1072, 1, 65439},  {1136, 1, 65440},
    {1200, 1, 65441},  {1264, 1, 65442},  {1328, 1, 65443},  {1392, 1, 65444},  {1456, 1, 65445},  {1520, 1, 65446},
    {1584, 1, 65447},  {1648, 1, 65448},  {1712, 1, 65449},  {1776, 1, 65450},  {1840, 1, 65451},  {1904, 1, 65452},
    {1968, 1, 65453},  {2032, 1, 65454},  {2096, 1, 65455},  {2224, 1, 65456},  {2352, 1, 65457},  {2480, 1, 65458},
    {2608, 1, 65459},  {2736, 1, 65460},  {2864, 1, 65461},  {2992, 1, 65462},  {3120, 1, 65463},  {3248, 1, 65464},
    {3376, 1, 65465},  {3504, 1, 65466},  {3632, 1, 65467},  {3760, 1, 65468},  {3888, 1, 65469},  {4016, 1, 65470},
    {4144, 1, 65471},  {4400, 1, 65472},  {4656, 1, 65473},  {4912, 1, 65474},  {5168, 1, 65475},  {5424, 1, 65476},
    {5680, 1, 65477},  {5936, 1, 65478},  {6192, 1, 65479},  {6448, 1, 65480},  {6704, 1, 65481},  {6960, 1, 65482},
    {7216, 1, 65483},  {7472, 1, 65484},  {7728, 1, 65485},  {7984, 1, 65486},  {8240, 1, 65487},  {8752, 1, 65488},
    {9264, 1, 65489},  {9776, 1, 65490},  {10288, 1, 65491}, {10800, 1, 65492}, {11312, 1, 65493}, {11824, 1, 65494},
    {12336, 1, 65495}, {12848, 1, 65496}, {13360, 1, 65497}, {13872, 1, 65498}, {14384, 1, 65499}, {14896, 1, 65500},
    {15408, 1, 65501}, {15920, 1, 65502}, {16432, 1, 65503}, {17456, 1, 65504}, {18480, 1, 65505}, {19504, 1, 65506},
    {20528, 1, 65507}, {21552, 1, 65508}, {22576, 1, 65509}, {23600, 1, 65510}, {24624, 1, 65511}, {25648, 1, 65512},
    {26672, 1, 65513}, {27696, 1, 65514}, {28720, 1, 65515}, {29744, 1, 65516}, {30768, 1, 65517}, {31792, 1, 65518},
    {32816, 1, 65519}, {34864, 1, 65520}, {36912, 1, 65521}, {38960, 1, 65522}, {41008, 1, 65523}, {43056, 1, 65524},
    {45104, 1, 65525}, {47152, 1, 65526}, {49200, 1, 65527}, {51248, 1, 65528}, {53296, 1, 65529}, {55344, 1, 65530},
    {57392, 1, 65531}, {59440, 1, 65532}, {61488, 1, 65533}, {63536, 1, 65534}};


static uint32_t sRandomWaveletDistribution(uint32_t* state)
{
	// Not really a "WaveletDistribution" as frequencies were taken after an Rle compression,
	// and with Rle instructions mixed there as well (so it is more an "AkoDistribution")

	// Generate a random index
	// https://en.wikipedia.org/wiki/Xorshift#Example_implementation
	uint32_t x = *state;
	x ^= static_cast<uint32_t>(x << 13);
	x ^= static_cast<uint32_t>(x >> 17);
	x ^= static_cast<uint32_t>(x << 5);
	*state = x;

	// Use index as cumulative
	const auto c = ((x & 0xFFFF) >> 1) + (0xFFFF >> 1); // Starts from mid
	uint16_t ret = s_cdf[255].value;

	for (int i = 0; i < 255; i += 1)
	{
		if (c < s_cdf[i + 1].cumulative)
		{
			ret = s_cdf[i].value;
			break;
		}
	}

	return ret;
}


static void sRandomTest(unsigned values_no, uint32_t seed = 4321)
{
	printf("RandomTest, len: %u, seed: %u\n", values_no, seed);

	auto values = reinterpret_cast<uint32_t*>(malloc(sizeof(uint32_t) * values_no));
	auto buffer = reinterpret_cast<uint32_t*>(malloc(sizeof(uint32_t) * values_no));
	assert(values != nullptr);
	assert(buffer != nullptr);

	// Manufacture values
	for (unsigned i = 0; i < values_no; i += 1)
		values[i] = sRandomWaveletDistribution(&seed) & 2147483647; // 32 bits maximum

	// Write
	size_t encoded_length = 0;
	{
		auto writer = ako::KagariBitWriter(values_no, buffer);
		for (unsigned i = 0; i < values_no; i += 1)
		{
			const auto ret = writer.WriteBits(values[i], sBitsLength(values[i]));
			assert(ret == 0);
		}

		encoded_length = writer.Finish();
		assert(encoded_length != 0);
	}

	// Read
	{
		auto reader = ako::KagariBitReader(encoded_length, buffer);
		for (unsigned i = 0; i < values_no; i += 1)
		{
			uint32_t v = 0;
			const auto ret = reader.ReadBits(sBitsLength(values[i]), v);
			assert(ret == 0);
			assert(v == values[i]);
		}
	}

	free(values);
	free(buffer);
}


int main(int argc, const char* argv[])
{
	(void)argc;
	(void)argv;

	printf("# Kagari Bits Test (Ako v%i.%i.%i, %s)\n", ako::VersionMajor(), ako::VersionMinor(), ako::VersionPatch(),
	       (ako::SystemEndianness() == ako::Endianness::Little) ? "little-endian" : "big-endian");


	sFixedTest();
	sRandomTest(4096);

	return EXIT_SUCCESS;
}
