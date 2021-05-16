/*-----------------------------

 [ako.h]
 - Alexander Brandt 2021
-----------------------------*/

#ifndef AKO_H
#define AKO_H

#ifdef AKO_EXPORT_SYMBOLS
#if defined(__clang__) || defined(__GNUC__)
#define AKO_EXPORT __attribute__((visibility("default")))
#elif defined(_MSC_VER)
#define AKO_EXPORT __declspec(dllexport)
#endif
#else
#define AKO_EXPORT // Whitespace
#endif

#include <stddef.h>
#include <stdint.h>


#define AKO_VER_MAJOR 0 // Library version
#define AKO_VER_MINOR 1
#define AKO_VER_PATCH 0

#define AKO_FORMAT 1 // Format version this library handles


struct AkoSettings
{
	float detail_gate[4]; // Applied on high frequencies
	size_t limit[4];
};

struct AkoHead
{
	char magic[5];

	uint8_t major;
	uint8_t minor;
	uint8_t format;

	uint32_t compressed_data_size;
	uint32_t info; // Bits 0-4 = Dimension, Bits 5-6 = Channels
};


AKO_EXPORT size_t AkoEncode(size_t dimension, size_t channels, const struct AkoSettings*, const uint8_t* input,
                            void** output);
AKO_EXPORT uint8_t* AkoDecode(size_t input_size, const void* input, size_t* out_dimension, size_t* out_channels);

AKO_EXPORT int AkoVersionMajor();
AKO_EXPORT int AkoVersionMinor();
AKO_EXPORT int AkoVersionPatch();
AKO_EXPORT const char* AkoVersionString();

#endif
