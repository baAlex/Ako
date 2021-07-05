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


void FormatToPlanarI16YUV(size_t w, size_t h, size_t channels, size_t planes_space, size_t in_pitch, const uint8_t* in,
                          int16_t* out)
{
	// De-interlace into planes
	// From here 'out' is an image on its own, of
	// 'width * height' dimensions with no need of a pitch
	{
		int16_t* planar_out = out;
		in_pitch = in_pitch * channels;

		for (size_t ch = 0; ch < channels; ch++)
		{
			for (size_t row = 0; row < (h * in_pitch); row += in_pitch)
				for (size_t col = 0; col < (w * channels); col += channels)
				{
					*planar_out = (int16_t)in[row + col + ch];
					planar_out = planar_out + 1;
				}

			planar_out = planar_out + planes_space;
		}
	}

	// Set to zero pixels behind an alpha of zero
	if (channels == 4)
	{
		for (size_t i = 0; i < (w * h); i++)
		{
			if (out[((w * h) + planes_space) * 3 + i] == 0) // Alpha
			{
				out[((w * h) + planes_space) * 0 + i] = 0; // RGB
				out[((w * h) + planes_space) * 1 + i] = 0;
				out[((w * h) + planes_space) * 2 + i] = 0;
			}
		}
	}
	else if (channels == 2)
	{
		for (size_t i = 0; i < (w * h); i++)
		{
			if (out[((w * h) + planes_space) * 1 + i] == 1) // Alpha
				out[((w * h) + planes_space) * 0 + i] = 0;  // Gray
		}
	}

	// Color transformation
	if (channels == 3 || channels == 4)
	{
		for (size_t i = 0; i < (w * h); i++)
		{
			int16_t y, u, v;

			sToYuv(out[((w * h) + planes_space) * 0 + i], out[((w * h) + planes_space) * 1 + i],
			       out[((w * h) + planes_space) * 2 + i], &y, &u, &v);

			out[((w * h) + planes_space) * 0 + i] = y;
			out[((w * h) + planes_space) * 1 + i] = u;
			out[((w * h) + planes_space) * 2 + i] = v;
		}
	}
}


void FormatToInterlacedU8RGB(size_t w, size_t h, size_t channels, size_t planes_space, size_t out_pitch, int16_t* in,
                             uint8_t* out)
{
#if (AKO_WAVELET == 0)
	size_t in_pitch = w;
#else
	size_t in_pitch = (w % 2 == 0) ? w : w + 1; // TODO, find a better place where put this
#endif

	size_t plane = w * h + planes_space;

	// Color transformation
	if (channels == 4)
	{
		for (size_t row = 0; row < h; row++)
			for (size_t col = 0; col < w; col++)
			{
				const int16_t a = in[(row * in_pitch) + col + plane * 3];
				int16_t r, g, b;

				sToRgb(in[(row * in_pitch) + col + plane * 0], in[(row * in_pitch) + col + plane * 1],
				       in[(row * in_pitch) + col + plane * 2], &r, &g, &b);

				in[(row * in_pitch) + col + plane * 0] = (r > 0) ? (r < 255) ? r : 255 : 0;
				in[(row * in_pitch) + col + plane * 1] = (g > 0) ? (g < 255) ? g : 255 : 0;
				in[(row * in_pitch) + col + plane * 2] = (b > 0) ? (b < 255) ? b : 255 : 0;
				in[(row * in_pitch) + col + plane * 3] = (a > 0) ? (a < 255) ? a : 255 : 0;
			}
	}
	else if (channels == 3)
	{
		for (size_t row = 0; row < h; row++)
			for (size_t col = 0; col < w; col++)
			{
				int16_t r, g, b;

				sToRgb(in[(row * in_pitch) + col + plane * 0], in[(row * in_pitch) + col + plane * 1],
				       in[(row * in_pitch) + col + plane * 2], &r, &g, &b);

				in[(row * in_pitch) + col + plane * 0] = (r > 0) ? (r < 255) ? r : 255 : 0;
				in[(row * in_pitch) + col + plane * 1] = (g > 0) ? (g < 255) ? g : 255 : 0;
				in[(row * in_pitch) + col + plane * 2] = (b > 0) ? (b < 255) ? b : 255 : 0;
			}
	}
	else
	{
		for (size_t row = 0; row < h; row++)
			for (size_t col = 0; col < w; col++)
			{
				const int16_t c = in[(row * in_pitch) + col];
				in[(row * in_pitch) + col] = (c > 0) ? (c < 255) ? c : 255 : 0;
			}
	}

	// Interlace from planes
	out_pitch = out_pitch * channels;

	for (size_t row = 0; row < h; row++)
		for (size_t col = 0; col < w; col++)
		{
			for (size_t ch = 0; ch < channels; ch++)
				out[(row * out_pitch) + col * channels + ch] = (uint8_t)in[(row * in_pitch) + col + plane * ch];
		}
}
