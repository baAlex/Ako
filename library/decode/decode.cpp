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

/*uint8_t* sDecodeInternal(const Callbacks& callbacks, const Settings& settings, size_t image_w, size_t image_h,
                         size_t channels, size_t depth, size_t input_size, const void* input, Status& out_status)
{
    (void)callbacks;
    (void)settings;
    (void)image_w;
    (void)image_h;
    (void)channels;
    (void)depth;
    (void)input_size;
    (void)input;
    (void)out_status;

    return NULL;
}*/


uint8_t* DecodeEx(const Callbacks& callbacks, size_t input_size, const void* input, Settings& out_settings,
                  size_t& out_width, size_t& out_height, size_t& out_channels, size_t& out_depth, Status& out_status)
{
	auto status = Status::Ok;

	// Checks
	if ((status = ValidateCallbacks(callbacks)) != Status::Ok ||
	    (status = ValidateInput(input, input_size)) != Status::Ok)
		goto return_failure;

	// Read image head (the most inefficient head ever)
	{
		if (input_size < sizeof(ImageHead))
		{
			status = Status::TruncatedImageHead;
			goto return_failure;
		}

		// Properties
		auto head = reinterpret_cast<const ImageHead*>(input);

		if (head->magic != IMAGE_HEAD_MAGIC)
		{
			status = Status::NotAnAkoFile;
			goto return_failure;
		}

		const size_t width = static_cast<size_t>(head->width) + 1;
		const size_t height = static_cast<size_t>(head->height) + 1;
		const size_t channels = static_cast<size_t>(head->channels) + 1;
		const size_t depth = static_cast<size_t>(head->depth) + 1;

		if ((status = ValidateProperties(width, height, channels, depth)) != Status::Ok)
			goto return_failure;

		// Settings
		auto temp_settings = DefaultSettings();

		temp_settings.tiles_dimension = static_cast<size_t>(head->tiles_dimension);

		temp_settings.color = ToColor(head->color, status);
		if (status != Status::Ok)
			goto return_failure;

		temp_settings.wavelet = ToWavelet(head->wavelet, status);
		if (status != Status::Ok)
			goto return_failure;

		temp_settings.wrap = ToWrap(head->wrap, status);
		if (status != Status::Ok)
			goto return_failure;

		temp_settings.compression = ToCompression(head->compression, status);
		if (status != Status::Ok)
			goto return_failure;

		if ((status = ValidateSettings(temp_settings)) != Status::Ok)
			goto return_failure;

		// All validated, return what we have (done here, at the end,
		// to avoid return invalid values in case of failure)
		out_width = width;
		out_height = height;
		out_channels = channels;
		out_depth = depth;
		out_settings = temp_settings;
	}

	// Decode!
	out_status = status;
	return static_cast<uint8_t*>(callbacks.malloc(out_width * out_height * out_channels));

return_failure:
	out_status = status;
	return NULL;
}

} // namespace ako
