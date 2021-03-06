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


static inline void sDeinterleave(int discard_non_visible, size_t channels, size_t width, size_t in_stride,
                                 size_t out_plane, const uint8_t* in, const uint8_t* in_end, int16_t* out)
{
	if (discard_non_visible != 0)
	{
		for (; in < in_end; in += in_stride, out += width)
			for (size_t col = 0; col < width; col++)
			{
				out[out_plane * (channels - 1) + col] = in[col * channels + (channels - 1)];

				if (in[col * channels + (channels - 1)] != 0)
				{
					for (size_t ch = 0; ch < (channels - 1); ch++)
						out[out_plane * ch + col] = in[col * channels + ch];
				}
				else
				{
					for (size_t ch = 0; ch < (channels - 1); ch++)
						out[out_plane * ch + col] = 0;
				}
			}
	}
	else
	{
		for (; in < in_end; in += in_stride, out += width)
			for (size_t col = 0; col < width; col++)
			{
				for (size_t ch = 0; ch < channels; ch++)
					out[out_plane * ch + col] = in[col * channels + ch];
			}
	}
}


void akoFormatToPlanarI16Yuv(int discard_non_visible, enum akoColor color, size_t channels, size_t width, size_t height,
                             size_t input_stride, size_t out_planes_spacing, const uint8_t* in, int16_t* out)
{
	// Deinterleave, convert from u8 to i16, and remove (or not) non visible pixels
	{
		const size_t in_stride = input_stride * channels;
		const size_t out_plane = (width * height) + out_planes_spacing;

		const uint8_t* in_end = in + in_stride * height;

		if (channels == 3)
			sDeinterleave(0, 3, width, in_stride, out_plane, in, in_end, out);
		else if (channels == 4)
			sDeinterleave(discard_non_visible, 4, width, in_stride, out_plane, in, in_end, out);
		else if (channels == 2)
			sDeinterleave(discard_non_visible, 2, width, in_stride, out_plane, in, in_end, out);
		else if (channels == 1)
			sDeinterleave(0, 1, width, in_stride, out_plane, in, in_end, out);
		else
			sDeinterleave(0, channels, width, in_stride, out_plane, in, in_end, out);
	}

	// Color transformation (to Yuv)
	if (channels >= 3)
	{
		const size_t out_plane = (width * height) + out_planes_spacing;

		if (color == AKO_COLOR_YCOCG)
		{
			// https://en.wikipedia.org/wiki/YCoCg#Conversion_with_the_RGB_color_model
			for (size_t c = 0; c < (width * height); c++) // SIMD friendly :)
			{
				const int16_t r = out[out_plane * 0 + c];
				const int16_t g = out[out_plane * 1 + c];
				const int16_t b = out[out_plane * 2 + c];

				out[out_plane * 1 + c] = (int16_t)(r - b);
				const int16_t temp = (int16_t)(b + ((r - b) / 2));
				out[out_plane * 2 + c] = (int16_t)(g - temp);
				out[out_plane * 0 + c] = (int16_t)(temp + ((g - temp) / 2));
			}
		}
		else if (color == AKO_COLOR_YCOCG_Q)
		{
			for (size_t c = 0; c < (width * height); c++)
			{
				const int16_t r = out[out_plane * 0 + c];
				const int16_t g = out[out_plane * 1 + c];
				const int16_t b = out[out_plane * 2 + c];

				out[out_plane * 1 + c] = (int16_t)(r - b);
				const int16_t temp = (int16_t)(b + ((r - b) / 2));
				out[out_plane * 2 + c] = (int16_t)(g - temp);
				out[out_plane * 0 + c] = (int16_t)((temp + ((g - temp) / 2)) * 2);
			}
		}
		else if (color == AKO_COLOR_SUBTRACT_G)
		{
			// https://developers.google.com/speed/webp/docs/compression#subtract_green_transform
			for (size_t c = 0; c < (width * height); c++)
			{
				const int16_t r = out[out_plane * 0 + c];
				const int16_t g = out[out_plane * 1 + c];
				const int16_t b = out[out_plane * 2 + c];

				out[out_plane * 0 + c] = (int16_t)(g);
				out[out_plane * 1 + c] = (int16_t)(r - g);
				out[out_plane * 2 + c] = (int16_t)(b - g);
			}
		}
	}
}


static inline void sYCoCgToRgb(size_t channels, size_t width, size_t height, size_t in_plane, int16_t* in)
{
	for (size_t c = 0; c < (width * height); c++)
	{
		const int16_t y = in[in_plane * 0 + c];
		const int16_t u = in[in_plane * 1 + c];
		const int16_t v = in[in_plane * 2 + c];

		const int16_t temp = (int16_t)(y - (v / 2));
		const int16_t g = (int16_t)(v + temp);
		const int16_t b = (int16_t)(temp - (u / 2));
		const int16_t r = (int16_t)(b + u);

		// Saturate
		in[in_plane * 0 + c] = (int16_t)((r > 0) ? (r < 255) ? r : 255 : 0);
		in[in_plane * 1 + c] = (int16_t)((g > 0) ? (g < 255) ? g : 255 : 0);
		in[in_plane * 2 + c] = (int16_t)((b > 0) ? (b < 255) ? b : 255 : 0);
	}

	for (size_t ch = 3; ch < channels; ch++)
		for (size_t c = 0; c < (width * height); c++)
		{
			const int16_t v = *(in + in_plane * ch + c);
			in[in_plane * ch + c] = (int16_t)((v > 0) ? (v < 255) ? v : 255 : 0);
		}
}


