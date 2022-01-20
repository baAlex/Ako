
#ifndef AKO_H
#define AKO_H

#include <stddef.h>
#include <stdint.h>

#define AKO_VERSION_MAJOR 0
#define AKO_VERSION_MINOR 2
#define AKO_VERSION_PATCH 0

#define AKO_FORMAT_VERSION 2

#define AKO_MAX_CHANNELS 16
#define AKO_MAX_WIDTH 4294967295
#define AKO_MAX_HEIGHT 4294967295
#define AKO_MIN_TILES_DIMENSION 8
#define AKO_MAX_TILES_DIMENSION 2147483648


enum akoStatus
{
	AKO_OK = 0,
	AKO_ERROR,

	AKO_INVALID_CHANNELS_NO,
	AKO_INVALID_DIMENSIONS,
	AKO_INVALID_TILES_DIMENSIONS,
	AKO_INVALID_WRAP_MODE,
	AKO_INVALID_WAVELET_TRANSFORMATION,
	AKO_INVALID_COLOR_TRANSFORMATION,
	AKO_INVALID_COMPRESSION_METHOD,

	AKO_INVALID_INPUT,
	AKO_INVALID_CALLBACKS,
	AKO_INVALID_MAGIC,
	AKO_UNSUPPORTED_VERSION,
	AKO_NO_ENOUGH_MEMORY,
	AKO_INVALID_FLAGS,
	AKO_BROKEN_INPUT,
};

enum akoWrap // Wrap mode
{
	AKO_WRAP_CLAMP = 0,
	AKO_WRAP_REPEAT,
	AKO_WRAP_ZERO,
};

enum akoWavelet // Wavelet transformation
{
	AKO_WAVELET_DD137 = 0,
	AKO_WAVELET_CDF53,
	AKO_WAVELET_HAAR,
	AKO_WAVELET_NONE,
};

enum akoColor // Color transformation
{
	AKO_COLOR_YCOCG = 0,
	AKO_COLOR_YCOCG_R,
	AKO_COLOR_SUBTRACT_G,
	AKO_COLOR_RGB,
};

enum akoCompression // Compression method
{
	AKO_COMPRESSION_ELIAS_RLE = 0,
	AKO_COMPRESSION_NONE,
};

enum akoEvent
{
	AKO_EVENT_NONE = 0,
	AKO_EVENT_FORMAT_START,
	AKO_EVENT_FORMAT_END,
	AKO_EVENT_WAVELET_START,
	AKO_EVENT_WAVELET_END,
	AKO_EVENT_COMPRESSION_START,
	AKO_EVENT_COMPRESSION_END,
};

struct akoSettings
{
	enum akoWrap wrap;
	enum akoWavelet wavelet;
	enum akoColor color;
	enum akoCompression compression;
	size_t tiles_dimension;

	int discard_transparent_pixels;
};

struct akoCallbacks
{
	void* (*malloc)(size_t);
	void* (*realloc)(void*, size_t);
	void (*free)(void*);

	void (*events)(size_t, size_t, enum akoEvent, void*);
	void* events_data;
};

struct akoHead
{
	uint8_t magic[3]; // "Ako"
	uint8_t version;  // 2 (AKO_FORMAT_VERSION)

	uint32_t width;  // 0 = Invalid
	uint32_t height; // Ditto

	uint32_t flags;
	// bits 0-3   : Channels,        0 = Gray, 1 = Gray + Alpha, 2 = RGB, 3 = RGBA, etc...
	// bits 4-5   : Wrap,            0 = Clamp, 1 = Repeat, 2 = Set to zero
	// bits 6-7   : Wavelet,         0 = DD137, 1 = CDF53, 2 = Haar, 3 = None
	// bits 8-9   : Color,           0 = YCOCG, 1 = YCOCG-R, 2 = Subtract Green, 3 = RGB
	// bits 10-11 : Compression,     0 = Elias Coding + Rle, 1 = No compression
	// bits 12-16 : Tiles dimension, 0 = No tiles, 1 = 8x8, 2 = 16x16, 3 = 32x32, 4 = 64x64, etc...
	// bits 17-32 : Unused bits (always zero)
};


size_t akoEncodeExt(const struct akoCallbacks*, const struct akoSettings*, size_t channels, size_t image_w,
                    size_t image_h, const void* in, void** out, enum akoStatus* out_status);
uint8_t* akoDecodeExt(const struct akoCallbacks*, size_t input_size, const void* in, struct akoSettings* out_s,
                      size_t* out_channels, size_t* out_w, size_t* out_h, enum akoStatus* out_status);

struct akoSettings akoDefaultSettings();
struct akoCallbacks akoDefaultCallbacks();
void akoDefaultFree(void*);

const char* akoStatusString(enum akoStatus);

int akoVersionMajor();
int akoVersionMinor();
int akoVersionPatch();

int akoFormatVersion();

#endif
