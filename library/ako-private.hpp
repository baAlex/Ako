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

unsigned NearPowerOfTwo(unsigned v);

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
void Unlift(const Wavelet& w, unsigned width, unsigned height, unsigned channels, const T* input, T* output);

template <typename T>
void Lift(const Wavelet& w, unsigned width, unsigned height, unsigned channels, const T* input, T* output);


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

template <typename T> size_t TileDataSize(unsigned tile_w, unsigned tile_h, unsigned channels)
{
	// Silly formula, but the idea is to abstract it to make it future-proof
	return tile_w * tile_h * channels * sizeof(T);
}

template <typename T>
size_t WorkareaSize(unsigned tiles_dimension, unsigned image_w, unsigned image_h, unsigned channels)
{
	if (tiles_dimension != 0)
		return (Min(tiles_dimension, image_w) * Min(tiles_dimension, image_h) * channels) * sizeof(T) +
		       sizeof(ImageHead) + sizeof(TileHead);

	return (image_w * image_h * channels) * sizeof(T) + sizeof(ImageHead) + sizeof(TileHead);
}

} // namespace ako

#endif
