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

namespace ako
{

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


void* BetterRealloc(const Callbacks& callbacks, void* ptr, size_t new_size)
{
	void* prev_ptr = ptr;
	if ((ptr = callbacks.realloc(ptr, new_size)) == nullptr)
	{
		callbacks.free(prev_ptr);
		return nullptr;
	}

	return ptr;
}


Endianness SystemEndianness()
{
	const int16_t i = 1;
	auto p = reinterpret_cast<const int8_t*>(&i);

	if (p[0] != 1)
		return Endianness::Big;

	return Endianness::Little;
}


uint32_t EndiannessReverseU32(uint32_t value)
{
	const auto b1 = static_cast<uint32_t>(value & 0xFF);
	const auto b2 = static_cast<uint32_t>((value >> 8) & 0xFF);
	const auto b3 = static_cast<uint32_t>((value >> 16) & 0xFF);
	const auto b4 = static_cast<uint32_t>((value >> 24) & 0xFF);

	return (b1 << 24) | (b2 << 16) | (b3 << 8) | b4;
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
