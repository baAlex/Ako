/*

MIT License

Copyright (c) 2021-2023 Alexander Brandt

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

#include "ako-private.hpp"

namespace ako
{

template <typename T>
static void sHaarHorizontalInverse(unsigned height, unsigned lp_w, unsigned hp_w, unsigned out_stride, const T* lowpass,
                                   const T* highpass, T* output)
{
	for (unsigned row = 0; row < height; row += 1)
	{
		// Beginning/middle
		for (unsigned col = 0; col < hp_w; col += 1)
		{
			const T lp = lowpass[col];
			const T hp = highpass[col];

			const T even = lp;
			const T odd = WrapSubtract(lp, hp);

			output[(col << 1) + 0] = even;
			output[(col << 1) + 1] = odd;
		}

		// End
		if (lp_w != hp_w)
		{
			const unsigned col = hp_w;
			const T even = lowpass[col];
			output[(col << 1) + 0] = even;
		}

		// Next row
		highpass += hp_w;
		lowpass += lp_w;
		output += out_stride;
	}
}


template <typename T>
static void sHaarInPlaceishVerticalInverse(unsigned width, unsigned lp_h, unsigned hp_h, const T* lowpass, T* highpass,
                                           T* out_lowpass)
{
	// Beginning/middle
	for (unsigned row = 0; row < hp_h; row += 1)
	{
		const T* lp = lowpass;
		T* hp = highpass;
		T* out_lp = out_lowpass;

		for (unsigned col = 0; col < width; col += 1)
		{
			const T even = lp[col];
			const T odd = WrapSubtract(lp[col], hp[col]);

			out_lp[col] = even;
			hp[col] = odd;
		}

		lowpass += width;
		highpass += width;
		out_lowpass += width;
	}

	// End
	if (lp_h != hp_h)
	{
		const T* lp = lowpass;
		T* out_lp = out_lowpass;

		for (unsigned col = 0; col < width; col += 1)
		{
			const T even = lp[col];
			out_lp[col] = even;
		}
	}
}


template <>
void HaarHorizontalInverse(unsigned height, unsigned lp_w, unsigned hp_w, unsigned out_stride, const int16_t* lowpass,
                           const int16_t* highpass, int16_t* output)
{
	sHaarHorizontalInverse(height, lp_w, hp_w, out_stride, lowpass, highpass, output);
}

template <>
void HaarInPlaceishVerticalInverse(unsigned width, unsigned lp_h, unsigned hp_h, const int16_t* lowpass,
                                   int16_t* highpass, int16_t* out_lowpass)
{
	sHaarInPlaceishVerticalInverse(width, lp_h, hp_h, lowpass, highpass, out_lowpass);
}

template <>
void HaarHorizontalInverse(unsigned height, unsigned lp_w, unsigned hp_w, unsigned out_stride, const int32_t* lowpass,
                           const int32_t* highpass, int32_t* output)
{
	sHaarHorizontalInverse(height, lp_w, hp_w, out_stride, lowpass, highpass, output);
}

template <>
void HaarInPlaceishVerticalInverse(unsigned width, unsigned lp_h, unsigned hp_h, const int32_t* lowpass,
                                   int32_t* highpass, int32_t* out_lowpass)
{
	sHaarInPlaceishVerticalInverse(width, lp_h, hp_h, lowpass, highpass, out_lowpass);
}

} // namespace ako
