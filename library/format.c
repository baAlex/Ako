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

 [format.c]
 - Alexander Brandt 2021
-----------------------------*/

#include "format.h"


void FormatToPlanarI16YUV(size_t dimension, size_t channels, const uint8_t* in, int16_t* out)
{
	// De-interlace into planes
	// If there is an alpha channel, the pixels are copied (or not) accordingly
	switch (channels)
	{
	case 4: // Alpha channel
		for (size_t i = 0; i < (dimension * dimension); i++)
			out[(dimension * dimension * 3) + i] = (int16_t)in[i * channels + 3];
		// fallthrough
	case 3:
		for (size_t i = 0; i < (dimension * dimension); i++)
		{
			if (channels < 4 || in[i * channels + 3] != 0)
				out[(dimension * dimension * 2) + i] = (int16_t)in[i * channels + 2];
			else
				out[(dimension * dimension * 2) + i] = 0;
		}
		// fallthrough
	case 2: // Alpha channel if 'channels == 2'
		for (size_t i = 0; i < (dimension * dimension); i++)
		{
			if (channels < 4 || in[i * channels + 3] != 0)
				out[(dimension * dimension * 1) + i] = (int16_t)in[i * channels + 1];
			else
				out[(dimension * dimension * 1) + i] = 0;
		}
		// fallthrough
	case 1:
		for (size_t i = 0; i < (dimension * dimension); i++)
		{
			if (channels == 1 || (channels == 2 && in[i * channels + 1] != 0))
				out[i] = (int16_t)in[i * channels];
			else if (channels < 4 || in[i * channels + 3] != 0)
				out[i] = (int16_t)in[i * channels];
			else
				out[i] = 0;
		}
	}

	// Color transformation, sRgb to YCoCg
	// https://en.wikipedia.org/wiki/YCoCg
	if (channels == 3 || channels == 4)
	{
		for (size_t i = 0; i < (dimension * dimension); i++)
		{
			const int16_t r = out[i];
			const int16_t g = out[(dimension * dimension * 1) + i];
			const int16_t b = out[(dimension * dimension * 2) + i];

			const int16_t y = r / 4 + g / 2 + b / 4;
			const int16_t co = 128 + r / 2 - b / 2;
			const int16_t cg = 128 - (r / 4) + g / 2 - b / 4;

			out[i] = y;
			out[(dimension * dimension * 1) + i] = co;
			out[(dimension * dimension * 2) + i] = cg;
		}
	}
}


void FormatToInterlacedU8RGB(size_t dimension, size_t channels, int16_t* in, uint8_t* out)
{
	// Color transformation, YCoCg to sRgb
	// https://en.wikipedia.org/wiki/YCoCg
	if (channels == 4)
	{
		for (size_t i = 0; i < (dimension * dimension); i++)
		{
			const int16_t y = in[(dimension * dimension * 0) + i];
			const int16_t co = in[(dimension * dimension * 1) + i] - 128;
			const int16_t cg = in[(dimension * dimension * 2) + i] - 128;
			const int16_t a = in[(dimension * dimension * 3) + i];

			const int16_t r = (y - cg + co);
			const int16_t g = (y + cg);
			const int16_t b = (y - cg - co);

			in[(dimension * dimension * 0) + i] = (r > 0) ? (r < 255) ? r : 255 : 0;
			in[(dimension * dimension * 1) + i] = (g > 0) ? (g < 255) ? g : 255 : 0;
			in[(dimension * dimension * 2) + i] = (b > 0) ? (b < 255) ? b : 255 : 0;
			in[(dimension * dimension * 3) + i] = (a > 0) ? (a < 255) ? a : 255 : 0;
		}
	}
	else if (channels == 3)
	{
		for (size_t i = 0; i < (dimension * dimension); i++)
		{
			const int16_t y = in[(dimension * dimension * 0) + i];
			const int16_t co = in[(dimension * dimension * 1) + i] - 128;
			const int16_t cg = in[(dimension * dimension * 2) + i] - 128;

			const int16_t r = (y - cg + co);
			const int16_t g = (y + cg);
			const int16_t b = (y - cg - co);

			in[(dimension * dimension * 0) + i] = (r > 0) ? (r < 255) ? r : 255 : 0;
			in[(dimension * dimension * 1) + i] = (g > 0) ? (g < 255) ? g : 255 : 0;
			in[(dimension * dimension * 2) + i] = (b > 0) ? (b < 255) ? b : 255 : 0;
		}
	}
	else
	{
		for (size_t i = 0; i < (dimension * dimension * channels); i++)
		{
			const int16_t c = in[i];
			in[i] = (c > 0) ? (c < 255) ? c : 255 : 0;
		}
	}

	// Interlace from planes
	switch (channels)
	{
	case 4:
		for (size_t i = 0; i < (dimension * dimension); i++)
			out[i * channels + 3] = (uint8_t)in[(dimension * dimension * 3) + i];
		// fallthrough
	case 3:
		for (size_t i = 0; i < (dimension * dimension); i++)
			out[i * channels + 2] = (uint8_t)in[(dimension * dimension * 2) + i];
		// fallthrough
	case 2:
		for (size_t i = 0; i < (dimension * dimension); i++)
			out[i * channels + 1] = (uint8_t)in[(dimension * dimension * 1) + i];
		// fallthrough
	case 1:
		for (size_t i = 0; i < (dimension * dimension); i++)
			out[i * channels] = (uint8_t)in[i];
	}
}
