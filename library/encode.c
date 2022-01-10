/*

MIT License

Copyright (c) 2021 Alexander Brandt

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


#include "ako-private.h"


static inline void sEvent(size_t tile_no, size_t total_tiles, enum akoEvent event, void* data,
                          void (*callback)(size_t, size_t, enum akoEvent, void*))
{
	if (callback != NULL)
		callback(tile_no, total_tiles, event, data);
}


AKO_EXPORT size_t akoEncodeExt(const struct akoCallbacks* c, const struct akoSettings* s, size_t channels,
                               size_t image_w, size_t image_h, const void* in, void** out, enum akoStatus* out_status)
{
	enum akoStatus status;

	size_t blob_size = 0;
	uint8_t* blob = NULL;

	void* workarea_a = NULL;
	void* workarea_b = NULL;

	// Check callbacks, settings and input
	const struct akoCallbacks checked_c = (c != NULL) ? *c : akoDefaultCallbacks();
	const struct akoSettings checked_s = (s != NULL) ? *s : akoDefaultSettings();

	if (checked_c.malloc == NULL || checked_c.realloc == NULL || checked_c.free == NULL)
	{
		status = AKO_INVALID_CALLBACKS;
		goto return_failure;
	}

	if (in == NULL)
	{
		status = AKO_INVALID_INPUT;
		goto return_failure;
	}

	// Allocate blob
	blob_size = sizeof(struct akoHead);

	if ((blob = checked_c.malloc(blob_size)) == NULL)
	{
		status = AKO_NO_ENOUGH_MEMORY;
		goto return_failure;
	}

	// Write head
	if ((status = akoHeadWrite(channels, image_w, image_h, &checked_s, blob)) != AKO_OK)
		goto return_failure;

	// Allocate workareas
	const size_t tiles_no = akoImageTilesNo(image_w, image_h, checked_s.tiles_dimension);
	const size_t tile_max_size = akoImageMaxTileDataSize(image_w, image_h, checked_s.tiles_dimension) * channels;

	workarea_a = checked_c.malloc(tile_max_size);
	workarea_b = checked_c.malloc(tile_max_size);

	if (workarea_a == NULL || workarea_b == NULL)
	{
		status = AKO_NO_ENOUGH_MEMORY;
		goto return_failure;
	}

	// AKO_DEV_PRINTF("E\tTiles no: %zu, Max tile size: %zu\n", tiles_no, tile_max_size);

	// Iterate tiles
	size_t tile_x = 0;
	size_t tile_y = 0;

	for (size_t t = 0; t < tiles_no; t++)
	{
		const size_t tile_w = akoTileDimension(tile_x, image_w, checked_s.tiles_dimension);
		const size_t tile_h = akoTileDimension(tile_y, image_h, checked_s.tiles_dimension);
		const size_t tile_size =
		    (checked_s.wavelet != AKO_WAVELET_NONE)
		        ? (akoTileDataSize(tile_w, tile_h) * channels)
		        : (tile_w * tile_h * channels * sizeof(int16_t)); // No wavelet is a rare case so those 16 bits
		                                                          // while wasting space, are a requirement as is the
		                                                          // format in which following steps operate

		// 1. Format
		sEvent(t, tiles_no, AKO_EVENT_FORMAT_START, checked_c.events_data, checked_c.events);
		{
			akoFormatToPlanarI16Yuv(checked_s.discard_transparent_pixels, checked_s.colorspace, channels, tile_w,
			                        tile_h, image_w, 0, (const uint8_t*)in + ((image_w * tile_y) + tile_x) * channels,
			                        workarea_a);
		}
		sEvent(t, tiles_no, AKO_EVENT_FORMAT_END, checked_c.events_data, checked_c.events);

		// 2. Wavelet transform
		sEvent(t, tiles_no, AKO_EVENT_WAVELET_START, checked_c.events_data, checked_c.events);
		// if (checked_s.wavelet != AKO_WAVELET_NONE)
		{
			// TODO
		}
		sEvent(t, tiles_no, AKO_EVENT_WAVELET_END, checked_c.events_data, checked_c.events);

		// 3. Compress
		sEvent(t, tiles_no, AKO_EVENT_COMPRESSION_START, checked_c.events_data, checked_c.events);
		// if (checked_s.compression == AKO_COMPRESSION_NONE)
		{
			// Make space
			void* updated_blob = checked_c.realloc(blob, blob_size + tile_size);
			if (updated_blob == NULL)
			{
				status = AKO_NO_ENOUGH_MEMORY;
				goto return_failure;
			}

			// Copy as is
			blob = updated_blob;
			for (size_t i = 0; i < tile_size; i++)
				blob[blob_size + i] = ((uint8_t*)workarea_a)[i];

			blob_size += tile_size; // Update
		}
		sEvent(t, tiles_no, AKO_EVENT_COMPRESSION_END, checked_c.events_data, checked_c.events);

		// 4. Developers, developers, developers
		if (t < 10)
		{
			// AKO_DEV_PRINTF("E\tTile %zu at %zu:%zu, %zux%zu px, size: %zu bytes, blob size: %zu bytes\n", t, tile_x,
			//                tile_y, tile_w, tile_h, tile_size, blob_size);

			// char filename[64];
			// for (size_t ch = 0; ch < channels; ch++)
			// {
			// 	snprintf(filename, 64, "Tile%zuCh%zuE.pgm", t, ch);
			// 	akoSavePgmI16(tile_w, tile_h, tile_w, ((int16_t*)workarea_a) + (tile_w * tile_h) * ch, filename);
			// }
		}
		else if (t == 11)
		{
			// AKO_DEV_PRINTF("E\t...\n");
		}

		// 5. Next tile
		tile_x += checked_s.tiles_dimension;
		if (tile_x >= image_w)
		{
			tile_x = 0;
			tile_y += checked_s.tiles_dimension;
		}
	}

	// AKO_DEV_PRINTF("\n");

	// Bye!
	checked_c.free(workarea_a);
	checked_c.free(workarea_b);

	if (out_status != NULL)
		*out_status = AKO_OK;

	if (out != NULL)
		*out = blob;
	else
		checked_c.free(blob); // Discard encoded data

	return blob_size;

return_failure:
	if (workarea_a != NULL)
		checked_c.free(workarea_a);
	if (workarea_b != NULL)
		checked_c.free(workarea_b);
	if (out_status != NULL)
		*out_status = status;
	if (blob != NULL)
		checked_c.free(blob);

	return 0;
}
