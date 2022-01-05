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


#if (AKO_FREESTANDING == 0)
#include <stdlib.h>
#endif


struct akoSettings akoDefaultSettings()
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
struct akoCallbacks akoDefaultCallbacks()
{
	struct akoCallbacks c = {0};
	c.malloc = malloc;
	c.realloc = realloc;
	c.free = free;

	return c;
}

void akoDefaultFree(void* ptr)
{
	free(ptr);
}
#endif


const char* akoStatusString(enum akoStatus status)
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
	default: break;
	}

	return "Unknown status code";
}
