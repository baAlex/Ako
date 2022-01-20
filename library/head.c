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


// FIXME, nothing here will work on big-endian machines
// =====


static inline enum akoStatus sValidate(size_t channels, size_t width, size_t height, size_t tiles_dimension,
                                       enum akoWrap wrap, enum akoWavelet wavelet, enum akoColor color,
                                       enum akoCompression compression)
{
	if (channels > AKO_MAX_CHANNELS)
		return AKO_INVALID_CHANNELS_NO;

	if (width == 0 || height == 0 || width > AKO_MAX_WIDTH || height > AKO_MAX_HEIGHT)
		return AKO_INVALID_DIMENSIONS;

	if (tiles_dimension != 0 &&
	    (tiles_dimension < AKO_MIN_TILES_DIMENSION || tiles_dimension > AKO_MAX_TILES_DIMENSION))
		return AKO_INVALID_TILES_DIMENSIONS;

	if (wrap != AKO_WRAP_CLAMP && wrap != AKO_WRAP_REPEAT && wrap != AKO_WRAP_ZERO)
		return AKO_INVALID_WRAP_MODE;

	if (wavelet != AKO_WAVELET_DD137 && wavelet != AKO_WAVELET_CDF53 && wavelet != AKO_WAVELET_HAAR &&
	    wavelet != AKO_WAVELET_NONE)
		return AKO_INVALID_WAVELET_TRANSFORMATION;

	if (color != AKO_COLOR_YCOCG && color != AKO_COLOR_YCOCG_R && color != AKO_COLOR_SUBTRACT_G &&
	    color != AKO_COLOR_RGB)
		return AKO_INVALID_COLOR_TRANSFORMATION;

	if (compression != AKO_COMPRESSION_ELIAS_RLE && compression != AKO_COMPRESSION_NONE)
		return AKO_INVALID_COMPRESSION_METHOD;

	return AKO_OK;
}


enum akoStatus akoHeadWrite(size_t channels, size_t width, size_t height, const struct akoSettings* s, void* out)
{
	struct akoHead* h = out;

	// To encode tiles dimensions we need some extra bit operations
	size_t binary_tiles_dimension = 0;
	if (s->tiles_dimension != 0)
	{
		for (size_t b = s->tiles_dimension; b > 1; b >>= 1)
			binary_tiles_dimension++;

		if (((size_t)1 << binary_tiles_dimension) != s->tiles_dimension)
			return AKO_INVALID_TILES_DIMENSIONS;

		binary_tiles_dimension -= 2; // As AKO_MIN_TILES_DIMENSION is 8
	}

	// Validate
	const enum akoStatus validation =
	    sValidate(channels, width, height, s->tiles_dimension, s->wrap, s->wavelet, s->color, s->compression);

	if (validation != AKO_OK)
		return validation;

	// Write
	h->magic[0] = 'A';
	h->magic[1] = 'k';
	h->magic[2] = 'o';
	h->version = AKO_FORMAT_VERSION;

	h->width = (uint32_t)width;
	h->height = (uint32_t)height;

	h->flags = (uint32_t)(channels - 1);
	h->flags |= (uint32_t)(s->wrap) << 4;
	h->flags |= (uint32_t)(s->wavelet) << 6;
	h->flags |= (uint32_t)(s->color) << 8;
	h->flags |= (uint32_t)(s->compression) << 10;
	h->flags |= (uint32_t)(binary_tiles_dimension) << 12;

	// Bye!
	return AKO_OK;
}


enum akoStatus akoHeadRead(const void* in, size_t* out_channels, size_t* out_width, size_t* out_height,
                           struct akoSettings* out_s)
{
	const struct akoHead* h = in;

	// Validate
	if (h->magic[0] != 'A' || h->magic[1] != 'k' || h->magic[2] != 'o')
		return AKO_INVALID_MAGIC;

	if (h->version != AKO_FORMAT_VERSION)
		return AKO_UNSUPPORTED_VERSION;

	if ((h->flags >> 15) != 0)
		return AKO_INVALID_FLAGS;

	const size_t channels = (size_t)((h->flags & 0x000F)) + 1;
	const enum akoWrap wrap = (enum akoWrap)((h->flags >> 4) & 0x0003);
	const enum akoWavelet wavelet = (enum akoWavelet)((h->flags >> 6) & 0x0003);
	const enum akoColor color = (enum akoColor)((h->flags >> 8) & 0x0003);
	const enum akoCompression compression = (enum akoCompression)((h->flags >> 10) & 0x0003);

	size_t tiles_dimension = ((h->flags >> 12) & 0x001F);
	if (tiles_dimension != 0)
	{
		if (tiles_dimension < (32 - 2))
			tiles_dimension = (size_t)1 << (tiles_dimension + 2);
		else
			return AKO_INVALID_TILES_DIMENSIONS;
	}

	const enum akoStatus validation =
	    sValidate(channels, (size_t)h->width, (size_t)h->height, tiles_dimension, wrap, wavelet, color, compression);

	if (validation != AKO_OK)
		return validation;

	// Write
	if (out_channels != NULL)
		*out_channels = channels;

	if (out_width != NULL)
		*out_width = (size_t)h->width;

	if (out_height != NULL)
		*out_height = (size_t)h->height;

	if (out_s != NULL)
	{
		out_s->wrap = wrap;
		out_s->wavelet = wavelet;
		out_s->color = color;
		out_s->compression = compression;
		out_s->tiles_dimension = tiles_dimension;
	}

	// Bye!
	return AKO_OK;
}
