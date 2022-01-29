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


#define HP(odd, even, even_p1) ((odd) - ((even) + (even_p1)) / 2)
#define LP(even, hp_l1, hp) ((even) + ((hp_l1) + (hp)) / 4)

#define EVEN(lp, hp_l1, hp) ((lp) - ((hp_l1) + (hp)) / 4)
#define ODD(hp, even, even_p1) ((hp) + ((even) + (even_p1)) / 2)


void akoCdf53LiftH(enum akoWrap wrap, int16_t q, int16_t g, size_t current_h, size_t target_w, size_t fake_last,
                   size_t in_stride, const int16_t* in, int16_t* out)
{
	int16_t hp_l1 = 0;
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

			const int16_t hp = HP(odd, even, even_p1);

			int16_t lp;
			if (wrap == AKO_WRAP_CLAMP)
				lp = LP(even, hp, hp);
			else if (wrap == AKO_WRAP_REPEAT)
			{
				const size_t last_c = (target_w - 1);
				const int16_t last_even = in[(r * in_stride) + (last_c * 2 + 0)];
				const int16_t last_odd =
				    (fake_last == 0) ? in[(r * in_stride) + (last_c * 2 + 1)] : in[(r * in_stride) + (last_c * 2 + 0)];
				const int16_t last_hp = HP(last_odd, last_even, even);
				lp = LP(even, last_hp, hp);
			}
			else
				lp = LP(even, 0, hp); // Zero

			out[(r * target_w * 2) + c + 0] = lp;
			out[(r * target_w * 2) + c + target_w] = hp;
			hp_l1 = hp;
		}

		// Middle values
		for (size_t c = 1; c < (target_w - 1); c++)
		{
			const int16_t even = in[(r * in_stride) + (c * 2 + 0)];
			const int16_t odd = in[(r * in_stride) + (c * 2 + 1)];
			const int16_t even_p1 = in[(r * in_stride) + (c * 2 + 2)];

			const int16_t hp = HP(odd, even, even_p1);
			const int16_t lp = LP(even, hp_l1, hp);

			out[(r * target_w * 2) + c + 0] = lp;
			out[(r * target_w * 2) + c + target_w] = hp;
			hp_l1 = hp;
		}

		// Last values
		{
			const size_t c = (target_w - 1);
			const int16_t even = in[(r * in_stride) + (c * 2 + 0)];
			const int16_t odd =
			    (fake_last == 0) ? in[(r * in_stride) + (c * 2 + 1)] : in[(r * in_stride) + (c * 2 + 0)];

			int16_t hp;
			if (wrap == AKO_WRAP_CLAMP)
				hp = HP(odd, even, even);
			else if (wrap == AKO_WRAP_REPEAT)
			{
				const int16_t first_even = in[(r * in_stride) + (0 * 2 + 0)];
				hp = HP(odd, even, first_even);
			}
			else
				hp = HP(odd, even, 0); // Zero

			const int16_t lp = LP(even, hp_l1, hp);

			out[(r * target_w * 2) + c + 0] = lp;
			out[(r * target_w * 2) + c + target_w] = hp;
		}

		// Gate
		for (size_t c = 0; c < target_w; c++)
		{
			const int16_t hp = out[(r * target_w * 2) + c + target_w];

			if (hp > -g && hp < g)
				out[(r * target_w * 2) + c + target_w] = 0;
			else
				out[(r * target_w * 2) + c + target_w] = hp / q;
		}
	}
}


