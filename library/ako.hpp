

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

enum Status
{
	Ok,
	Error,
	NotImplemented,
};

struct Callbacks
{
	void* (*malloc)(size_t);
	void* (*realloc)(void*, size_t);
	void (*free)(void*);
};

struct Settings
{
	int empty; // For now
};


AKO_EXPORT size_t Encode(const Callbacks*, const Settings*, size_t channels, size_t width, size_t height,
                         const void* input, Status* out_status);

AKO_EXPORT void* Decode(const Callbacks*, size_t input_size, const void* input, Settings* out_settings,
                        size_t* out_channels, size_t* out_width, size_t* out_height, Status* out_status);

AKO_EXPORT Settings DefaultSettings();
AKO_EXPORT Callbacks DefaultCallbacks();

AKO_EXPORT const char* StatusString(Status);

AKO_EXPORT int VersionMajor();
AKO_EXPORT int VersionMinor();
AKO_EXPORT int VersionPatch();
AKO_EXPORT int FormatVersion();

} // namespace ako
