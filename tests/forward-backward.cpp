

#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ako-private.hpp"
#include "ako.hpp"


using namespace ako;


#define MAX_TILE_INFO 1024

struct GatheredTileInfo
{
	unsigned width;
	unsigned height;
	unsigned x;
	unsigned y;

	size_t data_size;
};

struct GatheredInfo
{
	// Provided by the library
	unsigned image_width;
	unsigned image_height;
	unsigned channels;
	unsigned depth;

	unsigned tiles_no;
	unsigned tiles_dimension;

	size_t workarea_size;

	GatheredTileInfo tile[MAX_TILE_INFO];

	// Made by us
	unsigned max_tile_index;
};

GatheredInfo g_encoded_info = {};
GatheredInfo g_decoded_info = {};


static void sCallbackGenericEvent(ako::GenericEvent e, unsigned a, unsigned b, unsigned c, size_t d, void* user_data)
{
	auto& data = *reinterpret_cast<GatheredInfo*>(user_data);

	switch (e)
	{
	case ako::GenericEvent::ImageDimensions:
		data.image_width = a;
		data.image_height = b;
		break;

	case ako::GenericEvent::ImageChannels: data.channels = a; break;
	case ako::GenericEvent::ImageDepth: data.depth = a; break;
	case ako::GenericEvent::TilesNo: data.tiles_no = a; break;
	case ako::GenericEvent::TilesDimension: data.tiles_dimension = a; break;
	case ako::GenericEvent::WorkareaSize: data.workarea_size = d; break;

	case ako::GenericEvent::TileDimensions:
		assert(a < MAX_TILE_INFO);
		data.max_tile_index = Max(data.max_tile_index, a);
		data.tile[a].width = b;
		data.tile[a].height = c;
		break;
	case ako::GenericEvent::TilePosition:
		assert(a < MAX_TILE_INFO);
		data.max_tile_index = Max(data.max_tile_index, a);
		data.tile[a].x = b;
		data.tile[a].y = c;
		break;
	case ako::GenericEvent::TileDataSize:
		assert(a < MAX_TILE_INFO);
		data.max_tile_index = Max(data.max_tile_index, a);
		data.tile[a].data_size = d;
		break;
	}
}


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


