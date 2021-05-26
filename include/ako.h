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

//

#define AKO_COLORSPACE 1 // 0 = RGB, 1 = YCOCG, 2 = YCOCG-R (reversible)
#define AKO_WAVELET 2    // 0 = Haar, 1 = CDF53, 2 = 97DD

// Haar: Haar wavelet
// The traditional one not suitable for integer arithmetics (lossy)
// Lowpass is '(a + b) / 2', a box filter.

// CDF53: Cohen–Daubechies–Feauveau 5/3.
// Used in JPEG2000 in lossless mode... should be lossless assuming that
// I'm doing everything right. A cost of this is that the lowpass pixels
// are not centre-aligned (as in a box filter) but at a tiny bit to the
// top-left corner.

// 97DD: Deslauriers-Dubuc 9/7
// Used in Dirac/VC-2. Lossy, the lowpass is the same as CDF53. The
// highpass carry extra samples in a smoothstep fashion... maybe it´s
// just me, but the functions and the image they produce looks similar.

//

#define AKO_DEV_TINY_BENCHMARK 0
#define AKO_DEV_SAVE_IMAGES 0

#endif
