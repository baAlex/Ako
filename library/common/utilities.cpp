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
		x <<= 1;

	return x;
}


Status ValidateCallbacks(const Callbacks& callbacks)
{
	if (callbacks.malloc == NULL || callbacks.realloc == NULL || callbacks.free == NULL)
		return Status::InvalidCallbacks;

	return Status::Ok;
}


Status ValidateSettings(const Settings& settings)
{
	if (settings.tiles_size != 0)
	{
		if (settings.tiles_size != NearPowerOfTwo(settings.tiles_size))
			return Status::InvalidSettings;
	}

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
	case Status::InvalidSettings: return "Invalid settings";
	case Status::InvalidInput: return "Invalid input";
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
