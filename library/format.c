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
#include "ako.h"


static inline void sToYuv(int16_t r, int16_t g, int16_t b, int16_t* y, int16_t* u, int16_t* v)
{
#if (AKO_COLORSPACE == 0)
	// Rgb
	*y = r;
	*u = g;
	*v = b;
#endif

#if (AKO_COLORSPACE == 1)
	// YCoCg
	// https://en.wikipedia.org/wiki/YCoCg#Conversion_with_the_RGB_color_model
	*y = (r / 4) + (g / 2) + (b / 4);
	*u = (r / 2) - (b / 2);
	*v = -(r / 4) + (g / 2) - (b / 4);
#endif

#if (AKO_COLORSPACE == 2)
	// YCoCg-R (reversible)
	// https://en.wikipedia.org/wiki/YCoCg#The_lifting-based_YCoCg-R_variation
	*u = r - b;
	int16_t tmp = b + *u / 2;
	*v = g - tmp;
	*y = tmp + *v / 2;
#endif
}


static inline void sToRgb(int16_t y, int16_t u, int16_t v, int16_t* r, int16_t* g, int16_t* b)
{
#if (AKO_COLORSPACE == 0)
	// Rgb
	*r = y;
	*g = u;
	*b = v;
#endif

#if (AKO_COLORSPACE == 1)
	// YCoCg
	int16_t tmp = y - v;
	*r = tmp + u;
	*g = y + v;
	*b = tmp - u;
#endif

#if (AKO_COLORSPACE == 2)
	// YCoCg-R (reversible)
	int16_t tmp = y - v / 2;
	*g = v + tmp;
	*b = tmp - u / 2;
	*r = *b + u;
#endif
}


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

	// Color transformation
	if (channels == 3 || channels == 4)
	{
		for (size_t i = 0; i < (dimension * dimension); i++)
		{
			int16_t y, u, v;

			sToYuv(out[i], out[(dimension * dimension * 1) + i], out[(dimension * dimension * 2) + i], &y, &u, &v);

			out[i] = y;
			out[(dimension * dimension * 1) + i] = u;
			out[(dimension * dimension * 2) + i] = v;
		}
	}
}


void FormatToInterlacedU8RGB(size_t dimension, size_t channels, int16_t* in, uint8_t* out)
{
	// Color transformation
	if (channels == 4)
	{
		for (size_t i = 0; i < (dimension * dimension); i++)
		{
			const int16_t a = in[(dimension * dimension * 3) + i];
			int16_t r, g, b;

			sToRgb(in[(dimension * dimension * 0) + i], in[(dimension * dimension * 1) + i],
			       in[(dimension * dimension * 2) + i], &r, &g, &b);

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
			int16_t r, g, b;

			sToRgb(in[(dimension * dimension * 0) + i], in[(dimension * dimension * 1) + i],
			       in[(dimension * dimension * 2) + i], &r, &g, &b);

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
