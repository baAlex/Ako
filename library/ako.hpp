
#ifndef AKO_HPP
#define AKO_HPP

#include <stddef.h>
#include <stdint.h>

#if defined(__clang__) || defined(__GNUC__)
#define AKO_EXPORT __attribute__((visibility("default")))
#elif defined(_MSC_VER)
#define AKO_EXPORT __declspec(dllexport)
#else
#define AKO_EXPORT // Empty
#endif

namespace ako
{

const int VERSION_MAJOR = 0;
const int VERSION_MINOR = 3;
const int VERSION_PATCH = 0;
const int FORMAT_VERSION = 3;

const unsigned MAXIMUM_WIDTH = (1 << 25);
const unsigned MAXIMUM_HEIGHT = (1 << 25);
const unsigned MAXIMUM_CHANNELS = (1 << 4);
const unsigned MAXIMUM_DEPTH = (1 << 4);

const unsigned MINIMUM_WIDTH = 1;
const unsigned MINIMUM_HEIGHT = 1;
const unsigned MINIMUM_CHANNELS = 1;
const unsigned MINIMUM_DEPTH = 1;

const unsigned MAXIMUM_TILES_DIMENSION = (1 << 25);

enum class Status
{
	Ok,
	Error,
	NotImplemented,

	NoEnoughMemory,

	InvalidCallbacks,
	InvalidInput,
	InvalidTilesDimension,
	InvalidDimensions,
	InvalidChannelsNo,
	InvalidDepth,

	TruncatedImageHead,
	TruncatedTileHead,
	NotAnAkoFile,
	InvalidColor,
	InvalidWavelet,
	InvalidWrap,
	InvalidCompression,
	InvalidTileHead,
};

enum class Color
{
	YCoCg,
	SubtractG,
	None,
};

enum class Wavelet
{
	Dd137,
	Cdf53,
	Haar,
	None,
};

enum class Wrap
{
	Clamp,
	Mirror,
	Repeat,
	Zero,
};

enum class Compression
{
	Kagari,
	Manbavaran,
	None,
};

enum class GenericEvent
{
	ImageDimensions,
	ImageChannels,
	ImageDepth,

	TilesNo,
	TilesDimension,

	WorkareaSize,

	TileDimensions,
	TilePosition,
	TileDataSize,
};

struct Callbacks
{
	void* (*malloc)(size_t);
	void* (*realloc)(void*, size_t);
	void (*free)(void*);

	void (*generic_event)(GenericEvent, unsigned, unsigned, unsigned, size_t, void* user_data);
	void (*format_event)(Color, unsigned tile_no, const void*, void* user_data);
	void (*compression_event)(Compression, unsigned tile_no, const void*, void* user_data);

	void* user_data;
};

struct Settings
{
	Color color;
	Wavelet wavelet;
	Wrap wrap;
	Compression compression;

	unsigned tiles_dimension;

	// Non recoverable when decoding:
	unsigned quantization;
	unsigned gate;
	unsigned chroma_loss;
	bool discard;
};

AKO_EXPORT size_t EncodeEx(const Callbacks&, const Settings&, unsigned width, unsigned height, unsigned channels,
                           unsigned depth, const void* input, void** output = nullptr, Status* out_status = nullptr);

AKO_EXPORT void* DecodeEx(const Callbacks&, size_t input_size, const void* input, unsigned& out_width,
                          unsigned& out_height, unsigned& out_channels, unsigned& out_depth,
                          Settings* out_settings = nullptr, Status* out_status = nullptr);

AKO_EXPORT Settings DefaultSettings();
AKO_EXPORT Callbacks DefaultCallbacks();

AKO_EXPORT const char* ToString(Status);
AKO_EXPORT const char* ToString(Color c);
AKO_EXPORT const char* ToString(Wavelet w);
AKO_EXPORT const char* ToString(Wrap w);
AKO_EXPORT const char* ToString(Compression c);

AKO_EXPORT int VersionMajor();
AKO_EXPORT int VersionMinor();
AKO_EXPORT int VersionPatch();
AKO_EXPORT int FormatVersion();

} // namespace ako

#endif
