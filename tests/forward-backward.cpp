

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

static GatheredInfo s_encoded_info = {};
static GatheredInfo s_decoded_info = {};


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


template <typename T>
static void sTest(const char* out_filename, const Settings& settings, unsigned width, unsigned height,
                  unsigned channels, unsigned depth, T (*data_generator)(unsigned, uint32_t&))
{
	printf(" - %ux%u px, %u channel(s), %u bpp\n", width, height, channels, depth);

	// Alloc data
	const auto data_size = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(channels) *
	                       ((depth <= 8) ? 1 : 2);

	auto data = reinterpret_cast<uint8_t*>(malloc(data_size));
	assert(data != nullptr);

	// Generate data
	{
		uint32_t generator_state = 1;
		T* pixel_component = reinterpret_cast<T*>(data);

		for (unsigned i = 0; i < (width * height * channels); i += 1, pixel_component += 1)
			*pixel_component = data_generator(i, generator_state);
	}

	// Encode
	void* encoded_blob = nullptr;
	size_t encoded_blob_size = 0;
	{
		auto callbacks = DefaultCallbacks();
		callbacks.generic_event = sCallbackGenericEvent;
		callbacks.user_data = &s_encoded_info;

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
		const auto hash = Adler32(encoded_blob, encoded_blob_size);
		printf("     Encoded blob hash: 0x%08x\n", hash); // Faster than use an hex viewer
	}
#endif

	// Decode
	void* decoded_data = nullptr;
	{
		auto callbacks = DefaultCallbacks();
		callbacks.generic_event = sCallbackGenericEvent;
		callbacks.user_data = &s_decoded_info;

		auto status = Status::Error; // Assume error
		Settings decoded_settings = {};
		unsigned decoded_width = 0;
		unsigned decoded_height = 0;
		unsigned decoded_channels = 0;
		unsigned decoded_depth = 0;

		decoded_data = DecodeEx(callbacks, encoded_blob_size, encoded_blob, decoded_width, decoded_height,
		                        decoded_channels, decoded_depth, &decoded_settings, &status);

		if (decoded_data == nullptr)
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
		assert(s_encoded_info.image_width == s_decoded_info.image_width);
		assert(s_encoded_info.image_height == s_decoded_info.image_height);
		assert(s_encoded_info.channels == s_decoded_info.channels);
		assert(s_encoded_info.depth == s_decoded_info.depth);
		assert(s_encoded_info.tiles_dimension == s_decoded_info.tiles_dimension);

		// If some of above happens, the encoder/decoder may allocate wrong amounts of memory, and/or process
		// non-existing tiles (Valgrind may also warn us if this happens)
		assert(s_encoded_info.workarea_size == s_decoded_info.workarea_size);
		assert(s_encoded_info.tiles_no == s_decoded_info.tiles_no);

		// Now with tiles
		assert(s_encoded_info.max_tile_index == s_decoded_info.max_tile_index);
		for (size_t i = 0; i < s_encoded_info.max_tile_index; i += 1) // Yes, ignoring 'tiles_no'
		{
			// Bad maths, since all information here isn't transmitted in the file itself, but calculated
			assert(s_encoded_info.tile[i].width == s_decoded_info.tile[i].width);
			assert(s_encoded_info.tile[i].height == s_decoded_info.tile[i].height);
			assert(s_encoded_info.tile[i].x == s_decoded_info.tile[i].x);
			assert(s_encoded_info.tile[i].y == s_decoded_info.tile[i].y);
			assert(s_encoded_info.tile[i].data_size == s_decoded_info.tile[i].data_size);

			// Tile data size should fit inside a workarea (Valgrind also detects this one)
			assert(s_encoded_info.tile[i].data_size <= s_decoded_info.workarea_size);
			assert(s_encoded_info.tile[i].data_size <= s_decoded_info.workarea_size);
		}
	}

	// Bye!
	DefaultCallbacks().free(encoded_blob);
	DefaultCallbacks().free(decoded_data);
	free(data);
}


template <typename T> T DataGenerator(unsigned i, uint32_t& inout_state)
{
	// https://en.wikipedia.org/wiki/Xorshift#Example_implementation
	uint32_t x = inout_state;
	x ^= static_cast<uint32_t>(x << 13);
	x ^= static_cast<uint32_t>(x >> 17);
	x ^= static_cast<uint32_t>(x << 5);
	inout_state = x;

	return static_cast<T>((i + 1) * 10 + (inout_state & 16));
}


int main()
{
	printf("# ForwardBackward (Ako v%i.%i.%i, %s)\n", VersionMajor(), VersionMinor(), VersionPatch(),
	       (SystemEndianness() == Endianness::Little) ? "little-endian" : "big-endian");

	auto settings = DefaultSettings();
	sTest("/tmp/1x1x1-8bpp.ako", settings, 1, 1, 1, 8, DataGenerator<uint8_t>);
	sTest("/tmp/640x480x3-8bpp.ako", settings, 640, 480, 3, 8, DataGenerator<uint8_t>);
	sTest("/tmp/1280x720x4-8bpp.ako", settings, 1280, 720, 4, 8, DataGenerator<uint8_t>);

	sTest("/tmp/1x1x1-16bpp.ako", settings, 1, 1, 1, 16, DataGenerator<uint16_t>);
	sTest("/tmp/640x480x3-16bpp.ako", settings, 640, 480, 3, 16, DataGenerator<uint16_t>);
	sTest("/tmp/1280x720x4-16bpp.ako", settings, 1280, 720, 4, 16, DataGenerator<uint16_t>);

	/*settings.tiles_dimension = 256;
	sTest("/tmp/out5.ako", settings, 128, 128, 1, 8);
	sTest("/tmp/out6.ako", settings, 256, 256, 1, 8);

	settings.tiles_dimension = 1024;
	sTest("/tmp/out7.ako", settings, 1490, 900, 2, 16);
	settings.tiles_dimension = 512;
	sTest("/tmp/out8.ako", settings, 1490, 900, 2, 16);
	settings.tiles_dimension = 256;
	sTest("/tmp/out9.ako", settings, 1490, 900, 2, 16);
	settings.tiles_dimension = 128;
	sTest("/tmp/out10.ako", settings, 1490, 900, 2, 16);*/

	return EXIT_SUCCESS;
}
