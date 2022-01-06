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


uint8_t* akoDecodeExt(const struct akoCallbacks* c, size_t input_size, const void* in, struct akoSettings* out_s,
                      size_t* out_channels, size_t* out_w, size_t* out_h, enum akoStatus* out_status)
{
	struct akoSettings s = {0};
	enum akoStatus status;

	size_t channels;
	size_t image_w;
	size_t image_h;

	wavelet_t* workarea_a = NULL;
	wavelet_t* workarea_b = NULL;

	// Check callbacks
	const struct akoCallbacks checked_c = (c != NULL) ? *c : akoDefaultCallbacks();

	if (checked_c.malloc == NULL || checked_c.realloc == NULL || checked_c.free == NULL)
	{
		status = AKO_INVALID_CALLBACKS;
		goto return_failure;
	}

	// Read head
	if (in == NULL || input_size < sizeof(struct akoHead))
	{
		status = AKO_INVALID_INPUT;
		goto return_failure;
	}

	if ((status = akoHeadRead(in, &channels, &image_w, &image_h, &s)) != AKO_OK)
		goto return_failure;

	// Allocate workareas
	const size_t tiles_no = akoImageTilesNo(image_w, image_h, s.tiles_dimension);
	const size_t tile_max_size = akoImageMaxTileDataSize(image_w, image_h, s.tiles_dimension) * channels;

	workarea_a = checked_c.malloc(tile_max_size);
	workarea_b = checked_c.malloc(tile_max_size);

	if (workarea_a == NULL || workarea_b == NULL)
	{
		status = AKO_NO_ENOUGH_MEMORY;
		goto return_failure;
	}

	DEV_PRINTF("D\tTiles no: %zu, Max tile size: %zu\n", tiles_no, tile_max_size);

	// Iterate tiles
	size_t tile_x = 0;
	size_t tile_y = 0;

	for (size_t t = 0; t < tiles_no; t++)
	{
		const size_t tile_w = akoTileDimension(tile_x, image_w, s.tiles_dimension);
		const size_t tile_h = akoTileDimension(tile_y, image_h, s.tiles_dimension);
		const size_t tile_size = akoTileDataSize(tile_w, tile_h) * channels;

		DEV_PRINTF("D\tTile %zu at %zu:%zu, %zux%zu px, size: %zu bytes\n", t, tile_x, tile_y, tile_w, tile_h,
		           tile_size);

		// Next tile
		tile_x += s.tiles_dimension;
		if (tile_x >= image_w)
		{
			tile_x = 0;
			tile_y += s.tiles_dimension;
		}
	}

	DEV_PRINTF("\n");

	// Bye!
	checked_c.free(workarea_a);
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

	return checked_c.malloc(1); // TODO

return_failure:
	if (workarea_a != NULL)
		checked_c.free(workarea_a);
	if (workarea_b != NULL)
		checked_c.free(workarea_b);
	if (out_status != NULL)
		*out_status = status;

	return NULL;
}
