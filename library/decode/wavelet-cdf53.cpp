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
static void sCdf53HorizontalInverse(unsigned height, unsigned lp_w, unsigned hp_w, unsigned out_stride,
                                    const T* lowpass, const T* highpass, T* output)
{
	for (unsigned row = 0; row < height; row += 1)
	{
		// Evens (length of 'lp_w')
		for (unsigned col = 0; col < hp_w; col += 1)
		{
			const T lp = lowpass[col];
			const T hp = highpass[col];
			const T hp_l1 = (col > 0) ? highpass[col - 1] : hp; // Clamp

			output[(col << 1) + 0] = WrapSubtract<T>(lp, WrapAdd(hp_l1, hp) / 4);
		}

		if (lp_w != hp_w) // Complete lowpass
		{
			const unsigned col = hp_w;

			const T lp = lowpass[col];
			const T hp = highpass[col - 1];
			const T hp_l1 = highpass[col - 2]; // Clamp

			output[(col << 1) + 0] = WrapSubtract<T>(lp, WrapAdd(hp_l1, hp) / 4);
		}

		// Odds (length of 'hp_w')
		for (unsigned col = 0; col < hp_w; col += 1)
		{
			const T hp = highpass[col];
			const T even = output[(col << 1) + 0];
			const T even_p1 = (col < hp_w - 1) ? output[(col << 1) + 2] : even; // Clamp

			output[(col << 1) + 1] = WrapAdd<T>(hp, WrapAdd(even, even_p1) / 2);
		}

		// Next row
		highpass += hp_w;
		lowpass += lp_w;
		output += out_stride;
	}
}


#ifdef AKO_SSE2

static inline __m128i DivideUsingShift(__m128i v, int shift)
{
	const auto mask = _mm_set1_epi16(-1); // All ones
	const auto sign = _mm_srli_epi16(v, 15);

	v = _mm_andnot_si128(v, mask);
	v = _mm_add_epi16(v, sign); // Add one if was negative, zero if wasn't

	v = _mm_srai_epi16(v, shift); // Now that values are positive, divide them

	v = _mm_andnot_si128(v, mask);
	v = _mm_add_epi16(v, sign);

	return v;
}

