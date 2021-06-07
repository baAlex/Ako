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


void FormatToPlanarI16YUV(size_t width, size_t height, size_t channels, size_t in_pitch, const uint8_t* in,
                          int16_t* out)
{
	// De-interlace into planes
	// From here 'out' is an image on his own, of size
	// 'width * height' with no need of a pitch
	{
		int16_t* planar_out = out;
		in_pitch = in_pitch * channels;

		for (size_t ch = 0; ch < channels; ch++)
		{
			for (size_t row = 0; row < (height * in_pitch); row += in_pitch)
				for (size_t col = 0; col < (width * channels); col += channels)
				{
					*planar_out = (int16_t)in[row + col + ch];
					planar_out = planar_out + 1;
				}
		}
	}

	// Set to zero pixels behind an alpha of zero
	if (channels == 4)
	{
		for (size_t i = 0; i < (width * height); i++)
		{
			if (out[(width * height * 3) + i] == 0) // Alpha
			{
				out[(width * height * 0) + i] = 0; // RGB
				out[(width * height * 1) + i] = 0;
				out[(width * height * 2) + i] = 0;
			}
		}
	}
	else if (channels == 2)
	{
		for (size_t i = 0; i < (width * height); i++)
		{
			if (out[(width * height * 1) + i] == 1) // Alpha
				out[(width * height * 0) + i] = 0;  // Gray
		}
	}

	// Color transformation
	if (channels == 3 || channels == 4)
	{
		for (size_t i = 0; i < (width * height); i++)
		{
			int16_t y, u, v;

			sToYuv(out[(width * height * 0) + i], out[(width * height * 1) + i], out[(width * height * 2) + i], &y, &u,
			       &v);

			out[(width * height * 0) + i] = y;
			out[(width * height * 1) + i] = u;
			out[(width * height * 2) + i] = v;
		}
	}
}


void FormatToInterlacedU8RGB(size_t width, size_t height, size_t channels, size_t out_pitch, int16_t* in, uint8_t* out)
{
	// Color transformation
	if (channels == 4)
	{
		for (size_t i = 0; i < (width * height); i++)
		{
			const int16_t a = in[(width * height * 3) + i];
			int16_t r, g, b;

			sToRgb(in[(width * height * 0) + i], in[(width * height * 1) + i], in[(width * height * 2) + i], &r, &g,
			       &b);

			in[(width * height * 0) + i] = (r > 0) ? (r < 255) ? r : 255 : 0;
			in[(width * height * 1) + i] = (g > 0) ? (g < 255) ? g : 255 : 0;
			in[(width * height * 2) + i] = (b > 0) ? (b < 255) ? b : 255 : 0;
			in[(width * height * 3) + i] = (a > 0) ? (a < 255) ? a : 255 : 0;
		}
	}
	else if (channels == 3)
	{
		for (size_t i = 0; i < (width * height); i++)
		{
			int16_t r, g, b;

			sToRgb(in[(width * height * 0) + i], in[(width * height * 1) + i], in[(width * height * 2) + i], &r, &g,
			       &b);

			in[(width * height * 0) + i] = (r > 0) ? (r < 255) ? r : 255 : 0;
			in[(width * height * 1) + i] = (g > 0) ? (g < 255) ? g : 255 : 0;
			in[(width * height * 2) + i] = (b > 0) ? (b < 255) ? b : 255 : 0;
		}
	}
	else
	{
		for (size_t i = 0; i < (width * height * channels); i++)
		{
			const int16_t c = in[i];
			in[i] = (c > 0) ? (c < 255) ? c : 255 : 0;
		}
	}

	// Interlace from planes
	{
		int16_t* planar_in = in;
		out_pitch = out_pitch * channels;

		switch (channels) // TODO?, early optimization?
		{
		case 4:
			for (size_t row = 0; row < (height * out_pitch); row += out_pitch)
				for (size_t col = 0; col < (width * channels); col += channels)
				{
					out[col + row + 3] = (uint8_t)planar_in[width * height * 3];
					out[col + row + 2] = (uint8_t)planar_in[width * height * 2];
					out[col + row + 1] = (uint8_t)planar_in[width * height * 1];
					out[col + row + 0] = (uint8_t)planar_in[width * height * 0];
					planar_in = planar_in + 1;
				}
			break;
		case 3:
			for (size_t row = 0; row < (height * out_pitch); row += out_pitch)
				for (size_t col = 0; col < (width * channels); col += channels)
				{
					out[col + row + 2] = (uint8_t)planar_in[width * height * 2];
					out[col + row + 1] = (uint8_t)planar_in[width * height * 1];
					out[col + row + 0] = (uint8_t)planar_in[width * height * 0];
					planar_in = planar_in + 1;
				}
			break;
		case 2:
			for (size_t row = 0; row < (height * out_pitch); row += out_pitch)
				for (size_t col = 0; col < (width * channels); col += channels)
				{
					out[col + row + 1] = (uint8_t)planar_in[width * height * 1];
					out[col + row + 0] = (uint8_t)planar_in[width * height * 0];
					planar_in = planar_in + 1;
				}
			break;
		case 1:
			for (size_t row = 0; row < (height * out_pitch); row += out_pitch)
				for (size_t col = 0; col < (width * channels); col += channels)
				{
					out[col + row + 0] = (uint8_t)planar_in[width * height * 0];
					planar_in = planar_in + 1;
				}
			break;
		}
	}
}
