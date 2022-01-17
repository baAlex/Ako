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


AKO_EXPORT struct akoSettings akoDefaultSettings()
{
	struct akoSettings s = {0};

	s.wrap = AKO_WRAP_CLAMP;
	s.wavelet = AKO_WAVELET_DD137;
	s.colorspace = AKO_COLORSPACE_YCOCG;
	s.tiles_dimension = 0;

	for (size_t i = 0; i < AKO_MAX_CHANNELS; i++)
		s.quantization[i] = 1.0F;

	s.discard_transparent_pixels = 0;

	return s;
}


#if (AKO_FREESTANDING == 0)
AKO_EXPORT struct akoCallbacks akoDefaultCallbacks()
{
	struct akoCallbacks c = {0};
	c.malloc = malloc;
	c.realloc = realloc;
	c.free = free;

	c.events = NULL;
	c.events_data = NULL;

	return c;
}

AKO_EXPORT void akoDefaultFree(void* ptr)
{
	free(ptr);
}
#endif


AKO_EXPORT const char* akoStatusString(enum akoStatus status)
{
	switch (status)
	{
	case AKO_OK: return "Everything Ok!";
	case AKO_ERROR: return "Something went wrong";
	case AKO_INVALID_CHANNELS_NO: return "Invalid channels number";
	case AKO_INVALID_DIMENSIONS: return "Invalid dimensions";
	case AKO_INVALID_TILES_DIMENSIONS: return "Invalid tiles dimensions";
	case AKO_INVALID_WRAP: return "Invalid wrap mode";
	case AKO_INVALID_WAVELET: return "Invalid wavelet";
	case AKO_INVALID_COLORSPACE: return "Invalid colorspace";
	case AKO_INVALID_INPUT: return "Invalid input";
	case AKO_INVALID_CALLBACKS: return "Invalid callbacks";
	case AKO_INVALID_MAGIC: return "Invalid magic (not an Ako file)";
	case AKO_UNSUPPORTED_VERSION: return "Unsupported version";
	case AKO_NO_ENOUGH_MEMORY: return "No enough memory";
	case AKO_INVALID_FLAGS: return "Invalid flags";
	case AKO_BROKEN_INPUT: return "Broken input/premature end";
	default: break;
	}

	return "Unknown status code";
}


inline size_t akoDividePlusOneRule(size_t v)
{
	return (v % 2 == 0) ? (v / 2) : ((v + 1) / 2);
}


inline size_t akoPlanesSpacing(size_t tile_w, size_t tile_h)
{
	return tile_w * 2 + tile_h * 2;
}


inline size_t akoImageMaxPlanesSpacingSize(size_t image_w, size_t image_h, size_t tiles_dimension)
{
	return sizeof(int16_t) * akoPlanesSpacing(akoTileDimension(0, image_w, tiles_dimension),
	                                          akoTileDimension(0, image_h, tiles_dimension));
}


size_t akoTileDataSize(size_t tile_w, size_t tile_h)
{
	/*
	    If 'tile_w' and 'tile_h' equals, are a power-of-two (2, 4, 8, 16, etc), and
	    function 'log2' operates on integers whitout rounding errors; then:

	    TileDataSize(d) = (d * d * W + log2(d / 2) * L + T)

	    Where, d = Tile dimension (either 'tile_w' or 'tile_h')
	           W = Size of wavelet coefficient
	           L = Size of lift head
	           T = Size of tile head

	    If not, is a recursive function, where we add 1 to tile dimensions
	    on lift steps that are not divisible by 2 (DividePlusOne rule). Lift
	    steps with their respective head. And finally one tile head at the end.

	    This later approach is used in below C code.
	*/

	size_t size = 0;

	while (tile_w > 2 && tile_h > 2) // For each lift step...
	{
		tile_w = akoDividePlusOneRule(tile_w);
		tile_h = akoDividePlusOneRule(tile_h);
		size += (tile_w * tile_h) * sizeof(int16_t) * 3; // Three highpasses...
		size += sizeof(struct akoLiftHead);              // One lift head...
	}

	size += (tile_w * tile_h) * sizeof(int16_t); // ... And one lowpass

	return size;
}


size_t akoTileDimension(size_t tile_pos, size_t image_d, size_t tiles_dimension)
{
	if (tiles_dimension == 0)
		return image_d;

	if (tile_pos + tiles_dimension > image_d)
		return (image_d % tiles_dimension);

	return tiles_dimension;
}


static inline size_t sMax(size_t a, size_t b)
{
	return (a > b) ? a : b;
}

static inline size_t sMin(size_t a, size_t b)
{
	return (a < b) ? a : b;
}

size_t akoImageMaxTileDataSize(size_t image_w, size_t image_h, size_t tiles_dimension)
{
	if (tiles_dimension == 0 || (tiles_dimension >= image_w && tiles_dimension >= image_h))
		return akoTileDataSize(image_w, image_h);

	if ((image_w % tiles_dimension) == 0 && (image_h % tiles_dimension) == 0)
		return akoTileDataSize(tiles_dimension, tiles_dimension);

	// Tiles of varying size on image borders
	// (in this horrible way because TileDataSize() is recursive, and my brain hurts)
	const size_t a = akoTileDataSize(tiles_dimension, tiles_dimension);
	const size_t b = akoTileDataSize(sMin(tiles_dimension, (image_w % tiles_dimension)), tiles_dimension);
	const size_t c = akoTileDataSize(tiles_dimension, sMin(tiles_dimension, (image_h % tiles_dimension)));

	return sMax(sMax(a, b), c);
}


size_t akoImageTilesNo(size_t image_w, size_t image_h, size_t tiles_dimension)
{
	if (tiles_dimension == 0)
		return 1;

	size_t tiles_x = (image_w / tiles_dimension);
	size_t tiles_y = (image_h / tiles_dimension);
	tiles_x = (image_w % tiles_dimension != 0) ? (tiles_x + 1) : tiles_x;
	tiles_y = (image_h % tiles_dimension != 0) ? (tiles_y + 1) : tiles_y;

	return (tiles_x * tiles_y);
}
