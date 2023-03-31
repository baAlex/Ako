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
static void sHaarHorizontalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                                   const T* input, T* output)
{
	const unsigned half = Half(width);
	const unsigned rule = HalfPlusOneRule(width);

	for (unsigned row = 0; row < height; row += 1)
	{
		// Beginning/middle
		for (unsigned col = 0; col < half - 1; col += 1)
		{
			const T even = input[(col << 1) + 0];
			const T odd = input[(col << 1) + 1];

			const T hp = WrapSubtract(even, odd);
			const T lp = even;

			output[col + 0] = lp;
			output[col + rule] = hp;
		}

		// End
		if (rule == half)
		{
			const unsigned col = half - 1;
			const T even = input[(col << 1) + 0];
			const T odd = input[(col << 1) + 1];

			const T hp = WrapSubtract(even, odd);
			const T lp = even;

			output[col + 0] = lp;
			output[col + rule] = hp;
		}
		else
		{
			for (unsigned col = half - 1; col < rule; col += 1)
			{
				const T even = input[(col << 1) + 0];
				const T odd = (col != rule - 1) ? input[(col << 1) + 1] : even;

				const T hp = WrapSubtract(even, odd);
				const T lp = even;

				output[col + 0] = lp;

				if (col != rule - 1)
					output[col + rule] = hp;
			}
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
	const unsigned rule = HalfPlusOneRule(height);
	const unsigned half = Half(height);

	// Beginning/middle
	for (unsigned row = 0; row < half - 1; row += 1)
	{
		const T* even = input + input_stride * ((row << 1) + 0);
		const T* odd = input + input_stride * ((row << 1) + 1);

		T* lp_out = output + output_stride * (row + 0);
		T* hp_out = output + output_stride * (row + rule);

		for (unsigned col = 0; col < width; col += 1)
		{
			const T hp = WrapSubtract(even[col], odd[col]);
			const T lp = even[col];

			lp_out[col] = lp;
			hp_out[col] = hp;
		}
	}

	// End
	if (rule == half)
	{
		const unsigned row = half - 1;
		const T* even = input + input_stride * ((row << 1) + 0);
		const T* odd = input + input_stride * ((row << 1) + 1);

		T* lp_out = output + output_stride * (row + 0);
		T* hp_out = output + output_stride * (row + rule);

		for (unsigned col = 0; col < width; col += 1)
		{
			const T hp = WrapSubtract(even[col], odd[col]);
			const T lp = even[col];

			lp_out[col] = lp;
			hp_out[col] = hp;
		}
	}
	else
	{
		for (unsigned row = half - 1; row < rule; row += 1)
		{
			const T* even = input + input_stride * ((row << 1) + 0);
			const T* odd = (row != rule - 1) ? (input + input_stride * ((row << 1) + 1)) : even;

			T* lp_out = output + output_stride * (row + 0);
			T* hp_out = output + output_stride * (row + rule);

			for (unsigned col = 0; col < width; col += 1)
			{
				const T hp = WrapSubtract(even[col], odd[col]);
				const T lp = even[col];

				lp_out[col] = lp;

				if (row != rule - 1)
					hp_out[col] = hp;
			}
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
