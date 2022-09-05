

#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ako-private.hpp"
#include "ako.hpp"


using namespace ako;


/*static uint32_t sLfsr32(uint32_t& state)
{
    // https://en.wikipedia.org/wiki/Xorshift#Example_implementation
    uint32_t x = state;
    x ^= static_cast<uint32_t>(x << 13);
    x ^= static_cast<uint32_t>(x >> 17);
    x ^= static_cast<uint32_t>(x << 5);
    state = x;

    return x;
}*/


static uint32_t sAdler32(const void* input, size_t size)
{
	// Function borrowed from LodePNG, modified.

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

	while (size != 0)
	{
		const auto amount = (size > 5552) ? 5552 : size;
		size -= amount;

		for (size_t i = 0; i != amount; ++i)
		{
			s1 += (*in++);
			s2 += s1;
		}

		s1 %= 65521;
		s2 %= 65521;
	}

	return (s2 << 16) | s1;
}


static void sEncode(const char* out_filename, unsigned width, unsigned height, unsigned channels, unsigned depth)
{
	printf(" - %ux%u px, %u channel(s), %u bpp\n", width, height, channels, depth);

	// Alloc data
	const auto data_size = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(channels) *
	                       ((depth <= 8) ? 1 : 2);

	auto data = static_cast<uint8_t*>(malloc(data_size));
	assert(data != NULL);
	memset(data, 0, data_size);

	// Encode
	void* encoded_blob = NULL;
	size_t encoded_blob_size = 0;
	{
		Status status = Status::Error; // Assume error
		encoded_blob_size = EncodeEx(DefaultCallbacks(), DefaultSettings(), width, height, channels, depth, data,
		                             &encoded_blob, status);

		if (encoded_blob_size == 0)
			printf("Ako encode error: '%s'\n", ToString(status));

		// Checks
		assert(encoded_blob_size != 0);
		assert(status != Status::Error); // Library should never return it, too vague
	}

#if !defined(_WIN64) && !defined(_WIN32)
	// Save
	{
		FILE* fp = fopen(out_filename, "wb");
		assert(fp != NULL);

		const auto n = fwrite(encoded_blob, encoded_blob_size, 1, fp);
		assert(n == 1);
		fclose(fp);

		// Checksum
		const auto hash = sAdler32(encoded_blob, encoded_blob_size);
		printf("     Encoded blob hash: 0x%08x\n", hash);
	}
#endif

	// Decode
	void* decoded_data = NULL;
	{
		Status status = Status::Error; // Assume error
		Settings settings = {};
		unsigned decoded_width = 0;
		unsigned decoded_height = 0;
		unsigned decoded_channels = 0;
		unsigned decoded_depth = 0;

		decoded_data = DecodeEx(DefaultCallbacks(), encoded_blob_size, encoded_blob, settings, decoded_width,
		                        decoded_height, decoded_channels, decoded_depth, status);

		if (decoded_data == 0)
			printf("Ako decode error: '%s'\n", ToString(status));

		// Checks
		assert(decoded_data != NULL);
		assert(status != Status::Error);

		assert(decoded_width == width);
		assert(decoded_height == height);
		assert(decoded_channels == channels);
		assert(decoded_depth == depth);
	}

	// Bye!
	DefaultCallbacks().free(encoded_blob);
	DefaultCallbacks().free(decoded_data);
	free(data);
}


int main()
{
	printf("# ForwardBackward (Ako v%i.%i.%i, %s)\n", VersionMajor(), VersionMinor(), VersionPatch(),
	       (SystemEndianness() == Endianness::Little) ? "little-endian" : "big-endian");

	sEncode("/tmp/out1.ako", 1, 1, 1, 8);
	sEncode("/tmp/out2.ako", 1, 1, 1, 16);

	sEncode("/tmp/out3.ako", 640, 480, 3, 8);
	sEncode("/tmp/out4.ako", 640, 480, 3, 16);

	return EXIT_SUCCESS;
}