static inline void sYCoCgQToRgb(size_t channels, size_t width, size_t height, size_t in_plane, int16_t* in)
{
	for (size_t c = 0; c < (width * height); c++)
	{
		const int16_t y = (int16_t)(in[in_plane * 0 + c] / 2);
		const int16_t u = in[in_plane * 1 + c];
		const int16_t v = in[in_plane * 2 + c];

		const int16_t temp = (int16_t)(y - (v / 2));
		const int16_t g = (int16_t)(v + temp);
		const int16_t b = (int16_t)(temp - (u / 2));
		const int16_t r = (int16_t)(b + u);

		// Saturate
		in[in_plane * 0 + c] = (int16_t)((r > 0) ? (r < 255) ? r : 255 : 0);
		in[in_plane * 1 + c] = (int16_t)((g > 0) ? (g < 255) ? g : 255 : 0);
		in[in_plane * 2 + c] = (int16_t)((b > 0) ? (b < 255) ? b : 255 : 0);
	}

	for (size_t ch = 3; ch < channels; ch++)
		for (size_t c = 0; c < (width * height); c++)
		{
			const int16_t v = *(in + in_plane * ch + c);
			in[in_plane * ch + c] = (int16_t)((v > 0) ? (v < 255) ? v : 255 : 0);
		}
}


static inline void sSubtractGToRgb(size_t channels, size_t width, size_t height, size_t in_plane, int16_t* in)
{
	for (size_t c = 0; c < (width * height); c++)
	{
		const int16_t y = in[in_plane * 0 + c];
		const int16_t u = in[in_plane * 1 + c];
		const int16_t v = in[in_plane * 2 + c];

		const int16_t r = (int16_t)(u + y);
		const int16_t g = (int16_t)(y);
		const int16_t b = (int16_t)(v + y);

		// Saturate
		in[in_plane * 0 + c] = (int16_t)((r > 0) ? (r < 255) ? r : 255 : 0);
		in[in_plane * 1 + c] = (int16_t)((g > 0) ? (g < 255) ? g : 255 : 0);
		in[in_plane * 2 + c] = (int16_t)((b > 0) ? (b < 255) ? b : 255 : 0);
	}

	for (size_t ch = 3; ch < channels; ch++)
		for (size_t c = 0; c < (width * height); c++)
		{
			const int16_t v = *(in + in_plane * ch + c);
			in[in_plane * ch + c] = (int16_t)((v > 0) ? (v < 255) ? v : 255 : 0);
		}
}


static inline void sSaturateRgb(size_t channels, size_t width, size_t height, size_t in_plane, int16_t* in)
{
	for (size_t ch = 0; ch < channels; ch++)
		for (size_t c = 0; c < (width * height); c++)
		{
			const int16_t v = *(in + in_plane * ch + c);
			in[in_plane * ch + c] = (int16_t)((v > 0) ? (v < 255) ? v : 255 : 0);
		}
}


static inline void sInterleave(size_t channels, size_t width, size_t in_plane, size_t out_stride, const int16_t* in,
                               uint8_t* out, const uint8_t* out_end)
{
	for (; out < out_end; out += out_stride, in += width)
		for (size_t col = 0; col < width; col++)
		{
			for (size_t ch = 0; ch < channels; ch++)
				out[col * channels + ch] = (uint8_t)(in[in_plane * ch + col]);
		}
}


void akoFormatToInterleavedU8Rgb(enum akoColor color, size_t channels, size_t width, size_t height,
                                 size_t in_planes_spacing, size_t output_stride, int16_t* in, uint8_t* out)
{
	// Color transformation (to Rgb) (destroys 'in')
	{
		const size_t in_plane = (width * height) + in_planes_spacing;

		if (color == AKO_COLOR_YCOCG && channels >= 3)
		{
			if (channels == 3)
				sYCoCgToRgb(3, width, height, in_plane, in);
			else if (channels == 4)
				sYCoCgToRgb(4, width, height, in_plane, in);
			else
				sYCoCgToRgb(channels, width, height, in_plane, in);
		}
		else if (color == AKO_COLOR_YCOCG_Q && channels >= 3)
		{
			if (channels == 3)
				sYCoCgQToRgb(3, width, height, in_plane, in);
			else if (channels == 4)
				sYCoCgQToRgb(4, width, height, in_plane, in);
			else
				sYCoCgQToRgb(channels, width, height, in_plane, in);
		}
		else if (color == AKO_COLOR_SUBTRACT_G && channels >= 3)
		{
			if (channels == 3)
				sSubtractGToRgb(3, width, height, in_plane, in);
			else if (channels == 4)
				sSubtractGToRgb(4, width, height, in_plane, in);
			else
				sSubtractGToRgb(channels, width, height, in_plane, in);
		}
		else
		{
			if (channels == 3)
				sSaturateRgb(3, width, height, in_plane, in);
			else if (channels == 4)
				sSaturateRgb(4, width, height, in_plane, in);
			else if (channels == 2)
				sSaturateRgb(2, width, height, in_plane, in);
			else if (channels == 1)
				sSaturateRgb(1, width, height, in_plane, in);
			else
				sSaturateRgb(channels, width, height, in_plane, in);
		}
	}

	// Interleave and convert from i16 to u8
	{
		const size_t in_plane = (width * height) + in_planes_spacing;

		const size_t out_stride = output_stride * channels;
		const uint8_t* out_end = out + out_stride * height;

		if (channels == 3)
			sInterleave(3, width, in_plane, out_stride, in, out, out_end);
		else if (channels == 4)
			sInterleave(4, width, in_plane, out_stride, in, out, out_end);
		else if (channels == 2)
			sInterleave(2, width, in_plane, out_stride, in, out, out_end);
		else if (channels == 1)
			sInterleave(1, width, in_plane, out_stride, in, out, out_end);
		else
			sInterleave(channels, width, in_plane, out_stride, in, out, out_end);
	}
}
