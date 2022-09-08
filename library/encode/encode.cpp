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

static void* sGrownBlob(const Callbacks& callbacks, size_t new_size, void* blob, size_t& out_current_size)
{
	if (out_current_size >= new_size)
		return blob;

	out_current_size = new_size;
	return BetterRealloc(callbacks, blob, new_size);
}


static void sWriteImageHead(const Settings& settings, unsigned image_w, unsigned image_h, unsigned channels,
                            unsigned depth, ImageHead& out)
{
	int td = 0;
	while ((1 << (td)) < static_cast<int>(settings.tiles_dimension))
		td += 1;

	out.magic = IMAGE_HEAD_MAGIC;

	out.a = static_cast<uint32_t>((image_w - 1) & 0x3FFFFFF) << 6; // 26 bits
	out.a |= static_cast<uint32_t>((depth - 1) & 0x3F);            // 6 bits
	out.b = static_cast<uint32_t>((image_h - 1) & 0x3FFFFFF) << 6; // 26 bits
	out.b |= static_cast<uint32_t>(td & 0x3F);                     // 6 bits

	out.c = 0;
	out.c |= static_cast<uint32_t>(((channels - 1) & 0x1F) << 12); // 5 bits
	out.c |= ((ToNumber(settings.color) & 0x07) << 9);             // 3 bits
	out.c |= ((ToNumber(settings.wavelet) & 0x07) << 6);           // 3 bits
	out.c |= ((ToNumber(settings.wrap) & 0x07) << 3);              // 3 bits
	out.c |= ((ToNumber(settings.compression) & 0x07) << 0);       // 3 bits

	if (SystemEndianness() != Endianness::Little)
	{
		out.magic = EndiannessReverseU32(out.magic);
		out.a = EndiannessReverseU32(out.a);
		out.b = EndiannessReverseU32(out.b);
		out.c = EndiannessReverseU32(out.c);
	}
}


static void sWriteTileHead(unsigned no, size_t compressed_size, TileHead& out)
{
	out.magic = TILE_HEAD_MAGIC;

	out.no = static_cast<uint32_t>(no);
	out.compressed_size = static_cast<uint32_t>(compressed_size); // TODO, check
	out.unused = 0;

	if (SystemEndianness() != Endianness::Little)
	{
		out.magic = EndiannessReverseU32(out.magic);
		out.no = EndiannessReverseU32(out.no);
		out.compressed_size = EndiannessReverseU32(out.compressed_size);
	}
}


