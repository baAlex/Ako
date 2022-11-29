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
static void sLift(const Callbacks& callbacks, const Wavelet& wavelet_transformation, unsigned width, unsigned height,
                  unsigned channels, T* input, T* output)
{
	// Everything here operates in reverse

	auto out = reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(output) +
	                                TileDataSize<T>(width, height, channels)); // Set to the very end

	unsigned lp_w = width;
	unsigned lp_h = height;
	unsigned hp_w = 0;
	unsigned hp_h = 0;

	const auto lifts_no = LiftsNo(width, height); // May return zero

	// Highpasses
	for (unsigned lift = 0; lift < lifts_no; lift += 1)
	{
		const auto prev_stride = (lp_w + hp_w);

		LiftMeasures(lift, width, height, lp_w, lp_h, hp_w, hp_h);
		const auto current_w = lp_w + hp_w;
		const auto current_h = lp_h + hp_h;

		if (callbacks.generic_event != nullptr)
			callbacks.generic_event(GenericEvent::LiftHighpassesDimensions, hp_w, hp_h, 0, {0}, callbacks.user_data);

		// Iterate in Vuy order
		for (unsigned ch = (channels - 1); ch < channels; ch -= 1) // Underflows
		{
			auto inout = input + (width * height) * ch;

			// Set auxiliary memory
			const auto current_stride = current_w;

			auto aux = inout + current_h * prev_stride;
			if (lift == 0)
				aux = output;

			// Wavelet transformation
			if ((wavelet_transformation == Wavelet::Dd137 || wavelet_transformation == Wavelet::Cdf53) &&
			    (current_w > 8 && current_h > 8)) // TODO, 8 isn't Cdf53 minimum
			{
				Cdf53HorizontalForward(current_w, current_h, prev_stride, current_stride, inout, aux);
				Cdf53VerticalForward(current_w, current_h, current_stride, current_stride, aux, inout);
			}
			else
			{
				HaarHorizontalForward(current_w, current_h, prev_stride, current_stride, inout, aux);
				HaarVerticalForward(current_w, current_h, current_stride, current_stride, aux, inout);
			}

			// Output highpasses
			out -= (hp_w * hp_h);                      // Quadrant D
			Memcpy2d(hp_w, hp_h, current_stride, hp_w, //
			         inout + lp_h * current_stride + lp_w, out);

			out -= (hp_w * lp_h);                      // Quadrant B
			Memcpy2d(hp_w, lp_h, current_stride, hp_w, //
			         inout + lp_w, out);

			out -= (lp_w * hp_h);                      // Quadrant C
			Memcpy2d(lp_w, hp_h, current_stride, lp_w, //
			         inout + lp_h * current_stride, out);

			// printf("! \tHpsCh%u %ux%u = %i, %i, %i\n", ch, lp_w, lp_h, q_c, q_b, q_d);
		}
	}

	// Lowpasses
	for (unsigned ch = (channels - 1); ch < channels; ch -= 1)
	{
		out -= (lp_w * lp_h); // Quadrant A

		const auto lp = input + (width * height) * ch;
		const auto current_stride = (lp_w + hp_w);
		Memcpy2d(lp_w, lp_h, current_stride, lp_w, lp, out);

		if (callbacks.generic_event != nullptr)
			callbacks.generic_event(GenericEvent::LiftLowpassDimensions, lp_w, lp_h, ch,
			                        GenericTypeDesignatedInitialization(*lp), callbacks.user_data);
	}
}


template <>
void Lift(const Callbacks& callbacks, const Wavelet& wavelet_transformation, unsigned width, unsigned height,
          unsigned channels, int16_t* input, int16_t* output)
{
	sLift(callbacks, wavelet_transformation, width, height, channels, input, output);
}

template <>
void Lift(const Callbacks& callbacks, const Wavelet& wavelet_transformation, unsigned width, unsigned height,
          unsigned channels, int32_t* input, int32_t* output)
{
	sLift(callbacks, wavelet_transformation, width, height, channels, input, output);
}

} // namespace ako
