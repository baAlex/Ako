

#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ako-private.hpp"
#include "ako.hpp"


const unsigned MAX_TILE_INFO = 1024;


struct GatheredTileInfo
{
	unsigned width;
	unsigned height;
	unsigned x;
	unsigned y;

	size_t data_size;
};

struct GatheredGeneralInfo
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

static GatheredGeneralInfo s_encoded_info = {};
static GatheredGeneralInfo s_decoded_info = {};


static void sCallbackGenericEvent(ako::GenericEvent e, unsigned arg_a, unsigned arg_b, unsigned arg_c,
                                  ako::GenericType arg_d, void* user_data)
{
	auto& data = *reinterpret_cast<GatheredGeneralInfo*>(user_data);

	switch (e)
	{
	case ako::GenericEvent::ImageDimensions:
		data.image_width = arg_a;
		data.image_height = arg_b;
		break;

	case ako::GenericEvent::ImageChannels: data.channels = arg_a; break;
	case ako::GenericEvent::ImageDepth: data.depth = arg_a; break;
	case ako::GenericEvent::TilesNo: data.tiles_no = arg_a; break;
	case ako::GenericEvent::TilesDimension: data.tiles_dimension = arg_a; break;
	case ako::GenericEvent::WorkareaSize: data.workarea_size = arg_d.u; break;

	case ako::GenericEvent::TileDimensions:
		assert(arg_a < MAX_TILE_INFO);
		data.max_tile_index = ako::Max(data.max_tile_index, arg_a);
		data.tile[arg_a].width = arg_b;
		data.tile[arg_a].height = arg_c;
		break;
	case ako::GenericEvent::TilePosition:
		assert(arg_a < MAX_TILE_INFO);
		data.max_tile_index = ako::Max(data.max_tile_index, arg_a);
		data.tile[arg_a].x = arg_b;
		data.tile[arg_a].y = arg_c;
		break;
	case ako::GenericEvent::TileDataSize:
		assert(arg_a < MAX_TILE_INFO);
		data.max_tile_index = ako::Max(data.max_tile_index, arg_a);
		data.tile[arg_a].data_size = arg_d.u;
		break;

	case ako::GenericEvent::LiftLowpassDimensions: // fallthrough
	case ako::GenericEvent::LiftHighpassesDimensions: break;
	}
}


