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


// Dd137, Deslauriers-Dubuc 13/7:
//   > 13/7-T: { d[n] = d0[n] + floor((1 / 16) * ((s0[n - 1] + s0[n + 2]) - 9 * (s0[n] + s0[n + 1])) + 0.5) }
//   > 13/7-T: { s[n] = s0[n] + floor((1 / 32) * ((-d[n - 2] - d[n + 1]) + 9 * (d[n - 1] + d[n])) + 0.5) }
//   (ADAMS, 2002; Table 5.2. Page 105)

// In other words:

// Forward transform / Lift:
//   hp[n] = odd[n] + ((even[n - 1] + even[n + 2]) - 9 * (even[n] + even[n + 1])) / 16
//   lp[n] = even[n] + ((-hp[n - 2] - hp[n + 1]) + 9 * (hp[n - 1] + hp[n])) / 32

// Inverse transform / Unlift:
//   even[n] = lp[n] - ((-hp[n - 2] - hp[n + 1]) + 9 * (hp[n - 1] + hp[n])) / 32
//   odd[n] = hp[n] - ((even[n - 1] + even[n + 2]) - 9 * (even[n] + even[n + 1])) / 16


static inline int16_t sHp(int16_t odd, int16_t even_l1, int16_t even, int16_t even_p1, int16_t even_p2)
{
	return odd + ((even_l1 + even_p2 - 9 * (even + even_p1) + 8) >> 4);
}

static inline int16_t sLp(int16_t even, int16_t hp_l2, int16_t hp_l1, int16_t hp, int16_t hp_p1)
{
	return even + ((-hp_l2 - hp_p1 + 9 * (hp_l1 + hp) + 16) >> 5);
}

static inline int16_t sEven(int16_t lp, int16_t hp_l2, int16_t hp_l1, int16_t hp, int16_t hp_p1)
{
	return lp - ((-hp_l2 - hp_p1 + 9 * (hp_l1 + hp) + 16) >> 5);
}

static inline int16_t sOdd(int16_t hp, int16_t even_l1, int16_t even, int16_t even_p1, int16_t even_p2)
{
	return hp - ((even_l1 + even_p2 - 9 * (even + even_p1) + 8) >> 4);
}


