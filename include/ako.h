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

#define AKO_FORMAT_VERSION 1 // Format version this library handles


enum akoColorspace
{
	AKO_COLORSPACE_YCOCG = 0,
	AKO_COLORSPACE_YCOCG_R,
	AKO_COLORSPACE_RGB,
};

struct AkoSettings
{
	float quantization[4];
	float noise_gate[4];
	size_t tiles_dimension;
};

struct AkoHead
{
	char magic[4];   // "AkoI"
	uint8_t version; // AKO_FORMAT_VERSION

	uint8_t unused1;
	uint8_t unused2;

	uint8_t format;
	uint32_t width;
	uint32_t height;
};


AKO_EXPORT size_t AkoEncode(size_t width, size_t height, size_t channels, const struct AkoSettings*,
                            const uint8_t* input, void** output);
AKO_EXPORT uint8_t* AkoDecode(size_t input_size, const void* in, size_t* out_width, size_t* out_height,
                              size_t* out_channels);

AKO_EXPORT int AkoVersionMajor();
AKO_EXPORT int AkoVersionMinor();
AKO_EXPORT int AkoVersionPatch();
AKO_EXPORT const char* AkoVersionString();

//

#define AKO_COMPRESSION 1 // 0 = None, 1 = Elias gamma coding
#define AKO_COLORSPACE 1  // 0 = RGB, 1 = YCOCG, 2 = YCOCG-R (reversible)
#define AKO_WAVELET 3     // 0 = None, 1 = Haar, 2 = CDF53, 3 = DD137
#define AKO_WRAP_MODE 1   // 0 = Repeat, 1 = Clamp to edge

// Haar: Haar wavelet
// CDF53: Cohen–Daubechies–Feauveau 5/3
// DD137: Deslauriers-Dubuc 13/7

// Formulas:

// Haar: hp[i] = odd[i] - (even[i] + odd[i]) / 2
// Haar: lp[i] = (even[i] + odd[i]) / 2

// CDF53: hp[i] = odd[i] - (even[i + 1] + even[i] + 2) >> 2
// CDF53: lp[i] = even[i] + (hp[i] + hp[i - 1] + 1) >> 1

// DD137: hp[i] = odd[i] + ((even[i + 2] + even[i - 1]) - 9 * (even[i + 1] + even[i]) + 8) >> 4
// DD137: lp[i] = even[i] + ((-hp[i + 1] - hp[i - 2]) + 9 * (hp[i] + hp[i - 1]) + 16) >> 5

//

#define AKO_DEV_PRINTF_DEBUG 1
#define AKO_DEV_TINY_BENCHMARK 1
#define AKO_DEV_SAVE_IMAGES 0

#endif
