

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

enum class Status
{
	Ok,
	Error,
	NotImplemented,
	InvalidCallbacks,
	InvalidSettings,
	InvalidInput,
};

enum class Color
{
	YCoCg,
	SubtractGreen,
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
	Manbavaran,
	Kagari,
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

	size_t tiles_size;

	unsigned quantization;
	unsigned gate;
};


AKO_EXPORT size_t Encode(const Callbacks&, const Settings&, size_t width, size_t height, size_t channels,
                         const void* input, void** output, Status& out_status);

AKO_EXPORT uint8_t* Decode(const Callbacks&, size_t input_size, const void* input, Settings& out_settings,
                           size_t& out_width, size_t& out_height, size_t& out_channels, Status& out_status);

AKO_EXPORT Settings DefaultSettings();
AKO_EXPORT Callbacks DefaultCallbacks();

AKO_EXPORT const char* StatusString(Status);

AKO_EXPORT int VersionMajor();
AKO_EXPORT int VersionMinor();
AKO_EXPORT int VersionPatch();
AKO_EXPORT int FormatVersion();

} // namespace ako