void akoDd137LiftH(enum akoWrap wrap, int16_t q, size_t current_h, size_t target_w, size_t fake_last, size_t in_stride,
                   const int16_t* in, int16_t* out)
{
	for (size_t r = 0; r < current_h; r++)
	{
		// HP
		for (size_t c = 0; c < 1; c++)
		{
			const int16_t even = in[(r * in_stride) + (c * 2 + 0)];
			const int16_t odd = in[(r * in_stride) + (c * 2 + 1)];
			const int16_t even_p1 = in[(r * in_stride) + (c * 2 + 2)];
			const int16_t even_p2 = in[(r * in_stride) + (c * 2 + 4)];

			int16_t even_l1;
			even_l1 = (c > 0) ? in[(r * in_stride) + (c * 2 - 2)] : even; // Clamp

			const int16_t hp = sHp(odd, even_l1, even, even_p1, even_p2);
			out[(r * target_w * 2) + (c + target_w)] = hp;
		}

		for (size_t c = 1; c < (target_w - 2); c++)
		{
			const int16_t even_l1 = in[(r * in_stride) + (c * 2 - 2)];
			const int16_t even = in[(r * in_stride) + (c * 2 + 0)];
			const int16_t odd = in[(r * in_stride) + (c * 2 + 1)];
			const int16_t even_p1 = in[(r * in_stride) + (c * 2 + 2)];
			const int16_t even_p2 = in[(r * in_stride) + (c * 2 + 4)];

			const int16_t hp = sHp(odd, even_l1, even, even_p1, even_p2);
			out[(r * target_w * 2) + (c + target_w)] = hp;
		}

		for (size_t c = (target_w - 2); c < target_w; c++)
		{
			const int16_t even_l1 = in[(r * in_stride) + (c * 2 - 2)];
			const int16_t even = in[(r * in_stride) + (c * 2 + 0)];

			int16_t even_p1;
			even_p1 = (c < (target_w - 1)) ? in[(r * in_stride) + (c * 2 + 2)] : even; // Clamp

			int16_t even_p2;
			even_p2 = (c < (target_w - 2)) ? in[(r * in_stride) + (c * 2 + 4)] : even_p1; // Clamp

			int16_t odd;
			odd = (c < (target_w - 1) || fake_last == 0) ? in[(r * in_stride) + (c * 2 + 1)] : even_p2; // Clamp

			const int16_t hp = sHp(odd, even_l1, even, even_p1, even_p2);
			out[(r * target_w * 2) + (c + target_w)] = hp;
		}

		// LP
		for (size_t c = 0; c < 2; c++)
		{
			const int16_t even = in[(r * in_stride) + (c * 2 + 0)];
			const int16_t hp = out[(r * target_w * 2) + (c + target_w + 0)];
			const int16_t hp_p1 = out[(r * target_w * 2) + (c + target_w + 1)];

			int16_t hp_l1;
			hp_l1 = (c > 0) ? out[(r * target_w * 2) + (c + target_w - 1)] : hp; // Clamp

			int16_t hp_l2;
			hp_l2 = (c > 1) ? out[(r * target_w * 2) + (c + target_w - 2)] : hp_l1; // Clamp

			const int16_t lp = sLp(even, hp_l2, hp_l1, hp, hp_p1);
			out[(r * target_w * 2) + (c)] = lp;
		}

		for (size_t c = 2; c < (target_w - 1); c++)
		{
			const int16_t even = in[(r * in_stride) + (c * 2 + 0)];
			const int16_t hp_l2 = out[(r * target_w * 2) + (c + target_w - 2)];
			const int16_t hp_l1 = out[(r * target_w * 2) + (c + target_w - 1)];
			const int16_t hp = out[(r * target_w * 2) + (c + target_w + 0)];
			const int16_t hp_p1 = out[(r * target_w * 2) + (c + target_w + 1)];

			const int16_t lp = sLp(even, hp_l2, hp_l1, hp, hp_p1);
			out[(r * target_w * 2) + (c)] = lp;
		}

		for (size_t c = (target_w - 1); c < target_w; c++)
		{
			const int16_t even = in[(r * in_stride) + (c * 2 + 0)];
			const int16_t hp_l2 = out[(r * target_w * 2) + (c + target_w - 2)];
			const int16_t hp_l1 = out[(r * target_w * 2) + (c + target_w - 1)];
			const int16_t hp = out[(r * target_w * 2) + (c + target_w + 0)];

			int16_t hp_p1;
			hp_p1 = (c < (target_w - 1)) ? out[(r * target_w * 2) + (c + target_w + 1)] : hp; // Clamp

			const int16_t lp = sLp(even, hp_l2, hp_l1, hp, hp_p1);
			out[(r * target_w * 2) + (c)] = lp;
		}

		// Quantize
		for (size_t c = 0; c < target_w; c++)
		{
			const int16_t hp = out[(r * target_w * 2) + c + target_w];
			out[(r * target_w * 2) + c + target_w] = hp / q;
		}
	}
}


