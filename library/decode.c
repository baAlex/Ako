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


AKO_EXPORT uint8_t* akoDecodeExt(const struct akoCallbacks* c, size_t input_size, const void* input,
                                 struct akoSettings* out_s, size_t* out_channels, size_t* out_w, size_t* out_h,
                                 enum akoStatus* out_status)
{
	struct akoSettings s = {0};
	enum akoStatus status;

	size_t channels;
	size_t image_w;
	size_t image_h;

	uint8_t* image = NULL;
	const uint8_t* blob = input;

	void* workarea_a = NULL;
	void* workarea_b = NULL;

	// Check callbacks and input
	const struct akoCallbacks checked_c = (c != NULL) ? *c : akoDefaultCallbacks();

	if (checked_c.malloc == NULL || checked_c.realloc == NULL || checked_c.free == NULL)
	{
		status = AKO_INVALID_CALLBACKS;
		goto return_failure;
	}

	if (input == NULL)
	{
		status = AKO_INVALID_INPUT;
		goto return_failure;
	}

	// Read head
	if ((blob + sizeof(struct akoHead)) > (const uint8_t*)input + sizeof(struct akoHead))
	{
		status = AKO_BROKEN_INPUT;
		goto return_failure;
	}

	if ((status = akoHeadRead(blob, &channels, &image_w, &image_h, &s)) != AKO_OK)
		goto return_failure;

	blob += sizeof(struct akoHead); // Update

	// Allocate workareas and image
	const size_t tiles_no = akoImageTilesNo(image_w, image_h, s.tiles_dimension);
	const size_t tile_total_size = (akoImageMaxTileDataSize(image_w, image_h, s.tiles_dimension) +
	                                akoImageMaxPlanesSpacingSize(image_w, image_h, s.tiles_dimension)) *
	                               channels;

	workarea_a = checked_c.malloc(tile_total_size);
	workarea_b = checked_c.malloc(tile_total_size);

	if (workarea_a == NULL || workarea_b == NULL)
	{
		status = AKO_NO_ENOUGH_MEMORY;
		goto return_failure;
	}

	if (tiles_no > 1) // Recycle
	{
		if ((image = checked_c.malloc(image_w * image_h * channels)) == NULL)
		{
			status = AKO_NO_ENOUGH_MEMORY;
			goto return_failure;
		}
	}
	else
	{
		if (s.wavelet != AKO_WAVELET_NONE)
			image = workarea_a;
		else
			image = workarea_b;
	}

	AKO_DEV_PRINTF("\nD\tTiles no: %zu, Tile total size: %zu\n", tiles_no, tile_total_size);

	// Iterate tiles
	size_t tile_x = 0;
	size_t tile_y = 0;

	size_t out_tile_size;
	size_t planes_spacing = 0; // Space in order to follow the akoDividePlusOneRule()

	for (size_t t = 0; t < tiles_no; t++)
	{
		const size_t tile_w = akoTileDimension(tile_x, image_w, s.tiles_dimension);
		const size_t tile_h = akoTileDimension(tile_y, image_h, s.tiles_dimension);

		if (s.wavelet != AKO_WAVELET_NONE)
		{
			out_tile_size = akoTileDataSize(tile_w, tile_h) * channels;
			planes_spacing = akoPlanesSpacing(tile_w, tile_h);
		}
		else
			out_tile_size = (tile_w * tile_h * channels * sizeof(int16_t));

		// 1. Decompress
		// if (s.compression == AKO_COMPRESSION_NONE)
		{
			sEvent(t, tiles_no, AKO_EVENT_COMPRESSION_START, checked_c.events_data, checked_c.events);

			// Check input
			if ((blob + out_tile_size) > (const uint8_t*)input + input_size)
			{
				status = AKO_BROKEN_INPUT;
				goto return_failure;
			}

			// Copy as is
			for (size_t i = 0; i < out_tile_size; i++)
				((uint8_t*)workarea_a)[i] = blob[i];

			blob += out_tile_size; // Update

			sEvent(t, tiles_no, AKO_EVENT_COMPRESSION_END, checked_c.events_data, checked_c.events);
		}

		// 2. Wavelet transform
		if (s.wavelet != AKO_WAVELET_NONE)
		{
			sEvent(t, tiles_no, AKO_EVENT_WAVELET_START, checked_c.events_data, checked_c.events);
			akoUnlift(t, &s, channels, tile_w, tile_h, planes_spacing, workarea_a, workarea_b);
			sEvent(t, tiles_no, AKO_EVENT_WAVELET_END, checked_c.events_data, checked_c.events);
		}

		// 3. Developers, developers, developers
		// (before the format step destroys workarea a)
		if (t < AKO_DEV_NOISE)
		{
			AKO_DEV_PRINTF(
			    "D\tTile %zu at %zu:%zu, %zux%zu px, planes spacing: %zu, size: %zu bytes, cursor: %zu bytes\n", t,
			    tile_x, tile_y, tile_w, tile_h, planes_spacing, out_tile_size, (size_t)(blob - (const uint8_t*)input));
		}
		else if (t == AKO_DEV_NOISE + 1)
		{
			AKO_DEV_PRINTF("D\t...\n");
		}

		// 4. Format
		{
			sEvent(t, tiles_no, AKO_EVENT_FORMAT_START, checked_c.events_data, checked_c.events);

			int16_t* workarea = (s.wavelet != AKO_WAVELET_NONE) ? workarea_b : workarea_a;
			akoFormatToInterleavedU8Rgb(s.colorspace, channels, tile_w, tile_h, planes_spacing, image_w, workarea,
			                            image + (image_w * tile_y + tile_x) * channels);

			sEvent(t, tiles_no, AKO_EVENT_FORMAT_END, checked_c.events_data, checked_c.events);
		}

		// 5. Next tile
		tile_x += s.tiles_dimension;
		if (tile_x >= image_w)
		{
			tile_x = 0;
			tile_y += s.tiles_dimension;
		}
	}

	// Bye!
	if (workarea_a != image)
		checked_c.free(workarea_a);
	if (workarea_b != image)
		checked_c.free(workarea_b);

	if (out_s != NULL)
		*out_s = s;
	if (out_channels != NULL)
		*out_channels = channels;
	if (out_w != NULL)
		*out_w = image_w;
	if (out_h != NULL)
		*out_h = image_h;

	if (out_status != NULL)
		*out_status = AKO_OK;

	return image;

return_failure:
	if (image != workarea_a && image != workarea_b)
		checked_c.free(image);
	if (workarea_a != NULL)
		checked_c.free(workarea_a);
	if (workarea_b != NULL)
		checked_c.free(workarea_b);
	if (out_status != NULL)
		*out_status = status;

	return NULL;
}
