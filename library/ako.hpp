
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

const size_t MAXIMUM_WIDTH = UINT32_MAX;
const size_t MAXIMUM_HEIGHT = UINT32_MAX;
const size_t MAXIMUM_CHANNELS = UINT32_MAX;
const size_t MAXIMUM_TILES_SIZE = UINT32_MAX;
const size_t MAXIMUM_DEPTH = 16;

enum class Status
{
	Ok,
	Error,
	NotImplemented,
	InvalidCallbacks,
	InvalidTilesDimension,
	InvalidInput,
	InvalidDimensions,
	InvalidChannelsNo,
	InvalidDepth,
	NoEnoughMemory,
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

struct Callbacks
{
	void* (*malloc)(size_t);
	void* (*realloc)(void*, size_t);
	void (*free)(void*);
};

struct Settings
{
	Color color;
	Wavelet wavelet;
	Wrap wrap;
	Compression compression;

	size_t tiles_dimension;

	unsigned quantization;
	unsigned gate;

	unsigned chroma_loss;
	bool discard;
};

AKO_EXPORT size_t EncodeEx(const Callbacks&, const Settings&, size_t width, size_t height, size_t channels,
                           size_t depth, const void* input, void** output, Status& out_status);

AKO_EXPORT uint8_t* DecodeEx(const Callbacks&, size_t input_size, const void* input, Settings& out_settings,
                             size_t& out_width, size_t& out_height, size_t& out_channels, size_t& out_depth,
                             Status& out_status);

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
