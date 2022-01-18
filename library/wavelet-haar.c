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


static void sHaarLiftH(size_t current_h, size_t target_w, size_t fake_last, size_t in_stride, const int16_t* in,
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


static void sHaarLiftV(size_t target_w, size_t current_h, const int16_t* in, int16_t* out)
{
	for (size_t r = 0; r < current_h; r++)
	{
		for (size_t c = 0; c < target_w; c++)
		{
			const int16_t even = in[(r * 2 + 0) * target_w + c];
			const int16_t odd = in[(r * 2 + 1) * target_w + c];

			out[(target_w * r) + c] = even;                                // LP
			out[(target_w * (current_h + r)) + c] = (int16_t)(odd - even); // HP
		}
	}
}


static void sHaarUnliftH(size_t current_w, size_t current_h, size_t out_stride, size_t ignore_last,
                         const int16_t* in_lp, const int16_t* in_hp, int16_t* out)
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


static void sHaarInPlaceishUnliftV(size_t current_w, size_t current_h, const int16_t* in_lp, const int16_t* in_hp,
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


//


void akoHaarLift(enum akoWrap wrap, size_t in_stride, size_t current_w, size_t current_h, size_t target_w,
                 size_t target_h, int16_t* lp, int16_t* aux)
{
	(void)wrap;

	const size_t fake_last_col = (target_w * 2) - current_w;
	const size_t fake_last_row = (target_h * 2) - current_h;

	// Horizontal lift
	sHaarLiftH(current_h, target_w, fake_last_col, in_stride, lp, aux);

	if (fake_last_row != 0)
		sHaarLiftH(1, target_w, fake_last_col, 0, lp + in_stride * (current_h - 1), aux + current_h * target_w * 2);

	// Vertical lift
	sHaarLiftV(target_w * 2, target_h, aux, lp);
}


void akoHaarUnlift(enum akoWrap wrap, size_t current_w, size_t current_h, size_t target_w, size_t target_h, int16_t* lp,
                   int16_t* hps, int16_t* aux)
{
	(void)wrap;

	const size_t ignore_last_col = (current_w * 2) - target_w;
	const size_t ignore_last_row = (current_h * 2) - target_h;

	int16_t* hp_c = hps + (current_w * current_h) * 0;
	int16_t* hp_b = hps + (current_w * current_h) * 1;
	int16_t* hp_d = hps + (current_w * current_h) * 2;

	// Vertical unlift
	sHaarInPlaceishUnliftV(current_w, current_h, lp, hp_c, aux, hp_c);
	sHaarInPlaceishUnliftV(current_w, current_h - ignore_last_row, hp_b, hp_d, hp_b, hp_d);

	// Horizontal unlift
	// (with care on that the vertical unlift, done in-place, interleave-ed rows)
	sHaarUnliftH(current_w, current_h, target_w * 2, ignore_last_col, aux, hp_b, lp + 0);
	sHaarUnliftH(current_w, current_h - ignore_last_row, target_w * 2, ignore_last_col, hp_c, hp_d, lp + target_w);
}