void akoDd137LiftV(enum akoWrap wrap, int16_t q, size_t target_w, size_t target_h, const int16_t* in, int16_t* out)
{
	// HP
	for (size_t r = 0; r < 1; r++)
	{
		for (size_t c = 0; c < target_w; c++)
		{
			const int16_t even = in[(r * 2 + 0) * target_w + c];
			const int16_t odd = in[(r * 2 + 1) * target_w + c];
			const int16_t even_p1 = in[(r * 2 + 2) * target_w + c];
			const int16_t even_p2 = in[(r * 2 + 4) * target_w + c];

			int16_t even_l1;
			even_l1 = (r > 0) ? in[(r * 2 - 2) * target_w + c] : even; // Clamp

			const int16_t hp = sHp(odd, even_l1, even, even_p1, even_p2);
			out[(target_w * (target_h + r)) + c] = hp;
		}
	}

	for (size_t r = 1; r < (target_h - 2); r++)
	{
		for (size_t c = 0; c < target_w; c++)
		{
			const int16_t even_l1 = in[(r * 2 - 2) * target_w + c];
			const int16_t even = in[(r * 2 + 0) * target_w + c];
			const int16_t odd = in[(r * 2 + 1) * target_w + c];
			const int16_t even_p1 = in[(r * 2 + 2) * target_w + c];
			const int16_t even_p2 = in[(r * 2 + 4) * target_w + c];

			const int16_t hp = sHp(odd, even_l1, even, even_p1, even_p2);
			out[(target_w * (target_h + r)) + (c)] = hp;
		}
	}

	for (size_t r = (target_h - 2); r < target_h; r++)
	{
		for (size_t c = 0; c < target_w; c++)
		{
			const int16_t even_l1 = in[(r * 2 - 2) * target_w + c];
			const int16_t even = in[(r * 2 + 0) * target_w + c];
			const int16_t odd = in[(r * 2 + 1) * target_w + c];

			int16_t even_p1;
			even_p1 = (r < (target_h - 1)) ? in[(r * 2 + 2) * target_w + c] : even; // Clamp

			int16_t even_p2;
			even_p2 = (r < (target_h - 2)) ? in[(r * 2 + 4) * target_w + c] : even_p1; // Clamp

			const int16_t hp = sHp(odd, even_l1, even, even_p1, even_p2);
			out[(target_w * (target_h + r)) + (c)] = hp;
		}
	}

	// LP
	for (size_t r = 0; r < 2; r++)
	{
		for (size_t c = 0; c < target_w; c++)
		{
			const int16_t even = in[(r * 2 + 0) * target_w + c];
			const int16_t hp = out[(target_w * (target_h + r + 0)) + c];
			const int16_t hp_p1 = out[(target_w * (target_h + r + 1)) + c];

			int16_t hp_l1;
			hp_l1 = (r > 0) ? out[(target_w * (target_h + r - 1)) + c] : hp; // Clamp

			int16_t hp_l2;
			hp_l2 = (r > 1) ? out[(target_w * (target_h + r - 2)) + c] : hp_l1; // Clamp

			const int16_t lp = sLp(even, hp_l2, hp_l1, hp, hp_p1);
			out[(target_w * r) + c] = lp;
		}
	}

	for (size_t r = 2; r < (target_h - 1); r++)
	{
		for (size_t c = 0; c < target_w; c++)
		{
			const int16_t even = in[(r * 2 + 0) * target_w + c];
			const int16_t hp_l2 = out[(target_w * (target_h + r - 2)) + c];
			const int16_t hp_l1 = out[(target_w * (target_h + r - 1)) + c];
			const int16_t hp = out[(target_w * (target_h + r + 0)) + c];
			const int16_t hp_p1 = out[(target_w * (target_h + r + 1)) + c];

			const int16_t lp = sLp(even, hp_l2, hp_l1, hp, hp_p1);
			out[(target_w * r) + (c)] = lp;
		}
	}

	for (size_t r = (target_h - 1); r < target_h; r++)
	{
		for (size_t c = 0; c < target_w; c++)
		{
			const int16_t even = in[(r * 2 + 0) * target_w + c];
			const int16_t hp_l2 = out[(target_w * (target_h + r - 2)) + c];
			const int16_t hp_l1 = out[(target_w * (target_h + r - 1)) + c];
			const int16_t hp = out[(target_w * (target_h + r + 0)) + c];

			int16_t hp_p1;
			hp_p1 = (r < (target_h - 1)) ? out[(target_w * (target_h + r + 1)) + c] : hp; // Clamp

			const int16_t lp = sLp(even, hp_l2, hp_l1, hp, hp_p1);
			out[(target_w * r) + (c)] = lp;
		}
	}

	// Quantize
	for (size_t r = 0; r < target_h; r++)
		for (size_t c = 0; c < target_w; c++)
		{
			const int16_t hp = out[(target_w * (target_h + r)) + c];
			out[(target_w * (target_h + r)) + c] = hp / q;
		}
}


