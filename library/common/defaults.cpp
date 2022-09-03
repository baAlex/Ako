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

Settings DefaultSettings()
{
	Settings settings = {};

	settings.color = Color::YCoCg;
	settings.wavelet = Wavelet::Dd137;
	settings.wrap = Wrap::Clamp;
	settings.compression = Compression::Kagari;

	settings.tiles_dimension = 0;

	settings.quantization = 4;
	settings.gate = 0;

	return settings;
}


#ifndef AKO_FREESTANDING
Callbacks DefaultCallbacks()
{
	Callbacks callbacks = {};

	callbacks.malloc = std::malloc;
	callbacks.realloc = std::realloc;
	callbacks.free = std::free;

	callbacks.generic_event = NULL;
	callbacks.format_event = NULL;
	callbacks.compression_event = NULL;
	callbacks.user_data = NULL;

	return callbacks;
}
#endif

} // namespace ako
