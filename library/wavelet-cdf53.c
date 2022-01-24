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


// Cdf53, Cohen–Daubechies–Feauveau 5/3:
//   > 5/3: { d[n] = d0[n] - floor((1 / 2) * (s0[n] + s0[n + 1])) }
//   > 5/3: { s[n] = s0[n] + floor((1 / 4) * (d[n - 1] + d[n]) + 0.5) }
//   ADAMS, Michael David (2002). Table 5.2. Page 105.

// In other words:

// Forward transform / Lift:
//   hp[n] = odd[n] - (even[n] + even[n + 1]) / 2
//   lp[n] = even[n] + (hp[n - 1] + hp[n]) / 4

// Inverse transform / Unlift:
//   even[n] = lp[n] - (hp[n - 1] + hp[n]) / 4
//   odd[n] = hp[n] + (even[n] + even[n + 1]) / 2


#define HP(odd, even, evenp1) (odd - (even + evenp1) / 2)
#define LP(even, hpl1, hp) (even + (hpl1 + hp) / 4)

#define EVEN(lp, hpl1, hp) (lp - (hpl1 + hp) / 4)
#define ODD(hp, even, evenp1) (hp + (even + evenp1) / 2)


void akoCdf53LiftH(enum akoWrap wrap, int16_t q, size_t current_h, size_t target_w, size_t fake_last, size_t in_stride,
                   const int16_t* in, int16_t* out)
{
	int16_t hp_prev = 0;
	if (q <= 0)
		q = 1;

	for (size_t r = 0; r < current_h; r++)
	{
		// First values
		{
			const size_t c = 0;
			const int16_t even = in[(r * in_stride) + (c * 2 + 0)];
			const int16_t odd = in[(r * in_stride) + (c * 2 + 1)];
			const int16_t even_p1 = in[(r * in_stride) + (c * 2 + 2)];

			const int16_t hp = HP(odd, even, even_p1) / q;
			const int16_t lp = LP(even, hp, hp); // Clamp

			out[(r * target_w * 2) + c + 0] = lp;
			out[(r * target_w * 2) + c + target_w] = hp;
			hp_prev = hp;
		}

		// Middle values
		for (size_t c = 1; c < (target_w - 1); c++)
		{
			const int16_t even = in[(r * in_stride) + (c * 2 + 0)];
			const int16_t odd = in[(r * in_stride) + (c * 2 + 1)];
			const int16_t even_p1 = in[(r * in_stride) + (c * 2 + 2)];

			const int16_t hp = HP(odd, even, even_p1) / q;
			const int16_t lp = LP(even, hp_prev, hp);

			out[(r * target_w * 2) + c + 0] = lp;
			out[(r * target_w * 2) + c + target_w] = hp;
			hp_prev = hp;
		}

		// Last values
		{
			const size_t c = (target_w - 1);
			const int16_t even = in[(r * in_stride) + (c * 2 + 0)];

			const int16_t odd =
			    (fake_last == 0) ? in[(r * in_stride) + (c * 2 + 1)] : in[(r * in_stride) + (c * 2 + 0)];

			const int16_t hp = HP(odd, even, even) / q; // Clamp
			const int16_t lp = LP(even, hp_prev, hp);

			out[(r * target_w * 2) + c + 0] = lp;
			out[(r * target_w * 2) + c + target_w] = hp;
		}
	}
}


void akoCdf53LiftV(enum akoWrap wrap, size_t target_w, size_t current_h, const int16_t* in, int16_t* out)
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


void akoCdf53UnliftH(enum akoWrap wrap, int16_t q, size_t current_w, size_t current_h, size_t out_stride,
                     size_t ignore_last, const int16_t* in_lp, const int16_t* in_hp, int16_t* out)
{
	int16_t even_prev = 0;
	if (q <= 0)
		q = 1;

	for (size_t r = 0; r < current_h; r++)
	{
		// First values
		{
			const size_t c = 0;
			const int16_t hp = in_hp[(r * current_w) + c] * q;
			const int16_t lp = in_lp[(r * current_w) + c];

			const int16_t even = EVEN(lp, hp, hp); // Clamp

			out[(r * out_stride) + (c * 2 + 0)] = even;
			even_prev = even;
		}

		// Middle values
		for (size_t c = 1; c < current_w; c++)
		{
			const int16_t hpl1 = in_hp[(r * current_w) + c - 1] * q;
			const int16_t hp = in_hp[(r * current_w) + c] * q;
			const int16_t lp = in_lp[(r * current_w) + c];

			const int16_t even = EVEN(lp, hpl1, hp);
			const int16_t odd = ODD(hpl1, even_prev, even);

			out[(r * out_stride) + (c * 2 - 1)] = odd;
			out[(r * out_stride) + (c * 2 + 0)] = even;
			even_prev = even;
		}

		// Last values
		if (ignore_last == 0) // Just even (omit next odd)
		{
			const size_t c = current_w;
			const int16_t hpl1 = in_hp[(r * current_w) + c - 1] * q;

			const int16_t odd = ODD(hpl1, even_prev, even_prev); // Clamp

			out[(r * out_stride) + (c * 2 - 1)] = odd;
		}
	}
}


void akoCdf53InPlaceishUnliftV(enum akoWrap wrap, size_t current_w, size_t current_h, const int16_t* in_lp,
                               const int16_t* in_hp, int16_t* out_lp, int16_t* out_hp)
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
