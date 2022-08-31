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

size_t NearPowerOfTwo(size_t v)
{
	size_t x = 2;
	while (x < v)
	{
		x <<= 1;
		if (x == 0)
			return 0;
	}

	return x;
}


size_t TilesNo(size_t tiles_dimension, size_t image_w, size_t image_h)
{
	if (tiles_dimension == 0)
		return 1;

	const size_t horizontal = image_w / tiles_dimension + ((image_w % tiles_dimension == 0) ? 0 : 1);
	const size_t vertical = image_h / tiles_dimension + ((image_h % tiles_dimension == 0) ? 0 : 1);

	return horizontal * vertical;
}


void TileMeasures(size_t tile_no, size_t tiles_dimension, size_t image_w, size_t image_h, //
                  size_t& out_w, size_t& out_h, size_t& out_x, size_t& out_y)
{
	if (tiles_dimension == 0)
	{
		out_x = 0;
		out_y = 0;
		out_w = image_w;
		out_h = image_h;
		return;
	}

	const size_t horizontal_tiles = image_w / tiles_dimension + ((image_w % tiles_dimension == 0) ? 0 : 1);

	out_x = (tile_no % horizontal_tiles) * tiles_dimension;
	out_y = (tile_no / horizontal_tiles) * tiles_dimension;

	out_w = (out_x + tiles_dimension <= image_w) ? (tiles_dimension) : (image_w % tiles_dimension);
	out_h = (out_y + tiles_dimension <= image_h) ? (tiles_dimension) : (image_h % tiles_dimension);
}


void* BetterRealloc(const Callbacks& callbacks, void* ptr, size_t new_size)
{
	void* prev_ptr = ptr;
	if ((ptr = callbacks.realloc(ptr, new_size)) == NULL)
	{
		callbacks.free(prev_ptr);
		return NULL;
	}

	return ptr;
}


Status ValidateCallbacks(const Callbacks& callbacks)
{
	if (callbacks.malloc == NULL || callbacks.realloc == NULL || callbacks.free == NULL)
		return Status::InvalidCallbacks;

	return Status::Ok;
}


Status ValidateSettings(const Settings& settings)
{
	if (settings.tiles_dimension != 0)
	{
		if (settings.tiles_dimension != NearPowerOfTwo(settings.tiles_dimension))
			return Status::InvalidTilesDimension;
	}

	return Status::Ok;
}


Status ValidateProperties(size_t image_w, size_t image_h, size_t channels, size_t depth)
{
	// clang-format off
	if (image_w == 0 || image_h == 0) { return Status::InvalidDimensions; }
	if (channels == 0)                { return Status::InvalidChannelsNo; }
	if (image_w > MAXIMUM_WIDTH)     { return Status::InvalidDimensions; }
	if (image_h > MAXIMUM_HEIGHT)    { return Status::InvalidDimensions; }
	if (channels > MAXIMUM_CHANNELS) { return Status::InvalidChannelsNo; }
	if (depth == 0)                  { return Status::InvalidDepth; }
	if (depth > MAXIMUM_DEPTH)       { return Status::InvalidDepth; }
	// clang-format on

	return Status::Ok;
}


Status ValidateInput(const void* ptr, size_t input_size)
{
	if (ptr == NULL || input_size == 0)
		return Status::InvalidInput;

	return Status::Ok;
}

} // namespace ako
