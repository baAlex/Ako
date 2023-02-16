

#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ako-private.hpp"
#include "ako.hpp"

#include "decode/compression-kagari.hpp"
#include "encode/compression-kagari.hpp"


static void sQuantizer(float q, unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                       const int16_t* in, int16_t* out)
{
	(void)q;
	for (unsigned row = 0; row < height; row += 1)
	{
		for (unsigned col = 0; col < width; col += 1)
			out[col] = in[col];
		in += input_stride;
		out += output_stride;
	}
}


static uint32_t sRandom32(uint32_t* state)
{
	// https://en.wikipedia.org/wiki/Xorshift#Example_implementation
	auto x = *state;
	x ^= static_cast<uint32_t>(x << 13);
	x ^= static_cast<uint32_t>(x >> 17);
	x ^= static_cast<uint32_t>(x << 5);
	*state = x;

	return x;
}


static uint32_t sAdler32(const void* input, size_t input_size)
{
	// Borrowed from LodePNG, modified.

	/*
	Copyright (c) 2005-2018 Lode Vandevenne

	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	    1. The origin of this software must not be misrepresented; you must not
	    claim that you wrote the original software. If you use this software
	    in a product, an acknowledgment in the product documentation would be
	    appreciated but is not required.

	    2. Altered source versions must be plainly marked as such, and must not be
	    misrepresented as being the original software.

	    3. This notice may not be removed or altered from any source
	    distribution.
	*/

	auto in = reinterpret_cast<const uint8_t*>(input);
	uint32_t s1 = 1;
	uint32_t s2 = 0;

	while (input_size != 0)
	{
		const auto amount = (input_size > 5552) ? 5552 : input_size;
		input_size -= amount;

		for (size_t i = 0; i != amount; i += 1)
		{
			s1 += (*in++);
			s2 += s1;
		}

		s1 %= 65521;
		s2 %= 65521;
	}

	return (s2 << 16) | s1;
}


static void sTestFile(const char* filename)
{
	const unsigned BLOCK_LENGTH = 256 * 256;
	const unsigned READS_MAX_LENGTH = (256 * 256) * 2;

	printf(" - File Test, filename '%s'\n", filename);

	FILE* fp = fopen(filename, "rb");
	assert(fp != nullptr);

	// Get filesize
	fseek(fp, 0, SEEK_END);
	const auto filesize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	// Allocate buffers
	auto big_buffer = malloc(static_cast<size_t>(filesize));
	auto tiny_buffer = reinterpret_cast<int16_t*>(malloc(sizeof(int16_t) * static_cast<size_t>(READS_MAX_LENGTH)));
	assert(big_buffer != nullptr);
	assert(tiny_buffer != nullptr);

	// Compress input
	uint32_t input_data_hash = 0x12345678;
	size_t compressed_size = 0;
	{
		auto compressor = ako::CompressorKagari(BLOCK_LENGTH, 1, static_cast<size_t>(filesize), big_buffer);
		uint32_t random_state = 1;
		size_t buffers_no = 0;

		while (1)
		{
			// Read a chunk of variable length
			const auto chunk_length = (sRandom32(&random_state) % (READS_MAX_LENGTH - 1)) + 1;
			if (fread(tiny_buffer, sizeof(int16_t), chunk_length, fp) != chunk_length)
				break;

			// Compress it
			if (compressor.Step(sQuantizer, 1.0F, chunk_length, 1, tiny_buffer) != 0)
			{
				printf("Nope, try with a more quantized image\n");
				assert(1 == 0); // NOLINT misc-static-assert
			}

			// Keep information
			input_data_hash ^= sAdler32(tiny_buffer, sizeof(int16_t) * chunk_length); // I know, a bad way of mix hashes
			buffers_no += 1;
		}

		// Done
		compressed_size = compressor.Finish();
		printf("\t%zu, %x, %.2f kB\n", buffers_no, input_data_hash, static_cast<float>(compressed_size) / 1000.0F);
		assert(compressed_size != 0);
	}

	// Decompress, same intricacies
	uint32_t decoded_data_hash = 0x12345678;
	{
		auto decompressor = ako::DecompressorKagari(BLOCK_LENGTH, 1, compressed_size, big_buffer);
		uint32_t random_state = 1;
		size_t buffers_no = 0;

		while (1)
		{
			// Decompress a chunk of variable length
			const auto chunk_length = (sRandom32(&random_state) % (READS_MAX_LENGTH - 1)) + 1;

			if (decompressor.Step(chunk_length, 1, tiny_buffer) != ako::Status::Ok)
				break;

			// Keep information
			decoded_data_hash ^= sAdler32(tiny_buffer, sizeof(int16_t) * chunk_length);
			buffers_no += 1;
		}

		// Done
		printf("\t%zu, %x\n", buffers_no, decoded_data_hash);
		assert(input_data_hash == decoded_data_hash);
	}

	// Bye!
	fclose(fp);
	free(big_buffer);
	free(tiny_buffer);
}


