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
		head->magic = IMAGE_HEAD_MAGIC;
		head->width = static_cast<uint32_t>(image_w);
		head->height = static_cast<uint32_t>(image_h);
		head->channels = static_cast<uint32_t>(channels);
		head->depth = static_cast<uint32_t>(depth);
		head->tiles_dimension = static_cast<uint32_t>(settings.tiles_dimension);

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

		std::printf("Tile %2zu, %zux%zu px, at: %4zu, %4zu, size: %zu bytes\n", t, tile_w, tile_h, tile_x, tile_y,
		            tile_size);

		// Write tile head
		{
			if ((blob = sGrownBlob(callbacks, blob_cursor + sizeof(TileHead), blob, blob_size)) == NULL)
			{
				status = Status::NoEnoughMemory;
				goto return_failure;
			}

			auto head = reinterpret_cast<TileHead*>(reinterpret_cast<uint8_t*>(blob) + blob_cursor);
			head->magic = TILE_HEAD_MAGIC;
			head->no = static_cast<uint32_t>(t);

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
		    (status = ValidateProperties(input, width, height, channels, depth)) != Status::Ok)
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
