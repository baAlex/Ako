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


static void sWriteImageHead(const Settings& settings, size_t image_w, size_t image_h, size_t channels, size_t depth,
                            ImageHead& out)
{
	int tsize = 0;
	while ((1 << (tsize)) < static_cast<int>(settings.tiles_dimension))
		tsize += 1;

	out.magic = IMAGE_HEAD_MAGIC;

	out.a = static_cast<uint32_t>((image_w - 1) & 0x3FFFFFF) << 6; // 26 bits
	out.a |= static_cast<uint32_t>((depth - 1) & 0x3F);            // 6 bits
	out.b = static_cast<uint32_t>((image_h - 1) & 0x3FFFFFF) << 6; // 26 bits
	out.b |= static_cast<uint32_t>(tsize & 0x3F);                  // 6 bits

	out.c = 0;
	out.c |= static_cast<uint32_t>(((channels - 1) & 0x1F) << 12); // 5 bits
	out.c |= ((ToNumber(settings.color) & 0x07) << 9);             // 3 bits
	out.c |= ((ToNumber(settings.wavelet) & 0x07) << 6);           // 3 bits
	out.c |= ((ToNumber(settings.wrap) & 0x07) << 3);              // 3 bits
	out.c |= ((ToNumber(settings.compression) & 0x07) << 0);       // 3 bits

	if (SystemEndianness() != Endianness::Little)
	{
		EndiannessReverseU32(out.magic);
		EndiannessReverseU32(out.a);
		EndiannessReverseU32(out.b);
		EndiannessReverseU32(out.c);
	}
}


static void sWriteTileHead(size_t no, size_t size, TileHead& out)
{
	out.magic = TILE_HEAD_MAGIC;

	out.no = static_cast<uint32_t>(no);
	out.size = static_cast<uint32_t>(size); // TODO, check
	out.unused = 0;

	if (SystemEndianness() != Endianness::Little)
	{
		EndiannessReverseU32(out.magic);
		EndiannessReverseU32(out.no);
		EndiannessReverseU32(out.size);
	}
}


template <typename T>
static size_t sEncodeInternal(const Callbacks& callbacks, const Settings& settings, size_t image_w, size_t image_h,
                              size_t channels, size_t depth, const void* input, void** output, Status& out_status)
{
	(void)input;
	auto status = Status::Ok;

	const size_t tiles_no = TilesNo(settings.tiles_dimension, image_w, image_h);
	const size_t workarea_size = WorkareaSize<T>(settings.tiles_dimension, image_w, image_h, channels);
	const size_t workareas_no = 2;

	void* workarea[workareas_no] = {NULL, NULL}; // Where work
	void* blob = NULL;                           // Where output our work

	size_t blob_cursor = 0;
	size_t blob_size =
	    (tiles_no == 1) ? workarea_size : 0; // With only one tile (the whole image) we can recycle memory

	size_t data_size_sum = 0;

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
			blob = workarea[0];
	}

	// Write image head
	{
		if ((blob = sGrownBlob(callbacks, blob_cursor + sizeof(ImageHead), blob, blob_size)) == NULL)
		{
			status = Status::NoEnoughMemory;
			goto return_failure;
		}

		auto head = reinterpret_cast<ImageHead*>(reinterpret_cast<uint8_t*>(blob) + blob_cursor);
		sWriteImageHead(settings, image_w, image_h, channels, depth, *head);
		blob_cursor += sizeof(ImageHead);
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
		data_size_sum += tile_size;

		std::printf(" - E | Tile %zu of %zu,\t%zux%zu px,\tat: %zu, %zu,\tsize: %zu bytes\n", t + 1, tiles_no, tile_w,
		            tile_h, tile_x, tile_y, tile_size);

		// Write tile head
		{
			if ((blob = sGrownBlob(callbacks, blob_cursor + sizeof(TileHead), blob, blob_size)) == NULL)
			{
				status = Status::NoEnoughMemory;
				goto return_failure;
			}

			auto head = reinterpret_cast<TileHead*>(reinterpret_cast<uint8_t*>(blob) + blob_cursor);
			sWriteTileHead(t, tile_size, *head);
			blob_cursor += sizeof(TileHead);
		}

		// Write raw data, just zeros
		{
			if ((blob = sGrownBlob(callbacks, blob_cursor + tile_size, blob, blob_size)) == NULL)
			{
				status = Status::NoEnoughMemory;
				goto return_failure;
			}

			auto data = reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(blob) + blob_cursor);
			for (size_t i = 0; i < (tile_w * tile_h * channels); i += 1)
				data[i] = 0;

			blob_cursor += tile_size;
		}
	}

	std::printf(" - E | Data size: %zu\n", data_size_sum);

	// Bye!
	for (size_t i = 0; i < workareas_no; i += 1)
	{
		if (workarea[i] != blob)         // Do not free recycled memory,
			callbacks.free(workarea[i]); // we may need to return it [A]
	}

	if (output != NULL)
		*output = blob; // [A] Now is user responsibility
	else
	{
		if (blob != NULL)
			callbacks.free(blob);
	}

	out_status = status;
	return blob_cursor;

return_failure:
	for (size_t i = 0; i < workareas_no; i += 1)
	{
		if (workarea[i] != NULL && workarea[i] != blob)
			callbacks.free(workarea[i]);
	}

	if (blob != NULL)
		callbacks.free(blob);

	out_status = status;
	return 0;
}


size_t EncodeEx(const Callbacks& callbacks, const Settings& settings, size_t width, size_t height, size_t channels,
                size_t depth, const void* input, void** output, Status& out_status)
{
	// Checks
	{
		auto status = Status::Ok;

		if ((status = ValidateCallbacks(callbacks)) != Status::Ok ||
		    (status = ValidateSettings(settings)) != Status::Ok ||
		    (status = ValidateProperties(width, height, channels, depth)) != Status::Ok ||
		    (status = ValidateInput(input)) != Status::Ok)
		{
			out_status = status;
			return 0;
		}
	}

	// Encode!
	if (depth <= 8)
		return sEncodeInternal<uint16_t>(callbacks, settings, width, height, channels, depth, input, output,
		                                 out_status);

	return sEncodeInternal<uint32_t>(callbacks, settings, width, height, channels, depth, input, output, out_status);
}

} // namespace ako
