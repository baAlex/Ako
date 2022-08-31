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
	out_x = ((tiles_dimension * tile_no) % image_w);
	out_y = ((tiles_dimension * tile_no) / image_w) * tiles_dimension;
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


Status ValidateProperties(const void* input, size_t image_w, size_t image_h, size_t channels, size_t depth)
{
	// clang-format off
	if (input == NULL)                { return Status::InvalidInput;      }
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


const char* ToString(Status s)
{
	switch (s)
	{
	case Status::Ok: return "Ok";
	case Status::Error: return "Unspecified error";
	case Status::NotImplemented: return "Not implemented";
	case Status::InvalidCallbacks: return "Invalid callbacks";
	case Status::InvalidTilesDimension: return "Invalid tiles dimension";
	case Status::InvalidInput: return "Invalid input";
	case Status::InvalidDimensions: return "Invalid dimensions";
	case Status::InvalidChannelsNo: return "Invalid number of channels";
	case Status::InvalidDepth: return "Invalid depth";
	case Status::NoEnoughMemory: return "No enough memory";
	}

	return "Invalid status";
}

const char* ToString(Color c)
{
	switch (c)
	{
	case Color::YCoCg: return "YCOCG";
	case Color::SubtractG: return "SUBTRACTG";
	case Color::None: return "NONE";
	}

	return "INVALID";
}

const char* ToString(Wavelet w)
{
	switch (w)
	{
	case Wavelet::Dd137: return "DD137";
	case Wavelet::Cdf53: return "CDF53";
	case Wavelet::Haar: return "HAAR";
	case Wavelet::None: return "NONE";
	}

	return "INVALID";
}

const char* ToString(Wrap w)
{
	switch (w)
	{
	case Wrap::Clamp: return "CLAMP";
	case Wrap::Mirror: return "MIRROR";
	case Wrap::Repeat: return "REPEAT";
	case Wrap::Zero: return "ZERO";
	}

	return "INVALID";
}

const char* ToString(Compression c)
{
	switch (c)
	{
	case Compression::Kagari: return "KAGARI";
	case Compression::Manbavaran: return "MANBAVARAN";
	case Compression::None: return "NONE";
	}

	return "INVALID";
}

} // namespace ako