void akoCdf53LiftV(enum akoWrap wrap, int16_t q, int16_t g, size_t target_w, size_t target_h, const int16_t* in,
                   int16_t* out)
{
	if (q <= 0)
		q = 1;

	// First values
	{
		const size_t r = 0;
		for (size_t c = 0; c < target_w; c++)
		{
			const int16_t even = in[(r * 2 + 0) * target_w + c];
			const int16_t odd = in[(r * 2 + 1) * target_w + c];
			const int16_t even_p1 = in[(r * 2 + 2) * target_w + c];

			const int16_t hp = HP(odd, even, even_p1);

			int16_t lp;
			if (wrap == AKO_WRAP_CLAMP)
				lp = LP(even, hp, hp);
			else if (wrap == AKO_WRAP_REPEAT)
			{
				const size_t last_r = (target_h - 1);
				const int16_t last_even = in[(last_r * 2 + 0) * target_w + c];
				const int16_t last_odd = in[(last_r * 2 + 1) * target_w + c];
				const int16_t last_hp = HP(last_odd, last_even, even);
				lp = LP(even, last_hp, hp);
			}
			else
				lp = LP(even, 0, hp); // Zero

			out[(target_w * r) + c] = lp;
			out[(target_w * (target_h + r)) + c] = hp;
		}
	}

	// Middle values
	for (size_t r = 1; r < (target_h - 1); r++)
	{
		for (size_t c = 0; c < target_w; c++)
		{
			const int16_t hp_l1 = out[(target_w * (target_h + r - 1)) + c];
			const int16_t even = in[(r * 2 + 0) * target_w + c];
			const int16_t odd = in[(r * 2 + 1) * target_w + c];
			const int16_t even_p1 = in[(r * 2 + 2) * target_w + c];

			const int16_t hp = HP(odd, even, even_p1);
			const int16_t lp = LP(even, hp_l1, hp);

			out[(target_w * r) + c] = lp;
			out[(target_w * (target_h + r)) + c] = hp;
		}
	}

	// Last values
	{
		const size_t r = (target_h - 1);
		for (size_t c = 0; c < target_w; c++)
		{
			const int16_t hp_l1 = out[(target_w * (target_h + r - 1)) + c];
			const int16_t even = in[(r * 2 + 0) * target_w + c];
			const int16_t odd = in[(r * 2 + 1) * target_w + c];

			int16_t hp;
			if (wrap == AKO_WRAP_CLAMP)
				hp = HP(odd, even, even);
			else if (wrap == AKO_WRAP_REPEAT)
			{
				const int16_t first_even = in[(0 * 2 + 0) * target_w + c];
				hp = HP(odd, even, first_even);
			}
			else
				hp = HP(odd, even, 0); // Zero

			const int16_t lp = LP(even, hp_l1, hp);

			out[(target_w * r) + c] = lp;
			out[(target_w * (target_h + r)) + c] = hp;
		}
	}

	// Gate
	for (size_t r = 0; r < target_h; r++)
		for (size_t c = 0; c < target_w; c++)
		{
			const int16_t hp = out[(target_w * (target_h + r)) + c];

			if (hp > -g && hp < g)
				out[(target_w * (target_h + r)) + c] = 0;
			else
				out[(target_w * (target_h + r)) + c] = hp / q;
		}
}


