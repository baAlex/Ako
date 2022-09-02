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
#include <cstdio>
#include <cstdlib>
#endif

namespace ako
{

const size_t IMAGE_HEAD_MAGIC = 0x036F6B41; // "Ako-0x03"
const size_t TILE_HEAD_MAGIC = 0x03546B41;  // "AkT-0x03"

struct ImageHead
{
	uint32_t magic;

	// Little endian, fields from most to least significant bits:
	uint32_t a; // Width (26), Depth (6)
	uint32_t b; // Height (26), Tiles size (6)
	uint32_t c; // Channels (5), Color (3), Wavelet (3), Wrap (3), Compression (3)
};

struct TileHead
{
	uint32_t magic;
	uint32_t no;
	uint32_t size; // In compressed form
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

size_t NearPowerOfTwo(size_t v);

size_t TilesNo(size_t tiles_dimension, size_t image_w, size_t image_h);
void TileMeasures(size_t tile_no, size_t tiles_dimension, size_t image_w, size_t image_h, //
                  size_t& out_w, size_t& out_h, size_t& out_x, size_t& out_y);

void* BetterRealloc(const Callbacks& callbacks, void* ptr, size_t new_size);

Endianness SystemEndianness();
uint32_t EndiannessReverseU32(uint32_t value);

Status ValidateCallbacks(const Callbacks& callbacks);
Status ValidateSettings(const Settings& settings);
Status ValidateProperties(size_t image_w, size_t image_h, size_t channels, size_t depth);
Status ValidateInput(const void* ptr, size_t input_size = 1); // TODO?


// Templates:

template <typename T> T Min(T a, T b)
{
	return (a < b) ? a : b;
}

template <typename T> size_t TileSize(size_t tile_w, size_t tile_h, size_t channels)
{
	// Silly formula, but the idea is to abstract it to make it future-proof
	return tile_w * tile_h * channels * sizeof(T);
}

template <typename T> size_t WorkareaSize(size_t tiles_dimension, size_t image_w, size_t image_h, size_t channels)
{
	if (tiles_dimension != 0)
		return (Min(tiles_dimension, image_w) * Min(tiles_dimension, image_h) * channels) * sizeof(T) +
		       sizeof(ImageHead) + sizeof(TileHead);

	return (image_w * image_h * channels) * sizeof(T) + sizeof(ImageHead) + sizeof(TileHead);
}

} // namespace ako

#endif