template <typename T>
static void sTest(const char* out_filename, const ako::Settings& settings, unsigned width, unsigned height,
                  unsigned channels, T (*data_generator)(unsigned, uint32_t&))
{
	const unsigned depth = sizeof(T) * 8; // Implicit

	printf(" - %ux%u px, %u channel(s), %ux%u px tile(s), %u bpp\n", width, height, channels, settings.tiles_dimension,
	       settings.tiles_dimension, depth);

	// Allocate data
	T* data;
	{
		const auto data_size =
		    static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(channels) * sizeof(T);

		data = reinterpret_cast<T*>(malloc(data_size));
		assert(data != nullptr);
	}

	// Generate data
	{
		uint32_t generator_state = 1;
		for (unsigned i = 0; i < (width * height * channels); i += 1)
			data[i] = data_generator(i, generator_state);
	}

	// Encode
	void* encoded_blob = nullptr;
	size_t encoded_blob_size = 0;
	{
		auto callbacks = ako::DefaultCallbacks();
		callbacks.generic_event = sCallbackGenericEvent;
		callbacks.user_data = &s_encoded_info;

		auto status = ako::Status::Error; // Assume error
		encoded_blob_size = EncodeEx(callbacks, settings, width, height, channels, depth, data, &encoded_blob, &status);

		if (encoded_blob_size == 0)
			printf("Ako encode error: '%s'\n", ToString(status));

		// Simple checks
		assert(encoded_blob_size != 0);
		assert(status != ako::Status::Error); // Library should never return it, too vague
	}

#if !defined(_WIN64) && !defined(_WIN32)
	// Save
	{
		auto fp = fopen(out_filename, "wb");
		assert(fp != nullptr);

		const auto n = fwrite(encoded_blob, encoded_blob_size, 1, fp);
		assert(n == 1);
		fclose(fp);
	}
#endif

	// Decode
	void* decoded_data = nullptr;
	{
		auto callbacks = ako::DefaultCallbacks();
		callbacks.generic_event = sCallbackGenericEvent;
		callbacks.user_data = &s_decoded_info;

		auto status = ako::Status::Error; // Assume error
		ako::Settings decoded_settings = {};
		unsigned decoded_width = 0;
		unsigned decoded_height = 0;
		unsigned decoded_channels = 0;
		unsigned decoded_depth = 0;

		decoded_data = DecodeEx(callbacks, encoded_blob_size, encoded_blob, &decoded_width, &decoded_height,
		                        &decoded_channels, &decoded_depth, &decoded_settings, &status);

		if (decoded_data == nullptr)
			printf("Ako decode error: '%s'\n", ToString(status));

		// Simple hecks
		assert(decoded_data != nullptr);
		assert(status != ako::Status::Error);

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

	// Check data
	{
		uint32_t generator_state = 1;
		for (unsigned i = 0; i < (width * height * channels); i += 1)
			assert(reinterpret_cast<T*>(data)[i] == data_generator(i, generator_state));
	}

	// Bye!
	ako::DefaultCallbacks().free(encoded_blob);
	ako::DefaultCallbacks().free(decoded_data);
	free(data);
}


template <typename T> static T sDataGenerator(unsigned i, uint32_t& inout_state)
{
	// https://en.wikipedia.org/wiki/Xorshift#Example_implementation
	uint32_t x = inout_state;
	x ^= static_cast<uint32_t>(x << 13);
	x ^= static_cast<uint32_t>(x >> 17);
	x ^= static_cast<uint32_t>(x << 5);
	inout_state = x;

	// return static_cast<T>((i + 1) * 10);
	return static_cast<T>((i + 1) * 10 + (i % 10) + (inout_state & 16));
}


int main()
{
	printf("# ForwardBackward Test (Ako v%i.%i.%i, %s)\n", ako::VersionMajor(), ako::VersionMinor(),
	       ako::VersionPatch(), (ako::SystemEndianness() == ako::Endianness::Little) ? "little-endian" : "big-endian");

	auto settings = ako::DefaultSettings();
	sTest("/tmp/1x1x1-8bpp.ako", settings, 1, 1, 1, sDataGenerator<uint8_t>);
	sTest("/tmp/640x480x3-8bpp.ako", settings, 640, 480, 3, sDataGenerator<uint8_t>);
	sTest("/tmp/1280x720x4-8bpp.ako", settings, 1280, 720, 4, sDataGenerator<uint8_t>);

	sTest("/tmp/1x1x1-16bpp.ako", settings, 1, 1, 1, sDataGenerator<uint16_t>);
	sTest("/tmp/640x480x3-16bpp.ako", settings, 640, 480, 3, sDataGenerator<uint16_t>);
	sTest("/tmp/1280x720x4-16bpp.ako", settings, 1280, 720, 4, sDataGenerator<uint16_t>);

	settings.tiles_dimension = 256;
	sTest("/tmp/128x128x1-8bpp-256tiles.ako", settings, 128, 128, 1, sDataGenerator<uint8_t>);
	sTest("/tmp/256x256x1-8bpp-256tiles.ako", settings, 256, 256, 1, sDataGenerator<uint8_t>);

	settings.tiles_dimension = 1024;
	sTest("/tmp/1490x900x2-8bpp-1024tiles.ako", settings, 1490, 900, 2, sDataGenerator<uint8_t>);
	settings.tiles_dimension = 512;
	sTest("/tmp/1490x900x2-8bpp-512tiles.ako", settings, 1490, 900, 2, sDataGenerator<uint8_t>);
	settings.tiles_dimension = 256;
	sTest("/tmp/1490x900x2-8bpp-256tiles.ako", settings, 1490, 900, 2, sDataGenerator<uint8_t>);
	settings.tiles_dimension = 128;
	sTest("/tmp/1490x900x2-8bpp-128tiles.ako", settings, 1490, 900, 2, sDataGenerator<uint8_t>);

	return EXIT_SUCCESS;
}
