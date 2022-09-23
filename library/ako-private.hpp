/*

MIT License

Copyright (c) 2021-2022 Alexander Brandt

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef AKO_PRIVATE_HPP
#define AKO_PRIVATE_HPP

#include "ako.hpp"

#ifndef AKO_FREESTANDING
#include <climits>
#include <cstdio>
#include <cstdlib>
#endif

namespace ako
{

const uint32_t IMAGE_HEAD_MAGIC = 0x036F6B41; // "Ako-0x03"
const uint32_t TILE_HEAD_MAGIC = 0x03546B41;  // "AkT-0x03"

struct ImageHead
{
	uint32_t magic;

	// Little endian, fields from most to least significant bits:
	uint32_t a; // Width (26), Depth (6)
	uint32_t b; // Height (26), Tiles dimension (6)
	uint32_t c; // Channels (5), Color (3), Wavelet (3), Wrap (3), Compression (3)
};

struct TileHead
{
	uint32_t magic;
	uint32_t no;
	uint32_t compressed_size;
	uint32_t unused;
};

enum class Endianness
{
	Little,
	Big
};


// common/conversions.cpp:

uint32_t ToNumber(Color c);
uint32_t ToNumber(Wavelet w);
uint32_t ToNumber(Wrap w);
uint32_t ToNumber(Compression c);

Color ToColor(uint32_t number, Status& out_status);
Wavelet ToWavelet(uint32_t number, Status& out_status);
Wrap ToWrap(uint32_t number, Status& out_status);
Compression ToCompression(uint32_t number, Status& out_status);


// common/essentials.cpp:

unsigned Half(unsigned length);            // By convention yields highpass length
unsigned HalfPlusOneRule(unsigned length); // By convention yields lowpass length
unsigned NearPowerOfTwo(unsigned v);

unsigned LiftsNo(unsigned width, unsigned height);
void LiftMeasures(unsigned no, unsigned width, unsigned height, unsigned& out_lp_w, unsigned& out_lp_h,
                  unsigned& out_hp_w, unsigned& out_hp_h);

unsigned TilesNo(unsigned tiles_dimension, unsigned image_w, unsigned image_h);
void TileMeasures(unsigned tile_no, unsigned tiles_dimension, unsigned image_w, unsigned image_h, //
                  unsigned& out_w, unsigned& out_h, unsigned& out_x, unsigned& out_y);

template <typename T> size_t TileDataSize(unsigned width, unsigned height, unsigned channels);
template <typename T>
size_t WorkareaSize(unsigned tiles_dimension, unsigned image_w, unsigned image_h, unsigned channels);

Status ValidateCallbacks(const Callbacks& callbacks);
Status ValidateSettings(const Settings& settings);
Status ValidateProperties(unsigned image_w, unsigned image_h, unsigned channels, unsigned depth);
Status ValidateInput(const void* ptr, size_t input_size = 1); // TODO?


// common/utilities.cpp:

void* BetterRealloc(const Callbacks& callbacks, void* ptr, size_t new_size);

void* Memset(void* output, int c, size_t size);
void* Memcpy(void* output, const void* input, size_t size);

template <typename T>
void Memcpy2d(size_t width, size_t height, size_t in_stride, size_t out_stride, const T* in, T* out);

template <typename T> void MemcpyReversingEndianness(size_t size, const T* input, void* output);

Endianness SystemEndianness();
template <typename T> T EndiannessReverse(T value);

template <typename T> T WrapSubtract(T a, T b);
template <typename T> T WrapAdd(T a, T b);

template <typename T> T SaturateToLower(T v);


// decode/compression.cpp:
// encode/compression.cpp:

template <typename T>
size_t Decompress(const Compression& compression_method, size_t compressed_size, unsigned width, unsigned height,
                  unsigned channels, const void* input, T* output, Status&);

template <typename T>
size_t Compress(const Compression& compression_method, unsigned width, unsigned height, unsigned channels,
                const T* input, void* output);


// decode/format.cpp:
// encode/format.cpp:

template <typename TIn, typename TOut>
void FormatToRgb(const Color& color_transformation, unsigned width, unsigned height, unsigned channels,
                 size_t output_stride, TIn* input, TOut* output); // Destroys input

template <typename TIn, typename TOut>
void FormatToInternal(const Color& color_transformation, bool discard, unsigned width, unsigned height,
                      unsigned channels, size_t input_stride, const TIn* input, TOut* output);


// decode/lifting.cpp:
// encode/lifting.cpp:

template <typename T>
void Unlift(const Wavelet& wavelet_transformation, unsigned width, unsigned height, unsigned channels, T* input,
            T* output);

template <typename T>
void Lift(const Wavelet& wavelet_transformation, unsigned width, unsigned height, unsigned channels, T* input,
          T* output);


// decode/haar.cpp:
// encode/haar.cpp:

template <typename T>
void HaarHorizontalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                           const T* input, T* output);

template <typename T>
void HaarVerticalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride, const T* input,
                         T* output);

template <typename T>
void HaarHorizontalInverse(unsigned height, unsigned lp_w, unsigned hp_w, unsigned out_stride, const T* lowpass,
                           const T* highpass, T* output);

template <typename T>
void HaarInPlaceishVerticalInverse(unsigned width, unsigned lp_h, unsigned hp_h, const T* lowpass, T* highpass,
                                   T* out_lowpass);


// Misc:

template <typename T> T Min(T a, T b)
{
	return (a < b) ? a : b;
}

template <typename T> T Max(T a, T b)
{
	return (a > b) ? a : b;
}

} // namespace ako

#endif