void akoCdf53UnliftH(enum akoWrap wrap, int16_t q, size_t current_w, size_t current_h, size_t out_stride,
                     size_t ignore_last, const int16_t* in_lp, const int16_t* in_hp, int16_t* out)
{
	int16_t even_l1 = 0;
	if (q <= 0)
		q = 1;

	for (size_t r = 0; r < current_h; r++)
	{
		// First values
		{
			const size_t c = 0;
			const int16_t hp = in_hp[(r * current_w) + c] * q;
			const int16_t lp = in_lp[(r * current_w) + c];

			int16_t even;
			if (wrap == AKO_WRAP_CLAMP)
				even = EVEN(lp, hp, hp);
			else if (wrap == AKO_WRAP_REPEAT)
			{
				const int16_t last_hp = in_hp[(r * current_w) + (current_w - 1)] * q;
				even = EVEN(lp, last_hp, hp);
			}
			else
				even = EVEN(lp, 0, hp); // Zero

			out[(r * out_stride) + (c * 2 + 0)] = even;
			even_l1 = even;
		}

		// Middle values
		for (size_t c = 1; c < current_w; c++)
		{
			const int16_t hp_l1 = in_hp[(r * current_w) + c - 1] * q;
			const int16_t hp = in_hp[(r * current_w) + c] * q;
			const int16_t lp = in_lp[(r * current_w) + c];

			const int16_t even = EVEN(lp, hp_l1, hp);
			const int16_t odd = ODD(hp_l1, even_l1, even);

			out[(r * out_stride) + (c * 2 - 1)] = odd;
			out[(r * out_stride) + (c * 2 + 0)] = even;
			even_l1 = even;
		}

		// Last values
		if (ignore_last == 0) // Just even (omit next odd)
		{
			const size_t c = current_w;
			const int16_t hp_l1 = in_hp[(r * current_w) + c - 1] * q;

			int16_t odd;
			if (wrap == AKO_WRAP_CLAMP)
				odd = ODD(hp_l1, even_l1, even_l1);
			else if (wrap == AKO_WRAP_REPEAT)
			{
				const int16_t first_even = out[(r * out_stride) + (0 * 2 + 0)];
				odd = ODD(hp_l1, even_l1, first_even);
			}
			else
				odd = ODD(hp_l1, even_l1, 0); // Zero

			out[(r * out_stride) + (c * 2 - 1)] = odd;
		}
	}
}


void akoCdf53InPlaceishUnliftV(enum akoWrap wrap, int16_t q, size_t current_w, size_t current_h, const int16_t* in_lp,
                               const int16_t* in_hp, int16_t* out_lp, int16_t* out_hp)
{
	if (q <= 0)
		q = 1;

	// First values
	for (size_t c = 0; c < current_w; c++)
	{
		const size_t r = 0;
		const int16_t hp = in_hp[(current_w * r) + c] * q;
		const int16_t lp = in_lp[(current_w * r) + c];

		int16_t even;
		if (wrap == AKO_WRAP_CLAMP)
			even = EVEN(lp, hp, hp);
		else if (wrap == AKO_WRAP_REPEAT)
		{
			const int16_t last_hp = in_hp[(current_w * (current_h - 1)) + c] * q;
			even = EVEN(lp, last_hp, hp);
		}
		else
			even = EVEN(lp, 0, hp); // Zero

		out_lp[(current_w * r) + c] = even;
	}

	// Middle values
	for (size_t r = 1; r < current_h; r++)
	{
		for (size_t c = 0; c < current_w; c++)
		{
			const int16_t even_l1 = out_lp[(current_w * (r - 1)) + c];

			const int16_t hp_l1 = in_hp[(current_w * (r - 1)) + c] * q;
			const int16_t hp = in_hp[(current_w * r) + c] * q;
			const int16_t lp = in_lp[(current_w * r) + c];

			const int16_t even = EVEN(lp, hp_l1, hp);
			const int16_t odd = ODD(hp_l1, even_l1, even);

			out_hp[(current_w * (r - 1)) + c] = odd;
			out_lp[(current_w * r) + c] = even;
		}
	}

	// Last values
	for (size_t c = 0; c < current_w; c++)
	{
		const size_t r = current_h;
		const int16_t even_l1 = out_lp[(current_w * (r - 1)) + c];
		const int16_t hp_l1 = in_hp[(current_w * (r - 1)) + c] * q;

		int16_t odd;
		if (wrap == AKO_WRAP_CLAMP)
			odd = ODD(hp_l1, even_l1, even_l1);
		else if (wrap == AKO_WRAP_REPEAT)
		{
			const int16_t first_even = out_lp[(current_w * 0) + c];
			odd = ODD(hp_l1, even_l1, first_even);
		}
		else
			odd = ODD(hp_l1, even_l1, 0); // Zero

		out_hp[(current_w * (r - 1)) + c] = odd;
	}
}