static void sTest(unsigned block_width, unsigned block_height, const char* input)
{
	const size_t input_length = strlen(input);
	const size_t buffer_size_a = input_length * sizeof(int16_t);
	const size_t buffer_size_b = buffer_size_a * 2; // x2 since compression can inflate size

	printf(" - Rle Test, length: %zu, input: '%s'\n", input_length, input);

	// Allocate buffers
	auto buffer_a = reinterpret_cast<int16_t*>(malloc(buffer_size_a));
	auto buffer_b = reinterpret_cast<int16_t*>(malloc(buffer_size_b));
	assert(buffer_a != nullptr);
	assert(buffer_b != nullptr);

	// Convert data
	for (size_t i = 0; i < input_length; i += 1)
		buffer_a[i] = static_cast<int16_t>(static_cast<unsigned char>(input[i]));

	// Compress
	size_t compressed_size = 0;
	{
		auto compressor = ako::CompressorKagari(block_width, block_height, buffer_size_b, buffer_b);

		assert(compressor.Step(sQuantizer, 1.0F, static_cast<unsigned>(input_length), 1, buffer_a) == 0);
		compressed_size = compressor.Finish();
		assert(compressed_size != 0);

		memset(buffer_a, 0, buffer_size_a);
	}

	// Decompress
	{
		auto decompressor = ako::DecompressorKagari(block_width, block_height, compressed_size, buffer_b);
		assert(decompressor.Step(static_cast<unsigned>(input_length), 1, buffer_a) == ako::Status::Ok);
	}

	// Compare
	for (size_t i = 0; i < input_length; i += 1)
		assert(buffer_a[i] == static_cast<int16_t>(static_cast<unsigned char>(input[i])));

	// Bye!
	free(buffer_a);
	free(buffer_b);
}


int main(int argc, const char* argv[])
{
	(void)argc;
	(void)argv;

	printf("# Kagari Test (Ako v%i.%i.%i, %s)\n", ako::VersionMajor(), ako::VersionMinor(), ako::VersionPatch(),
	       (ako::SystemEndianness() == ako::Endianness::Little) ? "little-endian" : "big-endian");

	if (argc > 1)
	{
		sTestFile(argv[1]);
		return EXIT_SUCCESS;
	}

	const size_t BLOCK_LENGTH = 16;

	// sTest(BLOCK_LENGTH, 1, "123456");       // Literal only
	sTest(BLOCK_LENGTH, 1, "111123456666"); // Literal only, since doesn'int16_t meets a minimum for a Rle
	sTest(BLOCK_LENGTH, 1, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"); // Rle
	sTest(BLOCK_LENGTH, 1, "aaaaaaaaaaaabcdefgaaaaaaaaaaaaaa"); // Both
	sTest(BLOCK_LENGTH, 1, "11111111222222223333333344444444555555556666666677777777");
	sTest(BLOCK_LENGTH, 1, "13525465112222221566664441111223333452123456666666111101"); // Case ending in a literal

	return EXIT_SUCCESS;
}
