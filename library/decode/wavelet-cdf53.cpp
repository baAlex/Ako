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

template <typename T> static T sEven(T lp, T hp_l1, T hp)
{
	return WrapSubtract<T>(lp, WrapAdd(hp_l1, hp) / 4);
}

template <typename T> static T sOdd(T hp, T even, T even_p1)
{
	return WrapAdd<T>(hp, WrapAdd(even, even_p1) / 2);
}


template <typename T>
static void sCdf53HorizontalInverse(unsigned height, unsigned lp_w, unsigned hp_w, unsigned out_stride,
                                    const T* lowpass, const T* highpass, T* output)
{
	for (unsigned row = 0; row < height; row += 1)
	{
		T hp_l1 = 0;

		// Beginning/middle
		for (unsigned col = 0; col < hp_w - 2; col += 1)
		{
			const T lp = lowpass[col + 0];
			const T lp_p1 = lowpass[col + 1];
			const T hp = highpass[col + 0];
			const T hp_p1 = highpass[col + 1];

			const T even = sEven(lp, hp_l1, hp);
			const T even_p1 = sEven(lp_p1, hp, hp_p1);
			const T odd = sOdd(hp, even, even_p1);

			output[(col << 1) + 0] = even;
			output[(col << 1) + 1] = odd;
			hp_l1 = hp;
		}

		// End
		if (lp_w == hp_w)
		{
			for (unsigned col = hp_w - 2; col < hp_w; col += 1)
			{
				const T lp = lowpass[col];
				const T hp = highpass[col];

				const T even = sEven(lp, hp_l1, hp);
				T even_p1 = even;

				if (col != hp_w - 1)
				{
					const T lp_p1 = lowpass[col + 1];
					const T hp_p1 = highpass[col + 1];
					even_p1 = sEven(lp_p1, hp, hp_p1);
				}

				const T odd = sOdd(hp, even, even_p1);

				output[(col << 1) + 0] = even;
				output[(col << 1) + 1] = odd;
				hp_l1 = hp;
			}
		}
		else
		{
			for (unsigned col = hp_w - 2; col < hp_w; col += 1)
			{
				const T lp = lowpass[col + 0];
				const T hp = highpass[col + 0];
				const T lp_p1 = lowpass[col + 1];
				const T hp_p1 = (col < hp_w - 1) ? highpass[col + 1] : 0;

				const T even = sEven(lp, hp_l1, hp);
				const T even_p1 = sEven(lp_p1, hp, hp_p1);
				const T odd = sOdd(hp, even, even_p1);

				output[(col << 1) + 0] = even;
				output[(col << 1) + 1] = odd;
				hp_l1 = hp;
			}

			{
				const unsigned col = hp_w;
				const T lp = lowpass[col];
				const T hp = 0;
				const T even = sEven(lp, hp_l1, hp);
				output[(col << 1) + 0] = even;
			}
		}

		// Next row
		highpass += hp_w;
		lowpass += lp_w;
		output += out_stride;
	}
}


template <typename T>
static void sCdf53InPlaceishVerticalInverse(unsigned width, unsigned lp_h, unsigned hp_h, const T* lowpass, T* highpass,
                                            T* out_lowpass)
{
	// Evens
	{
		const T* lp = lowpass;
		const T* hp = highpass;
		T* out_lp = out_lowpass;

		// Beginning
		{
			const T hp_l1 = 0;
			for (unsigned col = 0; col < width; col += 1)
			{
				const T even = sEven(lp[col], hp_l1, hp[col]);
				out_lp[col] = even;
			}

			lp += width;
			hp += width;
			out_lp += width;
		}

		// Middle
		for (unsigned row = 1; row < hp_h; row += 1)
		{
			const T* hp_l1 = hp - width;
			for (unsigned col = 0; col < width; col += 1)
			{
				const T even = sEven(lp[col], hp_l1[col], hp[col]);
				out_lp[col] = even;
			}

			lp += width;
			hp += width;
			out_lp += width;
		}

		// End
		if (lp_h != hp_h)
		{
			const T* hp_l1 = hp - width;
			for (unsigned col = 0; col < width; col += 1)
			{
				const T even = sEven(lp[col], hp_l1[col], static_cast<T>(0));
				out_lp[col] = even;
			}
		}
	}

	// Odds
	{
		const T* even = out_lowpass;
		T* hp = highpass;

		// Beginning/middle
		for (unsigned row = 0; row < hp_h - 1; row += 1)
		{
			const T* even_p1 = even + width;
			for (unsigned col = 0; col < width; col += 1)
			{
				const T odd = sOdd(hp[col], even[col], even_p1[col]);
				hp[col] = odd;
			}

			even += width;
			hp += width;
		}

		// End
		{
			const T* even_p1 = (lp_h == hp_h) ? even : even + width;
			for (unsigned col = 0; col < width; col += 1)
			{
				const T odd = sOdd(hp[col], even[col], even_p1[col]);
				hp[col] = odd;
			}
		}
	}
}


template <>
void Cdf53HorizontalInverse(unsigned height, unsigned lp_w, unsigned hp_w, unsigned out_stride, const int16_t* lowpass,
                            const int16_t* highpass, int16_t* output)
{
	sCdf53HorizontalInverse(height, lp_w, hp_w, out_stride, lowpass, highpass, output);
}

template <>
void Cdf53InPlaceishVerticalInverse(unsigned width, unsigned lp_h, unsigned hp_h, const int16_t* lowpass,
                                    int16_t* highpass, int16_t* out_lowpass)
{
	sCdf53InPlaceishVerticalInverse(width, lp_h, hp_h, lowpass, highpass, out_lowpass);
}

template <>
void Cdf53HorizontalInverse(unsigned height, unsigned lp_w, unsigned hp_w, unsigned out_stride, const int32_t* lowpass,
                            const int32_t* highpass, int32_t* output)
{
	sCdf53HorizontalInverse(height, lp_w, hp_w, out_stride, lowpass, highpass, output);
}

template <>
void Cdf53InPlaceishVerticalInverse(unsigned width, unsigned lp_h, unsigned hp_h, const int32_t* lowpass,
                                    int32_t* highpass, int32_t* out_lowpass)
{
	sCdf53InPlaceishVerticalInverse(width, lp_h, hp_h, lowpass, highpass, out_lowpass);
}

} // namespace ako