template <typename TIn, typename TOut>
static size_t sEncodeInternal(const Callbacks& callbacks, const Settings& settings, unsigned image_w, unsigned image_h,
                              unsigned channels, unsigned depth, const TIn* input, void** output, Status& out_status)
{
	auto status = Status::Ok;

	const unsigned tiles_no = TilesNo(settings.tiles_dimension, image_w, image_h);
	const size_t workarea_size = WorkareaSize<TOut>(settings.tiles_dimension, image_w, image_h, channels);
	const size_t workareas_no = 2;

	void* workarea[workareas_no] = {nullptr, nullptr}; // Where work
	void* blob = nullptr;                              // Where output our work

	size_t blob_cursor = 0;
	size_t blob_size =
	    (tiles_no == 1) ? workarea_size : 0; // With only one tile (the whole image) we can recycle memory

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
		{
			blob = workarea[0];
			workarea[0] = (reinterpret_cast<uint8_t*>(workarea[0]) + sizeof(ImageHead) + sizeof(TileHead)); // HACK
		}
	}

	// Write image head
	{
		if ((blob = sGrownBlob(callbacks, blob_cursor + sizeof(ImageHead), blob, blob_size)) == nullptr)
		{
			status = Status::NoEnoughMemory;
			goto return_failure;
		}

		auto head = reinterpret_cast<ImageHead*>(reinterpret_cast<uint8_t*>(blob) + blob_cursor);
		sWriteImageHead(settings, image_w, image_h, channels, depth, *head);
		blob_cursor += sizeof(ImageHead);
	}

	// Iterate tiles
	for (unsigned t = 0; t < tiles_no; t += 1)
	{
		unsigned tile_w = 0;
		unsigned tile_h = 0;
		unsigned tile_x = 0;
		unsigned tile_y = 0;
		size_t tile_data_size;

		TileMeasures(t, settings.tiles_dimension, image_w, image_h, tile_w, tile_h, tile_x, tile_y);
		tile_data_size = TileDataSize<TOut>(tile_w, tile_h, channels);

		if (callbacks.generic_event != nullptr)
		{
			callbacks.generic_event(GenericEvent::TileDimensions, t + 1, tile_w, tile_h, 0, callbacks.user_data);
			callbacks.generic_event(GenericEvent::TilePosition, t + 1, tile_x, tile_y, 0, callbacks.user_data);
			callbacks.generic_event(GenericEvent::TileDataSize, t + 1, 0, 0, tile_data_size, callbacks.user_data);
		}

		// 1. Format
		{
			if (callbacks.format_event != nullptr)
				callbacks.format_event(settings.color, t + 1, nullptr, callbacks.user_data);

			FormatToInternal(settings.color, settings.discard, tile_w, tile_h, channels, image_w,
			                 input + (tile_x + image_w * tile_y) * channels, reinterpret_cast<TOut*>(workarea[0]));

			if (callbacks.format_event != nullptr)
				callbacks.format_event(settings.color, t + 1, workarea[0], callbacks.user_data);
		}

		// 2. Wavelet transform
		{}

		// 3. Compression
		{}

		// 4. Write tile head
		{
			if ((blob = sGrownBlob(callbacks, blob_cursor + sizeof(TileHead), blob, blob_size)) == nullptr)
			{
				status = Status::NoEnoughMemory;
				goto return_failure;
			}

			auto head = reinterpret_cast<TileHead*>(reinterpret_cast<uint8_t*>(blob) + blob_cursor);
			sWriteTileHead(t, tile_data_size, *head);
			blob_cursor += sizeof(TileHead);
		}

		// 5. Write image data
		{
			if ((blob = sGrownBlob(callbacks, blob_cursor + tile_data_size, blob, blob_size)) == nullptr)
			{
				status = Status::NoEnoughMemory;
				goto return_failure;
			}

			if (tiles_no != 1) // HACK
			{
				auto out = reinterpret_cast<TOut*>(reinterpret_cast<uint8_t*>(blob) + blob_cursor);
				for (size_t i = 0; i < (tile_w * tile_h * channels); i += 1)
					out[i] = reinterpret_cast<TOut*>(workarea[0])[i];
			}

			blob_cursor += tile_data_size;
		}
	}

	// Bye!
	if (tiles_no == 1)
		workarea[0] = (reinterpret_cast<uint8_t*>(workarea[0]) - sizeof(ImageHead) - sizeof(TileHead)); // HACK

	for (size_t i = 0; i < workareas_no; i += 1)
	{
		if (workarea[i] != blob)         // Do not free recycled memory,
			callbacks.free(workarea[i]); // we may need to return it [A]
	}

	if (output != nullptr)
		*output = blob; // [A] Now is user responsibility
	else
	{
		if (blob != nullptr)
			callbacks.free(blob);
	}

	out_status = status;
	return blob_cursor;

return_failure:
	if (tiles_no == 1)
		workarea[0] = (reinterpret_cast<uint8_t*>(workarea[0]) - sizeof(ImageHead) - sizeof(TileHead)); // HACK

	for (size_t i = 0; i < workareas_no; i += 1)
	{
		if (workarea[i] != nullptr && workarea[i] != blob)
			callbacks.free(workarea[i]);
	}

	if (blob != nullptr)
		callbacks.free(blob);

	out_status = status;
	return 0;
}


size_t EncodeEx(const Callbacks& callbacks, const Settings& settings, unsigned width, unsigned height,
                unsigned channels, unsigned depth, const void* input, void** output, Status* out_status)
{
	auto status = Status::Ok;
	size_t blob_size = 0;

	// Checks
	{
		if ((status = ValidateCallbacks(callbacks)) != Status::Ok ||
		    (status = ValidateSettings(settings)) != Status::Ok ||
		    (status = ValidateProperties(width, height, channels, depth)) != Status::Ok ||
		    (status = ValidateInput(input)) != Status::Ok)
		{
			goto return_failure;
		}
	}

	// Encode!
	if (depth <= 8)
	{
		blob_size = sEncodeInternal<uint8_t, int16_t>(callbacks, settings, width, height, channels, depth,
		                                              reinterpret_cast<const uint8_t*>(input), output, status);
	}
	else
	{
		blob_size = sEncodeInternal<uint16_t, int32_t>(callbacks, settings, width, height, channels, depth,
		                                               reinterpret_cast<const uint16_t*>(input), output, status);
	}

	// Bye!
	if (out_status != nullptr)
		*out_status = status;
	return blob_size;

return_failure:
	if (out_status != nullptr)
		*out_status = status;
	return 0;
}

} // namespace ako
