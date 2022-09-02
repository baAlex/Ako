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

const char* ToString(Status s)
{
	switch (s)
	{
	case Status::Ok: return "Ok!";
	case Status::Error: return "Unspecified error";
	case Status::NotImplemented: return "Feature not implemented";

	case Status::NoEnoughMemory: return "No enough memory";

	case Status::InvalidCallbacks: return "Invalid callbacks";
	case Status::InvalidInput: return "Invalid input";
	case Status::InvalidTilesDimension: return "Invalid tiles dimension";
	case Status::InvalidDimensions: return "Invalid image dimensions";
	case Status::InvalidChannelsNo: return "Invalid number of channels";
	case Status::InvalidDepth: return "Invalid depth";

	case Status::TruncatedImageHead: return "Truncated data, near image head";
	case Status::TruncatedTileHead: return "Truncated data, near a tile head";
	case Status::NotAnAkoFile: return "Not an Ako file";
	case Status::InvalidColor: return "Invalid color";
	case Status::InvalidWavelet: return "Invalid wavelet";
	case Status::InvalidWrap: return "Invalid wrap";
	case Status::InvalidCompression: return "Invalid compression";
	case Status::InvalidTileHead: return "Invalid tile head";
	}

	// This case should never happen, but... since we are a dynamic library,
	// nothing prevents our user from providing us weird values
	return "Api error";
}

const char* ToString(Color c)
{
	switch (c)
	{
	case Color::YCoCg: return "YCOCG";
	case Color::SubtractG: return "SUBTRACTG";
	case Color::None: return "NONE";
	}

	return "Api error";
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

	return "Api error";
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

	return "Api error";
}

const char* ToString(Compression c)
{
	switch (c)
	{
	case Compression::Kagari: return "KAGARI";
	case Compression::Manbavaran: return "MANBAVARAN";
	case Compression::None: return "NONE";
	}

	return "Api error";
}


uint32_t ToNumber(Color c)
{
	switch (c)
	{
	case Color::YCoCg: return 0;
	case Color::SubtractG: return 1;
	case Color::None: return 2;
	}

	return UINT8_MAX; // This never will happen, but it seems that
	                  // Clang is the only compiler aware of it
}

uint32_t ToNumber(Wavelet w)
{
	switch (w)
	{
	case Wavelet::Dd137: return 0;
	case Wavelet::Cdf53: return 1;
	case Wavelet::Haar: return 2;
	case Wavelet::None: return 3;
	}

	return UINT8_MAX;
}

uint32_t ToNumber(Wrap w)
{
	switch (w)
	{
	case Wrap::Clamp: return 0;
	case Wrap::Mirror: return 1;
	case Wrap::Repeat: return 2;
	case Wrap::Zero: return 3;
	}

	return UINT8_MAX;
}

uint32_t ToNumber(Compression c)
{
	switch (c)
	{
	case Compression::Kagari: return 0;
	case Compression::Manbavaran: return 1;
	case Compression::None: return 2;
	}

	return UINT8_MAX;
}


Color ToColor(uint32_t number, Status& out_status)
{
	out_status = Status::Ok;
	switch (number)
	{
	case 0: return Color::YCoCg;
	case 1: return Color::SubtractG;
	case 2: return Color::None;
	}

	out_status = Status::InvalidColor;
	return Color::None;
}

Wavelet ToWavelet(uint32_t number, Status& out_status)
{
	out_status = Status::Ok;
	switch (number)
	{
	case 0: return Wavelet::Dd137;
	case 1: return Wavelet::Cdf53;
	case 2: return Wavelet::Haar;
	case 3: return Wavelet::None;
	}

	out_status = Status::InvalidColor;
	return Wavelet::None;
}

Wrap ToWrap(uint32_t number, Status& out_status)
{
	out_status = Status::Ok;
	switch (number)
	{
	case 0: return Wrap::Clamp;
	case 1: return Wrap::Mirror;
	case 2: return Wrap::Repeat;
	case 3: return Wrap::Zero;
	}

	out_status = Status::InvalidColor;
	return Wrap::Zero;
}

Compression ToCompression(uint32_t number, Status& out_status)
{
	out_status = Status::Ok;
	switch (number)
	{
	case 0: return Compression::Kagari;
	case 1: return Compression::Manbavaran;
	case 2: return Compression::None;
	}

	out_status = Status::InvalidColor;
	return Compression::None;
}

} // namespace ako
