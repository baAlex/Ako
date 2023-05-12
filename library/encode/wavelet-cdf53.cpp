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

template <typename T> static T sHp(T odd, T even, T even_p1)
{
	return WrapSubtract<T>(odd, WrapAdd(even, even_p1) / 2);
}

template <typename T> static T sLp(T even, T hp_l1, T hp)
{
	return WrapAdd<T>(even, WrapAdd(hp_l1, hp) / 4);
}


template <typename T>
static void sCdf53HorizontalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                                    const T* input, T* output)
{
	const unsigned half = Half(width);
	const unsigned rule = HalfPlusOneRule(width);

	for (unsigned row = 0; row < height; row += 1)
	{
		T hp_l1 = 0;

		// Beginning/middle
		for (unsigned col = 0; col < half - 1; col += 1)
		{
			const T even = input[(col << 1) + 0];
			const T odd = input[(col << 1) + 1];
			const T even_p1 = input[(col << 1) + 2];

			const T hp = sHp(odd, even, even_p1);
			const T lp = sLp(even, hp_l1, hp);

			output[col + 0] = lp;
			output[col + rule] = hp;
			hp_l1 = hp;
		}

		// End
		if (rule == half)
		{
			const unsigned col = half - 1;
			const T even = input[(col << 1) + 0];
			const T odd = input[(col << 1) + 1];
			const T even_p1 = even;

			const T hp = sHp(odd, even, even_p1);
			const T lp = sLp(even, hp_l1, hp);

			output[col + 0] = lp;
			output[col + rule] = hp;
		}
		else
		{
			for (unsigned col = half - 1; col < rule; col += 1)
			{
				const T even = input[(col << 1) + 0];
				const T odd = (col < rule - 1) ? input[(col << 1) + 1] : even;
				const T even_p1 = (col < rule - 1) ? input[(col << 1) + 2] : even;

				const T hp = (col < rule - 1) ? sHp(odd, even, even_p1) : 0;
				const T lp = sLp(even, hp_l1, hp);

				output[col + 0] = lp;
				if (col < rule - 1)
					output[col + rule] = hp;

				hp_l1 = hp;
			}
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
	const unsigned half = Half(height);
	const unsigned rule = HalfPlusOneRule(height);

	// Beginning
	{
		const unsigned row = 0;
		const T* even = input + input_stride * ((row << 1) + 0);
		const T* odd = input + input_stride * ((row << 1) + 1);
		const T* even_p1 = input + input_stride * ((row << 1) + 2);

		T* lp_out = output + output_stride * (row + 0);
		T* hp_out = output + output_stride * (row + rule);

		for (unsigned col = 0; col < width; col += 1)
		{
			const T hp = sHp(odd[col], even[col], even_p1[col]);
			const T lp = sLp(even[col], static_cast<T>(0), hp);

			lp_out[col] = lp;
			hp_out[col] = hp;
		}
	}

	// Middle
	for (unsigned row = 1; row < half - 1; row += 1)
	{
		const T* even = input + input_stride * ((row << 1) + 0);
		const T* odd = input + input_stride * ((row << 1) + 1);
		const T* even_p1 = input + input_stride * ((row << 1) + 2);

		T* lp_out = output + output_stride * (row + 0);
		T* hp_out = output + output_stride * (row + rule);

		T* hp_l1 = output + output_stride * ((row - 1) + rule);

		for (unsigned col = 0; col < width; col += 1)
		{
			const T hp = sHp(odd[col], even[col], even_p1[col]);
			const T lp = sLp(even[col], hp_l1[col], hp);

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
		const T* even_p1 = even;

		T* lp_out = output + output_stride * (row + 0);
		T* hp_out = output + output_stride * (row + rule);

		T* hp_l1 = output + output_stride * ((row - 1) + rule);

		for (unsigned col = 0; col < width; col += 1)
		{
			const T hp = sHp(odd[col], even[col], even_p1[col]);
			const T lp = sLp(even[col], hp_l1[col], hp);

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
			const T* even_p1 = (row != rule - 1) ? (input + input_stride * ((row << 1) + 2)) : even;

			T* lp_out = output + output_stride * (row + 0);
			T* hp_out = output + output_stride * (row + rule);

			T* hp_l1 = output + output_stride * ((row - 1) + rule);

			for (unsigned col = 0; col < width; col += 1)
			{
				const T hp = (row != rule - 1) ? sHp(odd[col], even[col], even_p1[col]) : 0;
				const T lp = sLp(even[col], hp_l1[col], hp);

				lp_out[col] = lp;

				if (row != rule - 1)
					hp_out[col] = hp;
			}
		}
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
