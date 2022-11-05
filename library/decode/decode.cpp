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

template <typename TCoeff, typename TOut>
static void* sDecodeInternal(const Callbacks& callbacks, const Settings& settings, unsigned image_w, unsigned image_h,
                             unsigned channels, unsigned depth, const void* input, const void* input_end,
                             Status& out_status)
{
	(void)depth; // Implicit in TCoeff and TOut

	auto status = Status::Ok;

	const unsigned tiles_no = TilesNo(settings.tiles_dimension, image_w, image_h);
	const size_t workarea_size = WorkareaSize<TCoeff>(settings.tiles_dimension, image_w, image_h, channels);
	const size_t workareas_no = 2;

	void* workarea[workareas_no] = {nullptr, nullptr}; // Where work
	void* image = nullptr;                             // Where output our work

	// Feedback
	if (callbacks.generic_event != nullptr)
	{
		callbacks.generic_event(GenericEvent::ImageDimensions, image_w, image_h, 0, {0}, callbacks.user_data);
		callbacks.generic_event(GenericEvent::ImageChannels, channels, 0, 0, {0}, callbacks.user_data);
		callbacks.generic_event(GenericEvent::ImageDepth, depth, 0, 0, {0}, callbacks.user_data);

		callbacks.generic_event(GenericEvent::TilesNo, tiles_no, 0, 0, {0}, callbacks.user_data);
		callbacks.generic_event(GenericEvent::TilesDimension, settings.tiles_dimension, 0, 0, {0}, callbacks.user_data);

		callbacks.generic_event(GenericEvent::WorkareaSize, 0, 0, 0, {workarea_size}, callbacks.user_data);
	}

	// Allocate memory
	{
		for (size_t i = 0; i < workareas_no; i += 1)
		{
			if ((workarea[i] = callbacks.malloc(workarea_size)) == nullptr)
			{
				status = Status::NoEnoughMemory;
				goto return_failure;
			}

			// Developers, developers, developers
			// Memset(workarea[i], 0, workarea_size);
		}

		if (tiles_no == 1)
			image = workarea[0];
		else
		{
			if ((image = callbacks.malloc((image_w * image_h * channels) * sizeof(TOut))) == nullptr)
			{
				status = Status::NoEnoughMemory;
				goto return_failure;
			}
		}
	}

	// Iterate tiles
	for (unsigned t = 0; t < tiles_no; t += 1)
	{
		size_t compressed_size = 0;

		unsigned tile_w = 0;
		unsigned tile_h = 0;
		unsigned tile_x = 0;
		unsigned tile_y = 0;
		size_t tile_data_size;

		TileMeasures(t, settings.tiles_dimension, image_w, image_h, tile_w, tile_h, tile_x, tile_y);
		tile_data_size = TileDataSize<TCoeff>(tile_w, tile_h, channels);

		// Feedback
		if (callbacks.generic_event != nullptr)
		{
			callbacks.generic_event(GenericEvent::TileDimensions, t + 1, tile_w, tile_h, {0}, callbacks.user_data);
			callbacks.generic_event(GenericEvent::TilePosition, t + 1, tile_x, tile_y, {0}, callbacks.user_data);
			callbacks.generic_event(GenericEvent::TileDataSize, t + 1, 0, 0, {tile_data_size}, callbacks.user_data);
		}

		// 1. Read and validate tile head
		{
			if ((reinterpret_cast<const uint8_t*>(input) + sizeof(TileHead)) >= input_end)
			{
				status = Status::TruncatedTileHead;
				goto return_failure;
			}

			if ((status = TileHeadRead(*reinterpret_cast<const TileHead*>(input), compressed_size)) != Status::Ok)
				goto return_failure;

			input = reinterpret_cast<const uint8_t*>(input) + sizeof(TileHead);
		}

		// 2. Decompression
		{
			if (callbacks.compression_event != nullptr)
				callbacks.compression_event(settings.compression, t + 1, nullptr, callbacks.user_data);

			if (Decompress(settings, compressed_size, tile_w, tile_h, channels, input,
			               reinterpret_cast<TCoeff*>(workarea[0]), status) != 0)
				goto return_failure;

			input = reinterpret_cast<const uint8_t*>(input) + compressed_size;

			if (callbacks.compression_event != nullptr)
				callbacks.compression_event(settings.compression, t + 1, workarea[0], callbacks.user_data);

			// Developers, developers, developers
			// printf("Hash: %8x \n", Adler32(workarea[0], tile_data_size));
		}

		// 3. Wavelet transformation
		{
			if (callbacks.lifting_event != nullptr)
				callbacks.lifting_event(settings.wavelet, settings.wrap, t + 1, nullptr, callbacks.user_data);

			Unlift(callbacks, settings.wavelet, tile_w, tile_h, channels, reinterpret_cast<TCoeff*>(workarea[0]),
			       reinterpret_cast<TCoeff*>(workarea[1]));

			if (callbacks.lifting_event != nullptr)
				callbacks.lifting_event(settings.wavelet, settings.wrap, t + 1, workarea[1], callbacks.user_data);
		}

		// 4. Format
		{
			if (callbacks.format_event != nullptr)
				callbacks.format_event(settings.color, t + 1, nullptr, callbacks.user_data);

			FormatToRgb(settings.color, tile_w, tile_h, channels, tile_w, reinterpret_cast<TCoeff*>(workarea[1]),
			            reinterpret_cast<TOut*>(workarea[0]));

			if (callbacks.format_event != nullptr)
				callbacks.format_event(settings.color, t + 1, workarea[0], callbacks.user_data);
		}

		// 5. Copy image data
		if (tiles_no != 1) // TODO, Format() is capable of write directly to output
		{
			auto out = reinterpret_cast<TOut*>(image) + (tile_x * channels) + (image_w * channels) * tile_y;

			for (size_t row = 0; row < tile_h; row += 1)
			{
				Memcpy(out, reinterpret_cast<TOut*>(workarea[0]) + row * tile_w * channels,
				       tile_w * channels * sizeof(TOut));
				out += (image_w * channels);
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
		if (workarea[i] != nullptr && workarea[i] != image)
			callbacks.free(workarea[i]);
	}

	if (image != nullptr)
		callbacks.free(image);

	out_status = status;
	return nullptr;
}


void* DecodeEx(const Callbacks& callbacks, size_t input_size, const void* input, unsigned* out_width,
               unsigned* out_height, unsigned* out_channels, unsigned* out_depth, Settings* out_settings,
               Status* out_status)
{
	auto settings = DefaultSettings();
	auto status = Status::Ok;
	void* image_data = nullptr;

	// Checks
	if ((status = ValidateCallbacks(callbacks)) != Status::Ok ||
	    (status = ValidateInput(input, input_size)) != Status::Ok)
		goto return_failure;

	// Read image head
	unsigned width;
	unsigned height;
	unsigned channels;
	unsigned depth;
	{
		if ((status = DecodeHead(input_size, input, &width, &height, &channels, &depth, &settings)) != Status::Ok)
			goto return_failure;

		if (out_width != nullptr)
			*out_width = width;
		if (out_height != nullptr)
			*out_height = height;
		if (out_channels != nullptr)
			*out_channels = channels;
		if (out_depth != nullptr)
			*out_depth = depth;
	}

	// Decode
	{
		auto input_end = reinterpret_cast<const void*>(reinterpret_cast<const uint8_t*>(input) + input_size);
		input = reinterpret_cast<const void*>(reinterpret_cast<const uint8_t*>(input) + sizeof(ImageHead));

		if (depth <= 8)
		{
			image_data = sDecodeInternal<int16_t, uint8_t>(callbacks, settings, width, height, channels, depth, input,
			                                               input_end, status);
		}
		else
		{
			image_data = sDecodeInternal<int32_t, uint16_t>(callbacks, settings, width, height, channels, depth, input,
			                                                input_end, status);
		}
	}

	// Bye!
	if (out_status != nullptr)
		*out_status = status;
	if (out_settings != nullptr)
		*out_settings = settings;

	return image_data;

return_failure:
	if (out_status != nullptr)
		*out_status = status;

	return nullptr;
}

} // namespace ako