void akoDd137UnliftH(enum akoWrap wrap, size_t current_w, size_t current_h, size_t out_stride, size_t ignore_last,
                     const int16_t* in_lp, const int16_t* in_hp, int16_t* out)
{
	const size_t odd_delay = 2;

	for (size_t r = 0; r < current_h; r++)
	{
		// First values
		for (size_t c = 0; c < (odd_delay + 1); c++)
		{
			const int16_t lp = in_lp[(r * current_w) + (c)];
			const int16_t hp = in_hp[(r * current_w) + (c + 0)];
			const int16_t hp_p1 = in_hp[(r * current_w) + (c + 1)];

			int16_t hp_l1;
			hp_l1 = (c > 0) ? in_hp[(r * current_w) + (c - 1)] : hp; // Clamp

			int16_t hp_l2;
			hp_l2 = (c > 1) ? in_hp[(r * current_w) + (c - 2)] : hp_l1; // Clamp

			const int16_t even = sEven(lp, hp_l2, hp_l1, hp, hp_p1);
			out[(r * out_stride) + (c * 2 + 0)] = even;
		}

		for (size_t c = 0; c < 1; c++)
		{
			const int16_t hp = in_hp[(r * current_w) + (c + 0)];
			const int16_t even = out[(r * out_stride) + (c * 2 + 0)];
			const int16_t even_p1 = out[(r * out_stride) + (c * 2 + 2)];
			const int16_t even_p2 = out[(r * out_stride) + (c * 2 + 4)];

			int16_t even_l1;
			even_l1 = (c > 0) ? out[(r * out_stride) + (c * 2 - 2)] : even; // Clamp

			const int16_t odd = sOdd(hp, even_l1, even, even_p1, even_p2);
			out[(r * out_stride) + (c * 2 + 1)] = odd;
		}

		// Middle values
		for (size_t c = (odd_delay + 1); c < (current_w - 2); c++)
		{
			const int16_t lp = in_lp[(r * current_w) + (c)];
			const int16_t hp_l2 = in_hp[(r * current_w) + (c - 2)];
			const int16_t hp_l1 = in_hp[(r * current_w) + (c - 1)];
			const int16_t hp = in_hp[(r * current_w) + (c + 0)];
			const int16_t hp_p1 = in_hp[(r * current_w) + (c + 1)];

			const int16_t even = sEven(lp, hp_l2, hp_l1, hp, hp_p1);
			out[(r * out_stride) + (c * 2 + 0)] = even;
		}

#pragma unroll 16
		for (size_t c = (odd_delay + 1); c < (current_w - 2); c++)
		{
			const int16_t hp = in_hp[(r * current_w) + (c + 0) - odd_delay];
			const int16_t even_l1 = out[(r * out_stride) + (c * 2 - 2) - odd_delay * 2];
			const int16_t even = out[(r * out_stride) + (c * 2 + 0) - odd_delay * 2];
			const int16_t even_p1 = out[(r * out_stride) + (c * 2 + 2) - odd_delay * 2];
			const int16_t even_p2 = out[(r * out_stride) + (c * 2 + 4) - odd_delay * 2];

			const int16_t odd = sOdd(hp, even_l1, even, even_p1, even_p2);
			out[(r * out_stride) + (c * 2 + 1) - odd_delay * 2] = odd;
		}

		// Last values
		for (size_t c = (current_w - 2); c < current_w; c++)
		{
			const int16_t lp = in_lp[(r * current_w) + (c)];
			const int16_t hp_l2 = in_hp[(r * current_w) + (c - 2)];
			const int16_t hp_l1 = in_hp[(r * current_w) + (c - 1)];
			const int16_t hp = in_hp[(r * current_w) + (c + 0)];

			int16_t hp_p1;
			hp_p1 = (c < (current_w - 1)) ? in_hp[(r * current_w) + (c + 1)] : hp; // Clamp

			const int16_t even = sEven(lp, hp_l2, hp_l1, hp, hp_p1);
			out[(r * out_stride) + (c * 2 + 0)] = even;
		}

		for (size_t c = (current_w - 2 - odd_delay); c < current_w; c++)
		{
			if (c == (current_w - 1) && ignore_last == 1)
				break;

			const int16_t hp = in_hp[(r * current_w) + (c + 0)];
			const int16_t even_l1 = out[(r * out_stride) + (c * 2 - 2)];
			const int16_t even = out[(r * out_stride) + (c * 2 + 0)];

			int16_t even_p1;
			even_p1 = (c < current_w - 1) ? out[(r * out_stride) + (c * 2 + 2)] : even; // Clamp

			int16_t even_p2;
			even_p2 = (c < current_w - 2) ? out[(r * out_stride) + (c * 2 + 4)] : even_p1; // Clamp

			const int16_t odd = sOdd(hp, even_l1, even, even_p1, even_p2);
			out[(r * out_stride) + (c * 2 + 1)] = odd;
		}
	}
}


