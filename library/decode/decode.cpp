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

static Status sReadTileHead(const TileHead& head_raw, size_t& out_compressed_size)
{
	TileHead head = head_raw;

	if (SystemEndianness() != Endianness::Little)
	{
		head.magic = EndiannessReverseU32(head.magic);
		head.no = EndiannessReverseU32(head.no);
		head.compressed_size = EndiannessReverseU32(head.compressed_size);
	}

	if (head.magic != TILE_HEAD_MAGIC || head.compressed_size == 0)
		return Status::InvalidTileHead;

	out_compressed_size = head.compressed_size;
	return Status::Ok;
}


template <typename T, typename TO>
static void* sDecodeInternal(const Callbacks& callbacks, const Settings& settings, unsigned image_w, unsigned image_h,
                             unsigned channels, unsigned depth, const void* input, const void* input_end,
                             Status& out_status)
{
	(void)depth; // Implicit in T and TO

	auto status = Status::Ok;

	const unsigned tiles_no = TilesNo(settings.tiles_dimension, image_w, image_h);
	const size_t workarea_size = WorkareaSize<T>(settings.tiles_dimension, image_w, image_h, channels);
	const size_t workareas_no = 2;

	void* workarea[workareas_no] = {nullptr, nullptr}; // Where work
	void* image = nullptr;                             // Where output our work

	if (callbacks.generic_event != nullptr)
	{
		callbacks.generic_event(GenericEvent::ImageDimensions, image_w, image_h, 0, 0, callbacks.user_data);
		callbacks.generic_event(GenericEvent::ImageChannels, channels, 0, 0, 0, callbacks.user_data);
		callbacks.generic_event(GenericEvent::ImageDepth, depth, 0, 0, 0, callbacks.user_data);

		callbacks.generic_event(GenericEvent::TilesNo, tiles_no, 0, 0, 0, callbacks.user_data);
		callbacks.generic_event(GenericEvent::TilesDimension, settings.tiles_dimension, 0, 0, 0, callbacks.user_data);

		callbacks.generic_event(GenericEvent::WorkareaSize, 0, 0, 0, workarea_size, callbacks.user_data);
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
		}

		if (tiles_no == 1)
			image = workarea[0];
		else
		{
			if ((image = callbacks.malloc((image_w * image_h * channels) * sizeof(TO))) == nullptr)
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
		tile_data_size = TileDataSize<T>(tile_w, tile_h, channels);

		if (callbacks.generic_event != nullptr)
		{
			callbacks.generic_event(GenericEvent::TileDimensions, t + 1, tile_w, tile_h, 0, callbacks.user_data);
			callbacks.generic_event(GenericEvent::TilePosition, t + 1, tile_x, tile_y, 0, callbacks.user_data);
			callbacks.generic_event(GenericEvent::TileDataSize, t + 1, 0, 0, tile_data_size, callbacks.user_data);
		}

		// Compression
		{
			if (callbacks.compression_event != nullptr)
				callbacks.compression_event(settings.compression, t + 1, 0, callbacks.user_data);
		}

		// Read and validate tile head
		{
			if ((reinterpret_cast<const uint8_t*>(input) + sizeof(TileHead)) >= input_end)
			{
				status = Status::TruncatedTileHead;
				goto return_failure;
			}

			if ((status = sReadTileHead(*reinterpret_cast<const TileHead*>(input), compressed_size)) != Status::Ok)
				goto return_failure;

			input = reinterpret_cast<const uint8_t*>(input) + sizeof(TileHead);
			input = reinterpret_cast<const uint8_t*>(input) + compressed_size;
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
		if (workarea[i] != nullptr && workarea[i] != image)
			callbacks.free(workarea[i]);
	}

	if (image != nullptr)
		callbacks.free(image);

	out_status = status;
	return nullptr;
}


static Status sReadImageHead(const ImageHead& head_raw, Settings& out_settings, unsigned& out_width,
                             unsigned& out_height, unsigned& out_channels, unsigned& out_depth)
{
	auto settings = DefaultSettings();
	auto status = Status::Ok;

	ImageHead head = head_raw;

	if (SystemEndianness() != Endianness::Little)
	{
		head.magic = EndiannessReverseU32(head.magic);
		head.a = EndiannessReverseU32(head.a);
		head.b = EndiannessReverseU32(head.b);
		head.c = EndiannessReverseU32(head.c);
	}

	if (head.magic != IMAGE_HEAD_MAGIC)
		return Status::NotAnAkoFile;

	const auto width = static_cast<unsigned>((head.a >> 6) & 0x3FFFFFF) + 1;  // 26 bits
	const auto depth = static_cast<unsigned>((head.a >> 0) & 0x3F) + 1;       // 6 bits
	const auto height = static_cast<unsigned>((head.b >> 6) & 0x3FFFFFF) + 1; // 26 bits
	const auto channels = static_cast<unsigned>((head.c >> 12) & 0x1F) + 1;   // 5 bits
	const auto td = static_cast<unsigned>((head.b >> 0) & 0x3F);              // 6 bits

	settings.tiles_dimension = 0;
	if (td != 0)
		settings.tiles_dimension = 1 << td;

	settings.color = ToColor(static_cast<uint32_t>(head.c >> 9) & 0x07, status); // 3 bits
	if (status != Status::Ok)
		return status;

	settings.wavelet = ToWavelet(static_cast<uint32_t>(head.c >> 6) & 0x07, status); // 3 bits
	if (status != Status::Ok)
		return status;

	settings.wrap = ToWrap(static_cast<uint32_t>(head.c >> 3) & 0x07, status); // 3 bits
	if (status != Status::Ok)
		return status;

	settings.compression = ToCompression(static_cast<uint32_t>(head.c >> 0) & 0x07, status); // 3 bits
	if (status != Status::Ok)
		return status;

	if ((status = ValidateProperties(width, height, channels, depth)) != Status::Ok ||
	    (status = ValidateSettings(settings)) != Status::Ok)
		return status;

	// All validated, return what we have
	out_settings = settings;
	out_width = width;
	out_height = height;
	out_channels = channels;
	out_depth = depth;

	return status;
}


void* DecodeEx(const Callbacks& callbacks, size_t input_size, const void* input, unsigned& out_width,
               unsigned& out_height, unsigned& out_channels, unsigned& out_depth, Settings* out_settings,
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
	{
		if (input_size < sizeof(ImageHead))
		{
			status = Status::TruncatedImageHead;
			goto return_failure;
		}

		// Read, validate and set settings, width, height, etc
		if ((status = sReadImageHead(*reinterpret_cast<const ImageHead*>(input), settings, out_width, out_height,
		                             out_channels, out_depth)) != Status::Ok)
			goto return_failure;
	}

	// Decode
	{
		auto input_end = reinterpret_cast<const void*>(reinterpret_cast<const uint8_t*>(input) + input_size);
		input = reinterpret_cast<const void*>(reinterpret_cast<const uint8_t*>(input) + sizeof(ImageHead));

		if (out_depth <= 8)
		{
			image_data = sDecodeInternal<uint16_t, uint8_t>(callbacks, settings, out_width, out_height, out_channels,
			                                                out_depth, input, input_end, status);
		}
		else
		{
			image_data = sDecodeInternal<uint32_t, uint16_t>(callbacks, settings, out_width, out_height, out_channels,
			                                                 out_depth, input, input_end, status);
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
