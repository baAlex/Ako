

#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ako-private.hpp"
#include "ako.hpp"

#include "decode/compression-kagari.hpp"
#include "encode/compression-kagari.hpp"


template <typename T> static inline T sQuantizer(float q, T value)
{
	(void)q;
	return value;
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
	printf(" - File Test, filename '%s'\n", filename);

	FILE* fp = fopen(filename, "rb");
	assert(fp != nullptr);

	// Get filesize
	fseek(fp, 0, SEEK_END);
	const auto size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	// Allocate buffers
	void* big_buffer = malloc(static_cast<size_t>(size));
	void* tiny_buffer = malloc(sizeof(int16_t) * 256);
	assert(big_buffer != nullptr);
	assert(tiny_buffer != nullptr);

	// Compress input, processing it in blocks
	uint32_t input_data_hash = 0x87654321;
	size_t compressed_size = 0;
	{
		auto compressor = ako::CompressorKagari<int16_t>(256, big_buffer, static_cast<size_t>(size));
		size_t blocks_no = 0;
		while (1)
		{
			const auto read_length = fread(tiny_buffer, sizeof(int16_t), 256, fp);
			if (read_length != 256) // Stop here, so no read is <256 length
				break;

			if (compressor.Step(sQuantizer, 1.0F, static_cast<unsigned>(read_length), 1,
			                    reinterpret_cast<int16_t*>(tiny_buffer)) != 0)
			{
				printf("Nope, try with a more quantized image\n");
				assert(1 == 0); // NOLINT misc-static-assert
			}

			input_data_hash ^= sAdler32(tiny_buffer, sizeof(int16_t) * read_length); // I know, a bad way of mix hashes
			blocks_no++;
		}

		compressed_size = compressor.Finish();
		assert(compressed_size != 0);

		printf("\t%zu, %x, %.2f\n", blocks_no, input_data_hash, static_cast<float>(compressed_size) / 1024.0F);
	}

	// Decompress, same intricacies
	uint32_t decoded_data_hash = 0x87654321;
	{
		auto decompressor = ako::DecompressorKagari<int16_t>(256, big_buffer, compressed_size);
		size_t blocks_no = 0;
		while (1)
		{
			if (decompressor.Step(256, 1, reinterpret_cast<int16_t*>(tiny_buffer)) != ako::Status::Ok)
				break;

			decoded_data_hash ^= sAdler32(tiny_buffer, sizeof(int16_t) * 256);
			blocks_no++;
		}

		printf("\t%zu, %x\n", blocks_no, decoded_data_hash);
		assert(input_data_hash == decoded_data_hash);
	}

	// Bye!
	fclose(fp);
	free(big_buffer);
	free(tiny_buffer);
}


template <typename T> static void sTest(size_t block_size, const char* input)
{
	const size_t input_length = strlen(input);
	const size_t buffer_size_a = input_length * sizeof(T);
	const size_t buffer_size_b = (input_length + 1) * sizeof(uint32_t); // WARNING, uint32 is the encoder internal type

	printf(" - Rle Test, length: %zu, input: '%s'\n", input_length, input);

	// Allocate buffers
	auto buffer_a = reinterpret_cast<T*>(malloc(buffer_size_a));
	auto buffer_b = reinterpret_cast<T*>(malloc(buffer_size_b));
	assert(buffer_a != nullptr);
	assert(buffer_b != nullptr);

	// Convert data
	for (size_t i = 0; i < input_length; i += 1)
		buffer_a[i] = static_cast<T>(static_cast<unsigned char>(input[i]));

	// """"Compress""""
	size_t compressed_size = 0;
	{
		auto compressor = ako::CompressorKagari<T>(block_size, buffer_b, buffer_size_b);

		assert(compressor.Step(sQuantizer, 1.0F, static_cast<unsigned>(input_length), 1, buffer_a) == 0);
		compressed_size = compressor.Finish();
		assert(compressed_size != 0);

		memset(buffer_a, 0, buffer_size_a);
	}

	// """"Decompress""""
	{
		auto decompressor = ako::DecompressorKagari<T>(block_size, buffer_b, compressed_size);

		assert(decompressor.Step(static_cast<unsigned>(input_length), 1, buffer_a) == ako::Status::Ok);
		// decompressor.Finish();
	}

	// Bye!
	free(buffer_a);
	free(buffer_b);
}


int main(int argc, const char* argv[])
{
	printf("# Kagari Test (Ako v%i.%i.%i, %s)\n", ako::VersionMajor(), ako::VersionMinor(), ako::VersionPatch(),
	       (ako::SystemEndianness() == ako::Endianness::Little) ? "little-endian" : "big-endian");

	if (argc > 1)
	{
		sTestFile(argv[1]);
		return EXIT_SUCCESS;
	}

	const size_t BLOCK_SIZE = 25;

	sTest<int16_t>(BLOCK_SIZE, "123456");       // Literal only
	sTest<int16_t>(BLOCK_SIZE, "111123456666"); // Literal only, since doesn't meets a minimum for a Rle
	sTest<int16_t>(BLOCK_SIZE, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"); // Rle
	sTest<int16_t>(BLOCK_SIZE, "aaaaaaaaaaaabcdefgaaaaaaaaaaaaaa"); // Both
	sTest<int16_t>(BLOCK_SIZE, "11111111222222223333333344444444555555556666666677777777");
	sTest<int16_t>(BLOCK_SIZE, "13525465112222221566664441111223333452123456666666111101"); // Case ending in a literal

	return EXIT_SUCCESS;
}
