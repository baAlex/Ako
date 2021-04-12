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
#define AKO_VER_MINOR 1 // Ditto
#define AKO_FORMAT 1    // Format version this library handles


struct AkoSettings
{
	float uniform_gate[4];
	float detail_gate[4]; // Applied on high frequencies
};

struct AkoHead
{
	char magic[5];

	uint8_t major;
	uint8_t minor;
	uint8_t format;

	uint32_t compressed_size;
	uint32_t info; // Bits 0-4 = Dimension, Bits 5-6 = Channels
};


AKO_EXPORT size_t AkoEncode(size_t dimension, size_t channels, const struct AkoSettings*, const uint8_t* input,
                            uint8_t** output);
AKO_EXPORT uint8_t* AkoDecode(size_t input_size, const uint8_t* input, size_t* out_dimension, size_t* out_channels);


// ######## Static-library only ########

#define AKO_COMPRESSION 1            // 0 = None, 1 = LZ4
#define AKO_COLOR_TRANSFORMATION 1   // 0 = None, 1 = YCoCg
#define AKO_WAVELET_TRANSFORMATION 1 // 0,1 = Activate/desactivate

#define AKO_DEV_TINY_BENCHMARK 1
#define AKO_DEV_SAVE_IMAGES 0

#endif
