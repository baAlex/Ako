/*

MIT License

Copyright (c) 2021 Alexander Brandt

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


#include "ako-private.h"


void akoHaarLiftH(size_t current_h, size_t target_w, size_t fake_last, size_t in_stride, const int16_t* in,
                  int16_t* out)
{
	for (size_t r = 0; r < current_h; r++)
	{
		for (size_t c = 0; c < (target_w - fake_last); c++)
		{
			const int16_t even = in[(r * in_stride) + (c * 2 + 0)];
			const int16_t odd = in[(r * in_stride) + (c * 2 + 1)];

			out[(r * target_w * 2) + c + 0] = even;                         // LP
			out[(r * target_w * 2) + c + target_w] = (int16_t)(odd - even); // HP
		}

		if (fake_last != 0)
		{
			const size_t c = (target_w - 1);

			const int16_t even = in[(r * in_stride) + (c * 2 + 0)];
			const int16_t odd = in[(r * in_stride) + (c * 2 + 0)];

			out[(r * target_w * 2) + c + 0] = even;                         // LP
			out[(r * target_w * 2) + c + target_w] = (int16_t)(odd - even); // HP
		}
	}
}


void akoHaarLiftV(size_t target_w, size_t target_h, const int16_t* in, int16_t* out)
{
	for (size_t r = 0; r < target_h; r++)
	{
		for (size_t c = 0; c < target_w; c++)
		{
			const int16_t even = in[(r * 2 + 0) * target_w + c];
			const int16_t odd = in[(r * 2 + 1) * target_w + c];

			out[(target_w * r) + c] = even;                               // LP
			out[(target_w * (target_h + r)) + c] = (int16_t)(odd - even); // HP
		}
	}
}


void akoHaarUnliftH(size_t current_w, size_t current_h, size_t out_stride, size_t ignore_last, const int16_t* in_lp,
                    const int16_t* in_hp, int16_t* out)
{
	for (size_t r = 0; r < current_h; r++)
	{
		for (size_t c = 0; c < (current_w - ignore_last); c++)
		{
			const int16_t lp = in_lp[(r * current_w) + c];
			const int16_t hp = in_hp[(r * current_w) + c];

			out[(r * out_stride) + (c * 2 + 0)] = lp;                 // Even
			out[(r * out_stride) + (c * 2 + 1)] = (int16_t)(lp + hp); // Odd
		}

		if (ignore_last != 0)
		{
			const size_t c = (current_w - 1);
			const int16_t lp = in_lp[(r * current_w) + c];

			out[(r * out_stride) + (c * 2 + 0)] = lp; // Just even
		}
	}
}


void akoHaarInPlaceishUnliftV(size_t current_w, size_t current_h, const int16_t* in_lp, const int16_t* in_hp,
                              int16_t* out_lp, int16_t* out_hp)
{
	for (size_t r = 0; r < current_h; r++)
	{
		for (size_t c = 0; c < current_w; c++)
		{
			const int16_t lp = in_lp[(current_w * r) + c];
			const int16_t hp = in_hp[(current_w * r) + c];

			out_lp[(current_w * r) + c] = lp;                 // Even
			out_hp[(current_w * r) + c] = (int16_t)(lp + hp); // Odd
		}
	}
}
