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
static void sHaarHorizontalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                                   const T* input, T* output)
{
	const auto half = Half(width);
	const auto rule = HalfPlusOneRule(width);

	for (unsigned row = 0; row < height; row += 1)
	{
		// Highpass (length of 'half')
		for (unsigned col = 0; col < half; col += 1)
		{
			const auto even = input[(col << 1) + 0];
			const auto odd = input[(col << 1) + 1];

			output[rule + col] = WrapSubtract(even, odd);
		}

		// Lowpass (length of 'rule')
		for (unsigned col = 0; col < rule; col += 1)
		{
			const auto even = input[(col << 1) + 0];
			output[col] = even;
		}

		// Next row
		input += input_stride;
		output += output_stride;
	}
}


template <typename T>
static void sHaarVerticalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                                 const T* input, T* output)
{
	const auto rule = HalfPlusOneRule(height);
	const auto half = Half(height);

	// Highpass (length of 'half')
	for (unsigned row = 0; row < half; row += 1)
	{
		const auto in = input + input_stride * (row << 1);
		auto hp_out = output + output_stride * (row + rule);

		for (unsigned col = 0; col < width; col += 1)
		{
			const auto even = in[col + input_stride * 0];
			const auto odd = in[col + input_stride * 1];

			hp_out[col] = WrapSubtract(even, odd);
		}
	}

	// Lowpass (length of 'rule')
	for (unsigned row = 0; row < rule; row += 1)
	{
		const auto in = input + input_stride * (row << 1);
		auto lp_out = output + output_stride * (row);

		for (unsigned col = 0; col < width; col += 1)
		{
			const auto even = in[col + output_stride * 0];
			lp_out[col] = even;
		}
	}
}


template <>
void HaarHorizontalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                           const int16_t* input, int16_t* output)
{
	sHaarHorizontalForward(width, height, input_stride, output_stride, input, output);
}

template <>
void HaarVerticalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                         const int16_t* input, int16_t* output)
{
	sHaarVerticalForward(width, height, input_stride, output_stride, input, output);
}

template <>
void HaarHorizontalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                           const int32_t* input, int32_t* output)
{
	sHaarHorizontalForward(width, height, input_stride, output_stride, input, output);
}

template <>
void HaarVerticalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                         const int32_t* input, int32_t* output)
{
	sHaarVerticalForward(width, height, input_stride, output_stride, input, output);
}

} // namespace ako
