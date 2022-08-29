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

uint8_t* Decode(const Callbacks& callbacks, size_t input_size, const void* input, Settings& out_settings,
                size_t& out_width, size_t& out_height, size_t& out_channels, Status& out_status)
{
	(void)out_settings;

	auto s = Status::Ok;

	// Checks
	if ((s = ValidateCallbacks(callbacks)) != Status::Ok)
	{
		out_status = s;
		return NULL;
	}

	if (input == NULL || input_size == 0)
	{
		out_status = Status::InvalidInput;
		return NULL;
	}

	// Do something
	const size_t width = 123;
	const size_t height = 123;
	const size_t channels = 3;
	out_width = width;
	out_height = height;
	out_channels = channels;

	auto image = static_cast<uint8_t*>(callbacks.malloc(width * height * channels));

	// Bye!
	out_status = s;
	return image;
}

} // namespace ako
