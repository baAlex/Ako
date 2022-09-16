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

#include "ako-private.hpp"

namespace ako
{

template <typename T>
static void HaarHorizontalInverse(unsigned height, unsigned lp_w, unsigned hp_w, unsigned out_stride, const T* lowpass,
                                  const T* highpass, T* output)
{
	for (unsigned row = 0; row < height; row += 1)
	{
		// Evens/Odds
		for (size_t col = 0; col < hp_w; col += 1)
		{
			const T lp = lowpass[col];
			const T hp = highpass[col];
			output[(col << 1) + 0] = lp;
			output[(col << 1) + 1] = lp - hp;
		}

		// Next row
		highpass += hp_w;
		lowpass += lp_w;
		output += out_stride;
	}
}


template <typename T>
static void HaarInPlaceishVerticalInverse(unsigned width, unsigned lp_h, unsigned hp_h, const T* lowpass, T* highpass,
                                          T* out_lowpass)
{
	(void)lp_h;

	// Evens (consumes 'lowpass', outputs to 'out_lowpass')
	for (unsigned row = 0; row < hp_h; row += 1)
	{
		const T* lp = lowpass + width * row;
		T* out_even = out_lowpass + width * row;

		for (unsigned col = 0; col < width; col += 1)
			out_even[col] = lp[col];
	}

	// Odds (consumes and output to 'highpass')
	for (unsigned row = 0; row < hp_h; row += 1)
	{
		T* hp = highpass + width * row;
		const T* even = out_lowpass + width * row;

		for (unsigned col = 0; col < width; col += 1)
			hp[col] = even[col] - hp[col];
	}
}


template <typename T>
static void sUnlift(const Wavelet& w, unsigned width, unsigned height, unsigned channels, T* input, T* output)
{
	(void)w;

	T* in = input;

	unsigned lp_w = 0;
	unsigned lp_h = 0;
	unsigned hp_w = 0;
	unsigned hp_h = 0;

	const auto lifts_no = LiftsNo(width, height);
	LiftMeasures((lifts_no - 1), width, height, lp_w, lp_h, hp_w, hp_h);

	// Lowpasses
	for (unsigned ch = 0; ch < channels; ch += 1)
	{
		T* lp = output + (width * height) * ch;
		Memcpy2d(lp_w, lp_h, lp_w, lp_w, in, lp); // Quadrant A
		// printf("! \tLpCh%u = %i\n", ch, *lp);

		in += (lp_w * lp_h) * 1; // Move it one lowpass quadrant forward
	}

	// Highpasses
	for (unsigned lift = (lifts_no - 1); lift < lifts_no; lift -= 1) // Underflows
	{
		LiftMeasures(lift, width, height, lp_w, lp_h, hp_w, hp_h);
		// printf("! \tUnlift, %ux%u <- lp: %ux%u, hp: %ux%u\n", lp_w + hp_w, lp_h + hp_h, lp_w, lp_h, hp_w, hp_h);

		// Iterate in Yuv order
		for (unsigned ch = 0; ch < channels; ch += 1)
		{
			T* lp = output + (width * height) * ch;

			// Set auxiliary memory
			T* aux = input; // in - (lp_w * lp_h);

			// Input highpasses
			auto hp_quad_c = in + (hp_w * hp_h) * 0; // Memcpy2d() in the encoder side already
			auto hp_quad_b = in + (hp_w * hp_h) * 1; // did all work involving strides
			auto hp_quad_d = in + (hp_w * hp_h) * 2;

			in += (hp_w * hp_h) * 3; // Move it three highpasses quadrants forward
			                         // printf("! \tHpsCh%u = %i, %i, %i\n", ch, *hp_quad_c, *hp_quad_b, *hp_quad_d);

			// Wavelet transformation
			HaarInPlaceishVerticalInverse(lp_w, lp_h, hp_h, lp, hp_quad_c, aux);
			HaarInPlaceishVerticalInverse(lp_w, lp_h, hp_h, hp_quad_b, hp_quad_d, hp_quad_b);
			HaarHorizontalInverse(lp_h, lp_w, hp_w, (lp_w + hp_w) << 1, aux, hp_quad_b, lp);
			HaarHorizontalInverse(lp_h, lp_w, hp_w, (lp_w + hp_w) << 1, hp_quad_c, hp_quad_d, lp + (lp_w + hp_w));
		}
	}
}


template <>
void Unlift(const Wavelet& w, unsigned width, unsigned height, unsigned channels, int16_t* input, int16_t* output)
{
	sUnlift(w, width, height, channels, input, output);
}

template <>
void Unlift(const Wavelet& w, unsigned width, unsigned height, unsigned channels, int32_t* input, int32_t* output)
{
	sUnlift(w, width, height, channels, input, output);
}

} // namespace ako
