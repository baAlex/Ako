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
static void HaarHorizontalForward(unsigned width, unsigned height, unsigned inout_stride, const T* input, T* output)
{
	const unsigned half = Half(width);
	const unsigned rule = HalfPlusOneRule(width);

	for (unsigned row = 0; row < height; row += 1)
	{
		// Highpass (length of 'half')
		for (unsigned col = 0; col < half; col += 1)
		{
			const T even = input[(col << 1) + 0];
			const T odd = input[(col << 1) + 1];

			output[rule + col] = even - odd;
		}

		// Lowpass (length of 'rule')
		for (unsigned col = 0; col < half; col += 1)
		{
			const T even = input[(col << 1) + 0];
			output[col] = even;
		}

		// Next row
		input += inout_stride;
		output += inout_stride; // TODO, the old C code, while complicated, had an individual more compact output
		                        // stride. I need to measure if that helps with cache locality, maybe helps in the
		                        // vertical transform (probably not that much since processors can deal with strides)
	}
}


template <typename T>
static void HaarVerticalForward(unsigned width, unsigned height, unsigned inout_stride, const T* input, T* output)
{
	const unsigned rule = HalfPlusOneRule(height);
	const unsigned half = Half(height);

	// Highpass (length of 'half')
	for (unsigned row = 0; row < half; row += 1)
	{
		const T* in = input + inout_stride * (row << 1);
		T* hp_out = output + inout_stride * (row + rule);

		for (unsigned col = 0; col < width; col += 1)
		{
			const T even = in[col + inout_stride * 0];
			const T odd = in[col + inout_stride * 1];

			hp_out[col] = even - odd;
		}
	}

	// Lowpass (length of 'rule')
	for (unsigned row = 0; row < half; row += 1)
	{
		const T* in = input + inout_stride * (row << 1);
		T* lp_out = output + inout_stride * (row);

		for (unsigned col = 0; col < width; col += 1)
		{
			const T even = in[col + inout_stride * 0];
			lp_out[col] = even;
		}
	}
}


template <typename T>
static void sLift(const Wavelet& w, unsigned width, unsigned height, unsigned channels, T* input, T* output)
{
	(void)w;

	// Everything here operates in reverse

	unsigned lp_w = width;
	unsigned lp_h = height;

	T* out = output + (width * height) * channels; // Set to the very end

	// Highpasses
	for (unsigned lift = 0; lp_w > 1 && lp_h > 1; lift += 1, lp_w = HalfPlusOneRule(lp_w), lp_h = HalfPlusOneRule(lp_h))
	{
		// printf("! \tLift, %ux%u -> lp: %ux%u, hp: %ux%u\n", lp_w, lp_h, HalfPlusOneRule(lp_w), HalfPlusOneRule(lp_h),
		//       Half(lp_w), Half(lp_h));

		// Iterate in Vuy order
		for (unsigned ch = (channels - 1); ch < channels; ch -= 1) // Underflows
		{
			T* inout = input + (width * height) * ch;

			// Lift
			{
				// Set auxiliary memory
				T* aux = inout + (width * lp_h);
				if (lift == 0)
					aux = output; // + (width * height) * ch; // First lift needs extra sauce

				// Wavelet transformation, note the ping-pong between buffers
				HaarHorizontalForward(lp_w, lp_h, width, inout, aux);
				HaarVerticalForward(lp_w, lp_h, width, aux, inout);
			}

			// Output highpasses
			{
				const unsigned next_lp_w = HalfPlusOneRule(lp_w);
				const unsigned next_lp_h = HalfPlusOneRule(lp_h);
				const unsigned hp_w = Half(lp_w);
				const unsigned hp_h = Half(lp_h);

				out -= (hp_w * hp_h) * 3; // Move it three highpasses quadrants back

				Memcpy2d(hp_w, hp_h, width, hp_w,
				         inout + next_lp_h * width, //
				         out + (hp_w * hp_h) * 0);  // Quadrant C

				Memcpy2d(hp_w, hp_h, width, hp_w,
				         inout + next_lp_w,        //
				         out + (hp_w * hp_h) * 1); // Quadrant B

				Memcpy2d(hp_w, hp_h, width, hp_w,
				         inout + next_lp_h * width + next_lp_w, //
				         out + (hp_w * hp_h) * 2);              // Quadrant D

				// printf("! \tHpsCh%u = %i, %i, %i\n", ch, *(out + (hp_w * hp_h) * 0), *(out + (hp_w * hp_h) * 1),
				//       *(out + (hp_w * hp_h) * 2));
			}
		}

		// Developers, developers, developers
		// if (lp_w <= width / 2) // Stop earlier
		//	break;
	}

	// Lowpasses
	for (unsigned ch = (channels - 1); ch < channels; ch -= 1)
	{
		out -= (lp_w * lp_h) * 1; // Move it one lowpass quadrant back

		const T* lp = input + (width * height) * ch;
		Memcpy2d(lp_w, lp_h, width, lp_w, lp, out); // Quadrant A
		                                            // printf("! \tLpCh%u = %i\n", ch, *lp);
	}

	// Developers, developers, developers
	// if (out != output)
	//	printf("Non square image! (%li)\n", out - output);
}


template <>
void Lift(const Wavelet& w, unsigned width, unsigned height, unsigned channels, int16_t* input, int16_t* output)
{
	sLift(w, width, height, channels, input, output);
}

template <>
void Lift(const Wavelet& w, unsigned width, unsigned height, unsigned channels, int32_t* input, int32_t* output)
{
	sLift(w, width, height, channels, input, output);
}

} // namespace ako
