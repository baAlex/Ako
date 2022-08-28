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

size_t Encode(const Callbacks& callbacks, const Settings& settings, size_t width, size_t height, size_t channels,
              const void* input, void** output, Status& out_status)
{
	auto s = Status::Ok;

	// Checks
	if ((s = ValidateCallbacks(callbacks)) != Status::Ok || (s = ValidateSettings(settings)) != Status::Ok)
	{
		out_status = s;
		return 0;
	}

	if (input == NULL || width == 0 || height == 0 || channels == 0)
	{
		out_status = Status::InvalidInput;
		return 0;
	}

	// Do something
	const size_t blob_size = 123;
	auto blob = callbacks.malloc(blob_size);

	// Bye!
	if (output != NULL)
		*output = blob;
	else
		callbacks.free(blob);

	out_status = s;
	return blob_size;
}

} // namespace ako
