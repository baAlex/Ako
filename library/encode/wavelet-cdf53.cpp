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
static void sCdf53HorizontalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                                    const T* input, T* output)
{
	const auto half = Half(width);
	const auto rule = HalfPlusOneRule(width);

	for (unsigned row = 0; row < height; row += 1)
	{
		// Highpass (length of 'half')
		for (unsigned col = 0; col < half; col += 1)
		{
			const T even = input[(col << 1) + 0];
			const T odd = input[(col << 1) + 1];
			const T even_p1 = (col < half - 1) ? input[(col << 1) + 2] : even; // Clamp

			output[rule + col] = WrapSubtract<T>(odd, WrapAdd(even, even_p1) / 2);
		}

		// Lowpass (length of 'rule')
		for (unsigned col = 0; col < half; col += 1)
		{
			const T even = input[(col << 1) + 0];
			const T hp = output[col + rule + 0];
			const T hp_l1 = (col > 0) ? output[col + rule - 1] : hp; // Clamp

			output[col] = WrapAdd<T>(even, WrapAdd(hp_l1, hp) / 4);
		}

		if (rule != half) // If length wasn't divisible by two, complete lowpass
		{
			const unsigned col = half;

			const T even = input[(col << 1) + 0];
			const T hp = output[col + half + 0];    // 'half' is the only change from above routine
			const T hp_l1 = output[col + half - 1]; // Clamp

			output[col] = WrapAdd<T>(even, WrapAdd(hp_l1, hp) / 4);
		}

		// Next row
		input += input_stride;
		output += output_stride;
	}
}


template <typename T>
static void sCdf53VerticalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                                  const T* input, T* output)
{
	const auto half = Half(height);
	const auto rule = HalfPlusOneRule(height);

	// Highpass (length of 'half')
	for (unsigned row = 0; row < half; row += 1)
	{
		const T* even = input + input_stride * ((row << 1) + 0);
		const T* odd = input + input_stride * ((row << 1) + 1);
		const T* even_p1 = (row < half - 1) ? (input + input_stride * ((row << 1) + 2)) : even; // Clamp

		T* out = output + output_stride * (row + rule + 0);

		for (unsigned col = 0; col < width; col += 1)
			out[col] = WrapSubtract<T>(odd[col], WrapAdd(even[col], even_p1[col]) / 2);
	}

	// Lowpass (length of 'rule')
	for (unsigned row = 0; row < half; row += 1)
	{
		const T* even = input + input_stride * ((row << 1) + 0);
		const T* hp = output + output_stride * (row + rule + 0);
		const T* hp_l1 = (row > 0) ? (output + output_stride * (row + rule - 1)) : hp; // Clamp

		T* out = output + output_stride * (row);

		for (unsigned col = 0; col < width; col += 1)
			out[col] = WrapAdd<T>(even[col], WrapAdd(hp_l1[col], hp[col]) / 4);
	}

	if (rule != half) // If length wasn't divisible by two, complete lowpass
	{
		const unsigned row = half;

		const T* even = input + input_stride * ((row << 1) + 0);
		const T* hp = output + output_stride * (row + half + 0);    // 'half' is the only change from above routine
		const T* hp_l1 = output + output_stride * (row + half - 1); // Clamp

		T* out = output + output_stride * (row);

		for (unsigned col = 0; col < width; col += 1)
			out[col] = WrapAdd<T>(even[col], WrapAdd(hp_l1[col], hp[col]) / 4);
	}
}


template <>
void Cdf53HorizontalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                            const int16_t* input, int16_t* output)
{
	sCdf53HorizontalForward(width, height, input_stride, output_stride, input, output);
}

template <>
void Cdf53VerticalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                          const int16_t* input, int16_t* output)
{
	sCdf53VerticalForward(width, height, input_stride, output_stride, input, output);
}

template <>
void Cdf53HorizontalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                            const int32_t* input, int32_t* output)
{
	sCdf53HorizontalForward(width, height, input_stride, output_stride, input, output);
}

template <>
void Cdf53VerticalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                          const int32_t* input, int32_t* output)
{
	sCdf53VerticalForward(width, height, input_stride, output_stride, input, output);
}

} // namespace ako
