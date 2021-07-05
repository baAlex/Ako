/*-----------------------------

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

-------------------------------

 [misc.c]
 - Alexander Brandt 2021
-----------------------------*/

#include "misc.h"
#include "ako.h"


inline size_t TilesNo(size_t image_w, size_t image_h, size_t tiles_dimension)
{
	size_t tiles_x = (image_w / tiles_dimension);
	size_t tiles_y = (image_h / tiles_dimension);
	tiles_x = (image_w % tiles_dimension != 0) ? (tiles_x + 1) : tiles_x;
	tiles_y = (image_h % tiles_dimension != 0) ? (tiles_y + 1) : tiles_y;

	return (tiles_x * tiles_y);
}


size_t RoundSplit(size_t x)
{
	return (x % 2 == 0) ? (x / 2) : ((x + 1) / 2);
}

inline size_t TileTotalLifts(size_t tile_w, size_t tile_h)
{
	size_t lift = 0;

	while (tile_w > 2 && tile_h > 2)
	{
		tile_w = RoundSplit(tile_w);
		tile_h = RoundSplit(tile_h);
		lift = lift + 1;
	}

	return lift;
}


size_t TileTotalLength(size_t tile_w, size_t tile_h)
{
	size_t length = 0;

	while (tile_w > 2 && tile_h > 2) // For each lift step...
	{
		tile_w = RoundSplit(tile_w);
		tile_h = RoundSplit(tile_h);
		length = length + (tile_w * tile_h) * 3; // Three highpasses...
	}

	return length + (tile_w * tile_h); // And one lowpass
}


static inline size_t sMax(size_t a, size_t b)
{
	return (a > b) ? a : b;
}

size_t WorkareaLength(size_t image_w, size_t image_h, size_t tiles_dimension)
{
	size_t tile_w = tiles_dimension;
	size_t tile_h = tiles_dimension;

	if (image_w < tiles_dimension)
		tile_w = image_w;
	if (image_h < tiles_dimension)
		tile_h = image_h;

	if (image_w == tile_w && image_h == tile_h)
		return TileTotalLength(image_w, image_h);

	// Non square-power-of-two tiles add extra pixels to their sizes
	// This in order of being able of divide by two on lift steps.
	const size_t tiles_x = (image_w / tile_w);
	const size_t tiles_y = (image_h / tile_h);
	const size_t border_tile_w = (image_w - tile_w * tiles_x);
	const size_t border_tile_h = (image_h - tile_h * tiles_y);

	// Border tiles are the "Non square-power-of-two" ones
	return sMax(TileTotalLength(tile_w, tile_h), TileTotalLength(border_tile_w, border_tile_h));
}
