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

template <typename T, typename TO>
static void* sDecodeInternal(const Callbacks& callbacks, const Settings& settings, size_t image_w, size_t image_h,
                             size_t channels, size_t depth, const void* input, const void* input_end,
                             Status& out_status)
{
	(void)depth;

	auto status = Status::Ok;

	const size_t tiles_no = TilesNo(settings.tiles_dimension, image_w, image_h);
	const size_t workarea_size = WorkareaSize<T>(settings.tiles_dimension, image_w, image_h, channels);
	const size_t workareas_no = 2;

	void* workarea[workareas_no] = {NULL, NULL}; // Where work
	void* image = NULL;                          // Where output our work

	std::printf(" - D | Workarea size: %zu bytes\n", workarea_size);

	// Allocate memory
	{
		for (size_t i = 0; i < workareas_no; i += 1)
		{
			if ((workarea[i] = callbacks.malloc(workarea_size)) == NULL)
			{
				status = Status::NoEnoughMemory;
				goto return_failure;
			}
		}

		if (tiles_no == 1)
			image = workarea[0];
		else
		{
			if ((image = callbacks.malloc((image_w * image_h * channels) * sizeof(TO))) == NULL)
			{
				status = Status::NoEnoughMemory;
				goto return_failure;
			}
		}
	}

	// Iterate tiles
	for (size_t t = 0; t < tiles_no; t += 1)
	{
		size_t tile_w = 0;
		size_t tile_h = 0;
		size_t tile_x = 0;
		size_t tile_y = 0;
		size_t tile_size;

		TileMeasures(t, settings.tiles_dimension, image_w, image_h, tile_w, tile_h, tile_x, tile_y);
		tile_size = TileSize<T>(tile_w, tile_h, channels);

		std::printf(" - D | Tile %zu of %zu,\t%zux%zu px,\tat: %zu, %zu,\tsize: %zu bytes\n", t + 1, tiles_no, tile_w,
		            tile_h, tile_x, tile_y, tile_size);

		// Read tile head
		{
			if ((reinterpret_cast<const uint8_t*>(input) + sizeof(TileHead)) >= input_end)
			{
				status = Status::TruncatedTileHead;
				goto return_failure;
			}

			auto head = reinterpret_cast<const TileHead*>(input);

			if (head->magic != TILE_HEAD_MAGIC || head->size == 0)
			{
				status = Status::InvalidTileHead;
				goto return_failure;
			}

			input = reinterpret_cast<const uint8_t*>(input) + sizeof(TileHead);
			input = reinterpret_cast<const uint8_t*>(input) + head->size;
		}

		// Write ones
		{
			auto p = reinterpret_cast<TO*>(image) + (tile_x * channels) + (image_w * channels) * tile_y;
			for (size_t r = 0; r < tile_h; r += 1)
			{
				for (size_t c = 0; c < (tile_w * channels); c += 1)
					p[c] = 255;

				p += (image_w * channels);
			}
		}
	}

	// Bye!
	for (size_t i = 0; i < workareas_no; i += 1)
	{
		if (workarea[i] != image)        // Do not free recycled memory,
			callbacks.free(workarea[i]); // we may need to return it
	}

	out_status = status;
	return image;

return_failure:
	for (size_t i = 0; i < workareas_no; i += 1)
	{
		if (workarea[i] != NULL && workarea[i] != image)
			callbacks.free(workarea[i]);
	}

	if (image != NULL)
		callbacks.free(image);

	out_status = status;
	return NULL;
}


void* DecodeEx(const Callbacks& callbacks, size_t input_size, const void* input, Settings& out_settings,
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
	{
		auto input_end = reinterpret_cast<const void*>(reinterpret_cast<const uint8_t*>(input) + input_size);
		input = reinterpret_cast<const void*>(reinterpret_cast<const uint8_t*>(input) + sizeof(ImageHead));

		if (out_depth <= 8)
			return sDecodeInternal<uint16_t, uint8_t>(callbacks, out_settings, out_width, out_height, out_channels,
			                                          out_depth, input, input_end, out_status);

		return sDecodeInternal<uint32_t, uint8_t>(callbacks, out_settings, out_width, out_height, out_channels,
		                                          out_depth, input, input_end, out_status);
	}

return_failure:
	out_status = status;
	return NULL;
}

} // namespace ako
