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

#include "ako-private.hpp"

#ifdef _MSC_VER
#include <intrin.h>
#endif


namespace ako
{

unsigned Ctz(unsigned x)
{
#ifdef _MSC_VER
	unsigned long ret;
	_BitScanForward(&ret, static_cast<unsigned long>(x));
	return static_cast<unsigned>(ret);
#else
	return static_cast<unsigned>(__builtin_ctz(x));
#endif
}


unsigned Half(unsigned length)
{
	return (length >> 1);
}


unsigned HalfPlusOneRule(unsigned length)
{
	if (length == 1)
		return 1;

	return (length + (length & 1)) >> 1;
}


unsigned NearPowerOfTwo(unsigned v)
{
	unsigned x = 2;
	while (x < v)
	{
		x <<= 1;
		if (x == 0)
			return 0;
	}

	return x;
}


unsigned LiftsNo(unsigned width, unsigned height)
{
	unsigned lp_w = width;
	unsigned lp_h = height;
	unsigned lift_no = 0;

	for (; lp_w > 1 && lp_h > 1; lift_no += 1)
	{
		lp_w = HalfPlusOneRule(lp_w);
		lp_h = HalfPlusOneRule(lp_h);
	}

	return lift_no;
}


void LiftMeasures(unsigned no, unsigned width, unsigned height, unsigned& out_lp_w, unsigned& out_lp_h,
                  unsigned& out_hp_w, unsigned& out_hp_h)
{
	out_hp_w = Half(width);
	out_hp_h = Half(height);
	out_lp_w = HalfPlusOneRule(width);
	out_lp_h = HalfPlusOneRule(height);

	for (unsigned i = 0; i < no && out_lp_w > 1 && out_lp_h > 1; i += 1)
	{
		out_hp_w = Half(out_lp_w);
		out_hp_h = Half(out_lp_h);
		out_lp_w = HalfPlusOneRule(out_lp_w);
		out_lp_h = HalfPlusOneRule(out_lp_h);
	}
}


unsigned TilesNo(unsigned tiles_dimension, unsigned image_w, unsigned image_h)
{
	if (tiles_dimension == 0)
		return 1;

	const auto horizontal = image_w / tiles_dimension + ((image_w % tiles_dimension == 0) ? 0 : 1);
	const auto vertical = image_h / tiles_dimension + ((image_h % tiles_dimension == 0) ? 0 : 1);

	return horizontal * vertical;
}


void TileMeasures(unsigned tile_no, unsigned tiles_dimension, unsigned image_w, unsigned image_h, //
                  unsigned& out_w, unsigned& out_h, unsigned& out_x, unsigned& out_y)
{
	if (tiles_dimension == 0)
	{
		out_x = 0;
		out_y = 0;
		out_w = image_w;
		out_h = image_h;
		return;
	}

	const auto horizontal_tiles = image_w / tiles_dimension + ((image_w % tiles_dimension == 0) ? 0 : 1);

	out_x = (tile_no % horizontal_tiles) * tiles_dimension;
	out_y = (tile_no / horizontal_tiles) * tiles_dimension;

	out_w = (out_x + tiles_dimension <= image_w) ? (tiles_dimension) : (image_w % tiles_dimension);
	out_h = (out_y + tiles_dimension <= image_h) ? (tiles_dimension) : (image_h % tiles_dimension);
}


template <typename T> static size_t sTileDataSize(unsigned width, unsigned height, unsigned channels)
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

template <> size_t TileDataSize<int16_t>(unsigned width, unsigned height, unsigned channels) // ???, weird syntax
{
	return sTileDataSize<int16_t>(width, height, channels);
}

template <> size_t TileDataSize<int32_t>(unsigned width, unsigned height, unsigned channels)
{
	return sTileDataSize<int32_t>(width, height, channels);
}


template <typename T>
static size_t sWorkareaSize(unsigned tiles_dimension, unsigned image_w, unsigned image_h, unsigned channels)
{
	if (tiles_dimension != 0)
		return (Min(tiles_dimension, image_w) * Min(tiles_dimension, image_h) * channels) * sizeof(T) +
		       sizeof(ImageHead) + sizeof(TileHead);

	return (image_w * image_h * channels) * sizeof(T) + sizeof(ImageHead) + sizeof(TileHead);
}

template <>
size_t WorkareaSize<int16_t>(unsigned tiles_dimension, unsigned image_w, unsigned image_h, unsigned channels)
{
	return sWorkareaSize<int16_t>(tiles_dimension, image_w, image_h, channels);
}

template <>
size_t WorkareaSize<int32_t>(unsigned tiles_dimension, unsigned image_w, unsigned image_h, unsigned channels)
{
	return sWorkareaSize<int32_t>(tiles_dimension, image_w, image_h, channels);
}


Status ValidateCallbacks(const Callbacks& callbacks)
{
	if (callbacks.malloc == nullptr || callbacks.realloc == nullptr || callbacks.free == nullptr)
		return Status::InvalidCallbacks;

	return Status::Ok;
}


Status ValidateSettings(const Settings& settings)
{
	if (settings.tiles_dimension != 0)
	{
		if (settings.tiles_dimension != NearPowerOfTwo(settings.tiles_dimension) ||
		    settings.tiles_dimension > MAXIMUM_TILES_DIMENSION)
			return Status::InvalidTilesDimension;

		if (settings.quantization < 0.0F || settings.gate < 0.0F || settings.chroma_loss < 0.0F ||
		    settings.ratio < 0.0F)
			return Status::InvalidSettings;
	}

	return Status::Ok;
}


Status ValidateProperties(unsigned image_w, unsigned image_h, unsigned channels, unsigned depth)
{
	// clang-format off
	if (image_w > MAXIMUM_WIDTH)     { return Status::InvalidDimensions; }
	if (image_h > MAXIMUM_HEIGHT)    { return Status::InvalidDimensions; }
	if (image_w < MINIMUM_WIDTH)     { return Status::InvalidDimensions; }
	if (image_h < MINIMUM_HEIGHT)    { return Status::InvalidDimensions; }

	if (channels < MINIMUM_CHANNELS) { return Status::InvalidChannelsNo; }
	if (channels > MAXIMUM_CHANNELS) { return Status::InvalidChannelsNo; }

	if (depth < MINIMUM_DEPTH)       { return Status::InvalidDepth; }
	if (depth > MAXIMUM_DEPTH)       { return Status::InvalidDepth; }
	// clang-format on

	return Status::Ok;
}


Status ValidateInput(const void* ptr, size_t input_size)
{
	if (ptr == nullptr || input_size == 0)
		return Status::InvalidInput;

	return Status::Ok;
}

} // namespace ako
