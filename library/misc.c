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


#include "ako-private.h"


AKO_EXPORT struct akoSettings akoDefaultSettings()
{
	struct akoSettings s = {0};

	s.wavelet = AKO_WAVELET_DD137;
	s.color = AKO_COLOR_YCOCG;
	s.wrap = AKO_WRAP_CLAMP;
	s.compression = AKO_COMPRESSION_KAGARI;
	s.tiles_dimension = 0;

	s.quantization = 16;
	s.gate = 0;

	s.chroma_loss = 1;
	s.discard_non_visible = 0;

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
	case AKO_INVALID_WRAP_MODE: return "Invalid wrap mode";
	case AKO_INVALID_WAVELET_TRANSFORMATION: return "Invalid wavelet transformation";
	case AKO_INVALID_COLOR_TRANSFORMATION: return "Invalid color transformation";
	case AKO_INVALID_COMPRESSION_METHOD: return "Invalid compression method";
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

	// If 'tile_w' and 'tile_h' equals, are a power-of-two (2, 4, 8, 16, etc), and
	// function 'log2' operates on integers without rounding errors; then:

	// TileDataSize(d) = (d * d * W + log2(d / 2) * L + T)

	// Where, d = Tile dimension (either 'tile_w' or 'tile_h')
	//        W = Size of wavelet coefficient
	//        L = Size of lift head
	//        T = Size of tile head

	// If not, is a recursive function, where we add 1 to tile dimensions
	// on lift steps that are not divisible by 2 (DividePlusOne rule). Lift
	// steps with their respective head. And finally one tile head at the end.

	// This later approach is used in below C code.

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


static void sLiftTargetDimensions(size_t current_w, size_t current_h, size_t tile_w, size_t tile_h, size_t* out_w,
                                  size_t* out_h)
{
	size_t prev_tile_w = tile_w;
	size_t prev_tile_h = tile_h;

	while (tile_w > current_w && tile_h > current_h)
	{
		prev_tile_w = tile_w;
		prev_tile_h = tile_h;

		if (tile_w <= 2 || tile_h <= 2)
			break;

		tile_w = akoDividePlusOneRule(tile_w);
		tile_h = akoDividePlusOneRule(tile_h);
	}

	*out_w = prev_tile_w;
	*out_h = prev_tile_h;
}


void* akoIterateLifts(const struct akoSettings* s, size_t channels, size_t tile_w, size_t tile_h, void* input,
                      void (*lp_callback)(const struct akoSettings*, size_t ch, size_t tile_w, size_t tile_h,
                                          size_t target_w, size_t target_h, coeff_t* input_lp, void* user_data),
                      void (*hp_callback)(const struct akoSettings*, size_t ch, size_t tile_w, size_t tile_h,
                                          const struct akoLiftHead*, size_t current_w, size_t current_h,
                                          size_t target_w, size_t target_h, coeff_t* aux, coeff_t* hp_c, coeff_t* hp_b,
                                          coeff_t* hp_d, void* user_data),
                      void* user_data)
{
	size_t target_w = 0;
	size_t target_h = 0;
	uint8_t* in = (uint8_t*)input;

	sLiftTargetDimensions(0, 0, tile_w, tile_h, &target_w, &target_h);

	// Lowpasses
	for (size_t ch = 0; ch < channels; ch++)
	{
		// Let the user do something with lowpass coefficients
		lp_callback(s, ch, tile_w, tile_h, target_w, target_h, (coeff_t*)in, user_data);

		// Adjust input (by one lowpass)
		in += (target_w * target_h) * sizeof(coeff_t);
	}

	// Highpasses
	struct akoLiftHead* head;

	while (target_w < tile_w && target_h < tile_h)
	{
		const size_t current_w = target_w;
		const size_t current_h = target_h;
		sLiftTargetDimensions(target_w, target_h, tile_w, tile_h, &target_w, &target_h);

		// Iterate in Yuv order
		for (size_t ch = 0; ch < channels; ch++)
		{
			// Lift head
			{
				head = (struct akoLiftHead*)in;

				// Adjust input (by one lift head)
				in += sizeof(struct akoLiftHead);
			}

			// Let the user do something with highpass coefficients
			coeff_t* hp_c = (coeff_t*)in + (current_w * current_h) * 0;
			coeff_t* hp_b = (coeff_t*)in + (current_w * current_h) * 1;
			coeff_t* hp_d = (coeff_t*)in + (current_w * current_h) * 2;

			hp_callback(s, ch, tile_w, tile_h, head, current_w, current_h, target_w, target_h, input, hp_c, hp_b, hp_d,
			            user_data);

			// Adjust input (by three highpasses)
			in += (current_w * current_h) * sizeof(coeff_t) * 3;
		}
	}

	return in;
}