void akoDd137InPlaceishUnliftV(enum akoWrap wrap, size_t current_w, size_t current_h, const int16_t* in_lp,
                               const int16_t* in_hp, int16_t* out_lp, int16_t* out_hp)
{
	const size_t odd_delay = 2;

	// First values
	for (size_t r = 0; r < (odd_delay + 1); r++)
	{
		for (size_t c = 0; c < current_w; c++)
		{
			const int16_t lp = in_lp[(r + 0) * current_w + c];
			const int16_t hp = in_hp[(r + 0) * current_w + c];
			const int16_t hp_p1 = in_hp[(r + 1) * current_w + c];

			int16_t hp_l1;
			hp_l1 = (r > 0) ? in_hp[(r - 1) * current_w + c] : hp; // Clamp

			int16_t hp_l2;
			hp_l2 = (r > 1) ? in_hp[(r - 2) * current_w + c] : hp_l1; // Clamp

			const int16_t even = sEven(lp, hp_l2, hp_l1, hp, hp_p1);
			out_lp[(r * current_w) + c] = even;
		}
	}

	for (size_t r = 0; r < 1; r++)
	{
		for (size_t c = 0; c < current_w; c++)
		{
			const int16_t hp = in_hp[(r + 0) * current_w + c];
			const int16_t even = out_lp[((r + 0) * current_w) + c];
			const int16_t even_p1 = out_lp[((r + 1) * current_w) + c];
			const int16_t even_p2 = out_lp[((r + 2) * current_w) + c];

			int16_t even_l1;
			even_l1 = (r > 0) ? out_lp[(r - 1) * current_w + c] : even; // Clamp

			const int16_t odd = sOdd(hp, even_l1, even, even_p1, even_p2);
			out_hp[(r * current_w) + c] = odd;
		}
	}

	// Middle values
	for (size_t r = (odd_delay + 1); r < (current_h - 2); r++)
	{
		for (size_t c = 0; c < current_w; c++)
		{
			const int16_t lp = in_lp[(r + 0) * current_w + c];
			const int16_t hp_l2 = in_hp[(r - 2) * current_w + c];
			const int16_t hp_l1 = in_hp[(r - 1) * current_w + c];
			const int16_t hp = in_hp[(r + 0) * current_w + c];
			const int16_t hp_p1 = in_hp[(r + 1) * current_w + c];

			const int16_t even = sEven(lp, hp_l2, hp_l1, hp, hp_p1);
			out_lp[(r * current_w) + c] = even;
		}

#pragma unroll 16
		for (size_t c = 0; c < current_w; c++)
		{
			const int16_t hp = in_hp[(r + 0 - odd_delay) * current_w + c];
			const int16_t even_l1 = out_lp[(r - 1 - odd_delay) * current_w + c];
			const int16_t even = out_lp[((r + 0 - odd_delay) * current_w) + c];
			const int16_t even_p1 = out_lp[((r + 1 - odd_delay) * current_w) + c];
			const int16_t even_p2 = out_lp[((r + 2 - odd_delay) * current_w) + c];

			const int16_t odd = sOdd(hp, even_l1, even, even_p1, even_p2);
			out_hp[((r - odd_delay) * current_w) + c] = odd;
		}
	}

	// Last values
	for (size_t r = (current_h - 2); r < current_h; r++)
	{
		for (size_t c = 0; c < current_w; c++)
		{
			const int16_t lp = in_lp[(r + 0) * current_w + c];
			const int16_t hp_l2 = in_hp[(r - 2) * current_w + c];
			const int16_t hp_l1 = in_hp[(r - 1) * current_w + c];
			const int16_t hp = in_hp[(r + 0) * current_w + c];

			int16_t hp_p1;
			hp_p1 = (r < (current_h - 1)) ? in_hp[(r + 1) * current_w + c] : hp; // Clamp

			const int16_t even = sEven(lp, hp_l2, hp_l1, hp, hp_p1);
			out_lp[(r * current_w) + c] = even;
		}
	}

	for (size_t r = (current_h - 2 - odd_delay); r < current_h; r++)
	{
		for (size_t c = 0; c < current_w; c++)
		{
			const int16_t hp = in_hp[(r + 0) * current_w + c];
			const int16_t even_l1 = out_lp[(r - 1) * current_w + c];
			const int16_t even = out_lp[((r + 0) * current_w) + c];

			int16_t even_p1;
			even_p1 = (r < (current_h - 1)) ? out_lp[((r + 1) * current_w) + c] : even; // Clamp

			int16_t even_p2;
			even_p2 = (r < (current_h - 2)) ? out_lp[((r + 2) * current_w) + c] : even_p1; // Clamp

			const int16_t odd = sOdd(hp, even_l1, even, even_p1, even_p2);
			out_hp[(r * current_w) + c] = odd;
		}
	}
}