static uint32_t sAdler32(const void* input, size_t data_size)
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

	while (data_size != 0)
	{
		const auto amount = (data_size > 5552) ? 5552 : data_size;
		data_size -= amount;

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


static void sTest(const char* out_filename, const Settings& settings, unsigned width, unsigned height,
                  unsigned channels, unsigned depth)
{
	printf(" - %ux%u px, %u channel(s), %u bpp\n", width, height, channels, depth);

	// Alloc data
	const auto data_size = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(channels) *
	                       ((depth <= 8) ? 1 : 2);

	auto data = static_cast<uint8_t*>(malloc(data_size));
	assert(data != nullptr);
	memset(data, 0, data_size);

	// Encode
	void* encoded_blob = nullptr;
	size_t encoded_blob_size = 0;
	{
		auto callbacks = DefaultCallbacks();
		callbacks.generic_event = sCallbackGenericEvent;
		callbacks.user_data = &g_encoded_info;

		auto status = Status::Error; // Assume error
		encoded_blob_size = EncodeEx(callbacks, settings, width, height, channels, depth, data, &encoded_blob, &status);

		if (encoded_blob_size == 0)
			printf("Ako encode error: '%s'\n", ToString(status));

		// Checks
		assert(encoded_blob_size != 0);
		assert(status != Status::Error); // Library should never return it, too vague
	}

#if !defined(_WIN64) && !defined(_WIN32)
	// Save
	{
		auto fp = fopen(out_filename, "wb");
		assert(fp != nullptr);

		const auto n = fwrite(encoded_blob, encoded_blob_size, 1, fp);
		assert(n == 1);
		fclose(fp);

		// Checksum
		const auto hash = sAdler32(encoded_blob, encoded_blob_size);
		printf("     Encoded blob hash: 0x%08x\n", hash); // Faster than use an hex viewer
	}
#endif

	// Decode
	void* decoded_data = nullptr;
	{
		auto callbacks = DefaultCallbacks();
		callbacks.generic_event = sCallbackGenericEvent;
		callbacks.user_data = &g_decoded_info;

		auto status = Status::Error; // Assume error
		Settings decoded_settings = {};
		unsigned decoded_width = 0;
		unsigned decoded_height = 0;
		unsigned decoded_channels = 0;
		unsigned decoded_depth = 0;

		decoded_data = DecodeEx(callbacks, encoded_blob_size, encoded_blob, decoded_width, decoded_height,
		                        decoded_channels, decoded_depth, &decoded_settings, &status);

		if (decoded_data == 0)
			printf("Ako decode error: '%s'\n", ToString(status));

		// Checks
		assert(decoded_data != nullptr);
		assert(status != Status::Error);

		assert(decoded_width == width);
		assert(decoded_height == height);
		assert(decoded_channels == channels);
		assert(decoded_depth == depth);

		assert(decoded_settings.color == settings.color);
		assert(decoded_settings.wavelet == settings.wavelet);
		assert(decoded_settings.wrap == settings.wrap);
		assert(decoded_settings.compression == settings.compression);
		assert(decoded_settings.tiles_dimension == settings.tiles_dimension);
	}

	// Compare callbacks information
	{
		// Following numbers can differ, mostly, because endianness issues, as they are transmitted on the image head
		assert(g_encoded_info.image_width == g_decoded_info.image_width);
		assert(g_encoded_info.image_height == g_decoded_info.image_height);
		assert(g_encoded_info.channels == g_decoded_info.channels);
		assert(g_encoded_info.depth == g_decoded_info.depth);
		assert(g_encoded_info.tiles_dimension == g_decoded_info.tiles_dimension);

		// If some of above happens, the encoder/decoder may allocate wrong amounts of memory, and/or process
		// non-existing tiles (Valgrind may also warn us if this happens)
		assert(g_encoded_info.workarea_size == g_decoded_info.workarea_size);
		assert(g_encoded_info.tiles_no == g_decoded_info.tiles_no);

		// Now with tiles
		assert(g_encoded_info.max_tile_index == g_decoded_info.max_tile_index);
		for (size_t i = 0; i < g_encoded_info.max_tile_index; i++) // Yes, ignoring 'tiles_no'
		{
			// Bad maths, since all information here isn't transmitted in the file itself, but calculated
			assert(g_encoded_info.tile[i].width == g_decoded_info.tile[i].width);
			assert(g_encoded_info.tile[i].height == g_decoded_info.tile[i].height);
			assert(g_encoded_info.tile[i].x == g_decoded_info.tile[i].x);
			assert(g_encoded_info.tile[i].y == g_decoded_info.tile[i].y);
			assert(g_encoded_info.tile[i].data_size == g_decoded_info.tile[i].data_size);

			// Tile data size should fit inside a workarea (Valgrind also detects this one)
			assert(g_encoded_info.tile[i].data_size <= g_decoded_info.workarea_size);
			assert(g_encoded_info.tile[i].data_size <= g_decoded_info.workarea_size);
		}
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

	auto settings = DefaultSettings();
	sTest("/tmp/out1.ako", settings, 1, 1, 1, 8);
	sTest("/tmp/out2.ako", settings, 1, 1, 1, 16);
	sTest("/tmp/out3.ako", settings, 640, 480, 3, 8);
	sTest("/tmp/out4.ako", settings, 640, 480, 3, 16);

	settings.tiles_dimension = 256;
	sTest("/tmp/out5.ako", settings, 128, 128, 1, 8);
	sTest("/tmp/out6.ako", settings, 256, 256, 1, 8);

	settings.tiles_dimension = 1024;
	sTest("/tmp/out7.ako", settings, 1490, 900, 2, 16);
	settings.tiles_dimension = 512;
	sTest("/tmp/out8.ako", settings, 1490, 900, 2, 16);
	settings.tiles_dimension = 256;
	sTest("/tmp/out9.ako", settings, 1490, 900, 2, 16);
	settings.tiles_dimension = 128;
	sTest("/tmp/out10.ako", settings, 1490, 900, 2, 16);

	return EXIT_SUCCESS;
}