template <typename T>
static void sCdf53HorizontalInverseSimd(unsigned height, unsigned lp_w, unsigned hp_w, unsigned out_stride,
                                        const T* lowpass, const T* highpass, T* output)
{
	for (unsigned row = 0; row < height; row += 1)
	{
		// Start
		unsigned col = 0;
		{
			const T lp = lowpass[col + 0];
			const T hp = highpass[col + 0];
			const T hp_l1 = hp; // Clamp
			const T lp_p1 = lowpass[col + 1];
			const T hp_p1 = highpass[col + 1];

			const T even = WrapSubtract<T>(lp, WrapAdd(hp_l1, hp) / 4);
			const T even_p1 = WrapSubtract<T>(lp_p1, WrapAdd(hp, hp_p1) / 4);
			const T odd = WrapAdd<T>(hp, WrapAdd(even, even_p1) / 2);

			output[(col << 1) + 0] = even;
			output[(col << 1) + 1] = odd;
		}

		// Middle
		if (hp_w >= 8)
		{
			for (col = 1; col < (hp_w - 8); col += 7)
			{
				const auto lp = _mm_loadu_si128(reinterpret_cast<const __m128i_u*>(lowpass + col + 0));
				const auto hp = _mm_loadu_si128(reinterpret_cast<const __m128i_u*>(highpass + col + 0));
				const auto hp_l1 = _mm_loadu_si128(reinterpret_cast<const __m128i_u*>(highpass + col - 1));

				const auto evens = _mm_sub_epi16(lp, DivideUsingShift(_mm_add_epi16(hp_l1, hp), 2));
				const auto evens_p1 = _mm_srli_si128(evens, 2); // A bit un-intuitive, but register order is little
				                                                // endian (that or my head is working in big endian)

				const auto odds = _mm_add_epi16(hp, DivideUsingShift(_mm_add_epi16(evens, evens_p1), 1));

				_mm_storeu_si128(reinterpret_cast<__m128i_u*>(output + (col << 1) + 0),
				                 _mm_unpacklo_epi16(evens, odds));
				_mm_storeu_si128(reinterpret_cast<__m128i_u*>(output + (col << 1) + 8),
				                 _mm_unpackhi_epi16(evens, odds));
			}
		}
		else
		{
			col = 1;
		}

		// End
		for (; col < hp_w; col += 1)
		{
			const T lp = lowpass[col + 0];
			const T hp = highpass[col + 0];
			const T hp_l1 = highpass[col - 1];

			const T even = WrapSubtract<T>(lp, WrapAdd(hp_l1, hp) / 4);
			T even_p1 = even; // Clamp

			if (col < hp_w - 1)
			{
				const T lp_p1 = lowpass[col + 1];
				const T hp_p1 = highpass[col + 1];
				even_p1 = WrapSubtract<T>(lp_p1, WrapAdd(hp, hp_p1) / 4);
			}

			const T odd = WrapAdd<T>(hp, WrapAdd(even, even_p1) / 2);

			output[(col << 1) + 0] = even;
			output[(col << 1) + 1] = odd;
		}

		// Complete lowpass (width wasn't divisible by two)
		if (lp_w != hp_w)
		{
			col = hp_w;

			const T lp = lowpass[col];
			const T hp = highpass[col - 1];
			const T hp_l1 = highpass[col - 2]; // Clamp

			output[(col << 1) + 0] = WrapSubtract<T>(lp, WrapAdd(hp_l1, hp) / 4);
		}

		// Next row
		highpass += hp_w;
		lowpass += lp_w;
		output += out_stride;
	}
}
#endif


template <typename T>
static void sCdf53InPlaceishVerticalInverse(unsigned width, unsigned lp_h, unsigned hp_h, const T* lowpass, T* highpass,
                                            T* out_lowpass)
{
	// Evens (length of 'lp_h')
	{
		const T* lp = lowpass;
		const T* hp = highpass;

		T* out = out_lowpass;

		for (unsigned row = 0; row < hp_h; row += 1)
		{
			const T* hp_l1 = (row > 0) ? (hp - width) : hp; // Clamp

			for (unsigned col = 0; col < width; col += 1)
				out[col] = WrapSubtract<T>(lp[col], WrapAdd(hp_l1[col], hp[col]) / 4);

			lp += width;
			hp += width;
			out += width;
		}

		if (lp_h != hp_h) // Complete lowpass
		{
			hp -= width;                 // Looks hacky
			const T* hp_l1 = hp - width; // Clamp

			for (unsigned col = 0; col < width; col += 1)
				out[col] = WrapSubtract<T>(lp[col], WrapAdd(hp_l1[col], hp[col]) / 4);
		}
	}

	// Odds (length of 'hp_h')
	{
		const T* even = out_lowpass;
		T* hp = highpass;

		for (unsigned row = 0; row < hp_h; row += 1)
		{
			const T* even_p1 = (row < hp_h - 1) ? (even + width) : even; // Clamp

			for (unsigned col = 0; col < width; col += 1)
				hp[col] = WrapAdd<T>(hp[col], WrapAdd(even[col], even_p1[col]) / 2);

			even += width;
			hp += width;
		}
	}
}


template <>
void Cdf53HorizontalInverse(unsigned height, unsigned lp_w, unsigned hp_w, unsigned out_stride, const int16_t* lowpass,
                            const int16_t* highpass, int16_t* output)
{
#ifdef AKO_SSE2
	sCdf53HorizontalInverseSimd(height, lp_w, hp_w, out_stride, lowpass, highpass, output);
#else
	sCdf53HorizontalInverse(height, lp_w, hp_w, out_stride, lowpass, highpass, output);
#endif
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
