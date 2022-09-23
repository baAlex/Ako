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
static void sUnlift(const Wavelet& wavelet_transformation, unsigned width, unsigned height, unsigned channels, T* input,
                    T* output)
{
	(void)wavelet_transformation;

	auto in = input;

	unsigned lp_w = width;
	unsigned lp_h = height;
	unsigned hp_w = 0;
	unsigned hp_h = 0;

	const auto lifts_no = LiftsNo(width, height); // May return zero, in such case 'highpasses' below is ignored
	if (lifts_no > 1)
		LiftMeasures((lifts_no - 1), width, height, lp_w, lp_h, hp_w, hp_h);

	// Lowpasses
	for (unsigned ch = 0; ch < channels; ch += 1)
	{
		auto lp = output + (width * height) * ch;
		Memcpy2d(lp_w, lp_h, lp_w, lp_w, in, lp);

		// printf("! \tLpCh%u (%ux%u) = %i\n", ch, lp_w, lp_h, *lp);

		in += (lp_w * lp_h); // Quadrant A
	}

	// Highpasses
	for (unsigned lift = (lifts_no - 1); lift < lifts_no; lift -= 1) // Underflows
	{
		LiftMeasures(lift, width, height, lp_w, lp_h, hp_w, hp_h);
		// printf("! \tUnlift, %ux%u <- lp: %ux%u, hp: %ux%u\n", lp_w + hp_w, lp_h + hp_h, lp_w, lp_h, hp_w, hp_h);

		// Iterate in Yuv order
		for (unsigned ch = 0; ch < channels; ch += 1)
		{
			auto lp = output + (width * height) * ch;

			// Set auxiliary memory
			auto aux = in - (lp_w * lp_h);

			// Input highpasses
			const auto hp_quad_c = in;
			in += (lp_w * hp_h); // Quadrant C

			auto hp_quad_b = in;
			in += (hp_w * lp_h); // Quadrant B

			const auto hp_quad_d = in;
			in += (hp_w * hp_h); // Quadrant D

			// printf("! \tHpsCh%u %ux%u = %i, %i, %i\n", ch, lp_w + hp_w, lp_h + hp_h, *hp_quad_c, *hp_quad_b,
			//       *hp_quad_d);

			// Wavelet transformation
			HaarInPlaceishVerticalInverse(lp_w, lp_h, hp_h, lp, hp_quad_c, aux);
			HaarInPlaceishVerticalInverse(hp_w, lp_h, hp_h, hp_quad_b, hp_quad_d, hp_quad_b);

			HaarHorizontalInverse(lp_h, lp_w, hp_w, (lp_w + hp_w) << 1, aux, hp_quad_b, lp);
			HaarHorizontalInverse(hp_h, lp_w, hp_w, (lp_w + hp_w) << 1, hp_quad_c, hp_quad_d, lp + (lp_w + hp_w));
		}
	}
}


template <>
void Unlift(const Wavelet& wavelet_transformation, unsigned width, unsigned height, unsigned channels, int16_t* input,
            int16_t* output)
{
	sUnlift(wavelet_transformation, width, height, channels, input, output);
}

template <>
void Unlift(const Wavelet& wavelet_transformation, unsigned width, unsigned height, unsigned channels, int32_t* input,
            int32_t* output)
{
	sUnlift(wavelet_transformation, width, height, channels, input, output);
}

} // namespace ako
