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


// common/utilities.cpp:

unsigned Half(unsigned length);            // By convention yields highpass length
unsigned HalfPlusOneRule(unsigned length); // By convention yields lowpass length
unsigned NearPowerOfTwo(unsigned v);

unsigned LiftsNo(unsigned width, unsigned height);
void LiftMeasures(unsigned no, unsigned width, unsigned height, unsigned& out_lp_w, unsigned& out_lp_h,
                  unsigned& out_hp_w, unsigned& out_hp_h);

unsigned TilesNo(unsigned tiles_dimension, unsigned image_w, unsigned image_h);
void TileMeasures(unsigned tile_no, unsigned tiles_dimension, unsigned image_w, unsigned image_h, //
                  unsigned& out_w, unsigned& out_h, unsigned& out_x, unsigned& out_y);

void* BetterRealloc(const Callbacks& callbacks, void* ptr, size_t new_size);

Endianness SystemEndianness();
uint32_t EndiannessReverseU32(uint32_t value);

Status ValidateCallbacks(const Callbacks& callbacks);
Status ValidateSettings(const Settings& settings);
Status ValidateProperties(unsigned image_w, unsigned image_h, unsigned channels, unsigned depth);
Status ValidateInput(const void* ptr, size_t input_size = 1); // TODO?


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
void Unlift(const Wavelet& w, unsigned width, unsigned height, unsigned channels, T* input, T* output);

template <typename T>
void Lift(const Wavelet& w, unsigned width, unsigned height, unsigned channels, T* input, T* output);


// Templates:

template <typename T> T Min(T a, T b)
{
	return (a < b) ? a : b;
}

template <typename T> T Max(T a, T b)
{
	return (a > b) ? a : b;
}


template <typename T> T SaturateToLower(T v);

template <> inline int16_t SaturateToLower(int16_t v)
{
	return (Max<int16_t>(0, Min<int16_t>(v, 255)));
}

template <> inline int32_t SaturateToLower(int32_t v)
{
	return (Max<int32_t>(0, Min<int32_t>(v, 65535)));
}


template <typename T>
void Memcpy2d(size_t width, size_t height, size_t in_stride, size_t out_stride, const T* in, T* out)
{
	for (size_t row = 0; row < height; row += 1)
	{
		for (size_t col = 0; col < width; col += 1)
			out[col] = in[col];

		in += in_stride;
		out += out_stride;
	}
}

inline void* Memset(void* output, int c, size_t size)
{
#if defined(__clang__) || defined(__GNUC__)
	__builtin_memset(output, c, size);
#else
	auto out = reinterpret_cast<uint8_t*>(output);
	for (; size != 0; size -= 1, out += 1)
		*out = static_cast<uint8_t>(c);
#endif

	return output;
}

inline void* Memcpy(void* output, const void* input, size_t size)
{
#if defined(__clang__) || defined(__GNUC__)
	__builtin_memcpy(output, input, size);
#else
	auto out = reinterpret_cast<uint8_t*>(output);
	auto in = reinterpret_cast<const uint8_t*>(input);
	for (; size != 0; size -= 1, out += 1, in += 1)
		*out = *in;
#endif

	return output;
}


template <typename T> size_t TileDataSize(unsigned width, unsigned height, unsigned channels)
{
	size_t size = 0;
	auto lp_w = width;
	auto lp_h = height;

	for (; lp_w > 1 && lp_h > 1;)
	{
		size += (Half(lp_w) * Half(lp_h));            // Quadrant D
		size += (Half(lp_w) * HalfPlusOneRule(lp_h)); // Quadrant B
		size += (HalfPlusOneRule(lp_w) * Half(lp_h)); // Quadrant C

		lp_w = HalfPlusOneRule(lp_w);
		lp_h = HalfPlusOneRule(lp_h);
	}

	size += (lp_w * lp_h) * 1; // Quadrant A

	return size * channels * sizeof(T);
}

template <typename T>
size_t WorkareaSize(unsigned tiles_dimension, unsigned image_w, unsigned image_h, unsigned channels)
{
	if (tiles_dimension != 0)
		return (Min(tiles_dimension, image_w) * Min(tiles_dimension, image_h) * channels) * sizeof(T) +
		       sizeof(ImageHead) + sizeof(TileHead);

	return (image_w * image_h * channels) * sizeof(T) + sizeof(ImageHead) + sizeof(TileHead);
}


template <typename T> T WrapSubtract(T a, T b);
template <typename T> T WrapAdd(T a, T b);

template <> inline int16_t WrapSubtract(int16_t a, int16_t b)
{
	// This seems to be a non trivial problem: https://en.wikipedia.org/wiki/Modulo_operation
	// In any case following formulas are reversible, in a defined way, in C and C++ compilers:
	return static_cast<int16_t>((static_cast<int32_t>(a) - static_cast<int32_t>(b)) % 65536);

	// Also: https://bugzilla.mozilla.org/show_bug.cgi?id=1031653
}

template <> inline int16_t WrapAdd(int16_t a, int16_t b)
{
	return static_cast<int16_t>((static_cast<int32_t>(a) + static_cast<int32_t>(b)) % 65536);
}

template <> inline int32_t WrapSubtract(int32_t a, int32_t b)
{
	return static_cast<int32_t>((static_cast<int64_t>(a) - static_cast<int64_t>(b)) % 4294967296);
}

template <> inline int32_t WrapAdd(int32_t a, int32_t b)
{
	return static_cast<int16_t>((static_cast<int64_t>(a) + static_cast<int64_t>(b)) % 4294967296);
}

} // namespace ako

#endif
