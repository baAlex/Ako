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
//   (ADAMS, 2002; Table 5.2. Page 105)


static inline int16_t sHp(int16_t odd, int16_t even, int16_t even_p1)
{
	return odd - (even + even_p1) / 2;
}

static inline int16_t sLp(int16_t even, int16_t hp_l1, int16_t hp)
{
	return even + (hp_l1 + hp) / 4;
}

static inline int16_t sEven(int16_t lp, int16_t hp_l1, int16_t hp)
{
	return lp - (hp_l1 + hp) / 4;
}

static inline int16_t sOdd(int16_t hp, int16_t even, int16_t even_p1)
{
	return hp + (even + even_p1) / 2;
}


void akoCdf53LiftH(enum akoWrap wrap, size_t current_h, size_t target_w, size_t fake_last, size_t in_stride,
                   const int16_t* in, int16_t* out)
{
	for (size_t r = 0; r < current_h; r++)
	{
		// HP, except last
		for (size_t c = 0; c < (target_w - 1); c++)
		{
			const int16_t even = in[(r * in_stride) + (c * 2 + 0)];
			const int16_t odd = in[(r * in_stride) + (c * 2 + 1)];
			const int16_t even_p1 = in[(r * in_stride) + (c * 2 + 2)];

			out[(r * target_w * 2) + (c + target_w)] = sHp(odd, even, even_p1);
		}

		// HP, last value
		{
			const size_t c = (target_w - 1);
			const int16_t even = in[(r * in_stride) + (c * 2 + 0)];

			int16_t even_p1;
			switch (wrap)
			{
			case AKO_WRAP_CLAMP: // falltrough
			case AKO_WRAP_MIRROR: even_p1 = even; break;
			case AKO_WRAP_REPEAT: even_p1 = in[(r * in_stride)]; break;
			case AKO_WRAP_ZERO: even_p1 = 0; break;
			}

			int16_t odd;
			if (fake_last == 0)
				odd = in[(r * in_stride) + (c * 2 + 1)];
			else
				odd = even;

			out[(r * target_w * 2) + (c + target_w)] = sHp(odd, even, even_p1);
		}

		// LP, first value
		{
			const size_t c = 0;
			const int16_t even = in[(r * in_stride) + (c * 2 + 0)];
			const int16_t hp = out[(r * target_w * 2) + (c + target_w + 0)];

			int16_t hp_l1;
			switch (wrap)
			{
			case AKO_WRAP_CLAMP: // falltrough
			case AKO_WRAP_MIRROR: hp_l1 = hp; break;
			case AKO_WRAP_REPEAT: hp_l1 = out[(r * target_w * 2) + (target_w * 2 - 1 + c)]; break;
			case AKO_WRAP_ZERO: hp_l1 = 0; break;
			}

			out[(r * target_w * 2) + c] = sLp(even, hp_l1, hp);
		}

		// LP, remaining values
		for (size_t c = 1; c < target_w; c++)
		{
			const int16_t even = in[(r * in_stride) + (c * 2 + 0)];
			const int16_t hp_l1 = out[(r * target_w * 2) + (c + target_w - 1)];
			const int16_t hp = out[(r * target_w * 2) + (c + target_w + 0)];

			out[(r * target_w * 2) + c] = sLp(even, hp_l1, hp);
		}
	}
}


void akoCdf53LiftV(enum akoWrap wrap, size_t target_w, size_t target_h, const int16_t* in, int16_t* out)
{
	// HP, except last
	for (size_t r = 0; r < (target_h - 1); r++)
	{
		for (size_t c = 0; c < target_w; c++)
		{
			const int16_t even = in[(r * 2 + 0) * target_w + c];
			const int16_t odd = in[(r * 2 + 1) * target_w + c];
			const int16_t even_p1 = in[(r * 2 + 2) * target_w + c];

			out[target_w * (target_h + r) + c] = sHp(odd, even, even_p1);
		}
	}

	// HP, last value
	{
		const size_t r = (target_h - 1);
		for (size_t c = 0; c < target_w; c++)
		{
			const int16_t even = in[(r * 2 + 0) * target_w + c];
			const int16_t odd = in[(r * 2 + 1) * target_w + c];

			int16_t even_p1;
			switch (wrap)
			{
			case AKO_WRAP_CLAMP: // falltrough
			case AKO_WRAP_MIRROR: even_p1 = even; break;
			case AKO_WRAP_REPEAT: even_p1 = in[c]; break;
			case AKO_WRAP_ZERO: even_p1 = 0; break;
			}

			out[target_w * (target_h + r) + c] = sHp(odd, even, even_p1);
		}
	}

	// LP, first value
	{
		const size_t r = 0;
		for (size_t c = 0; c < target_w; c++)
		{
			const int16_t even = in[(r * 2 + 0) * target_w + c];
			const int16_t hp = out[target_w * (target_h + r + 0) + c];

			int16_t hp_l1;
			switch (wrap)
			{
			case AKO_WRAP_CLAMP: // falltrough
			case AKO_WRAP_MIRROR: hp_l1 = hp; break;
			case AKO_WRAP_REPEAT: hp_l1 = out[target_w * (target_h * 2 - 1) + c]; break;
			case AKO_WRAP_ZERO: hp_l1 = 0; break;
			}

			out[(target_w * r) + c] = sLp(even, hp_l1, hp);
		}
	}

	// LP, remaining values
	for (size_t r = 1; r < target_h; r++)
	{
		for (size_t c = 0; c < target_w; c++)
		{
			const int16_t even = in[(r * 2 + 0) * target_w + c];
			const int16_t hp_l1 = out[target_w * (target_h + r - 1) + c];
			const int16_t hp = out[target_w * (target_h + r + 0) + c];

			out[(target_w * r) + c] = sLp(even, hp_l1, hp);
		}
	}
}


#define ODD_DELAY 1 // We need at minimum one even to calculate an odd

void akoCdf53UnliftH(enum akoWrap wrap, size_t current_w, size_t current_h, size_t out_stride, size_t ignore_last,
                     const int16_t* in_lp, const int16_t* in_hp, int16_t* out)
{
	for (size_t r = 0; r < current_h; r++)
	{
		// Even, first values
		for (size_t c = 0; c < (ODD_DELAY + 1); c++)
		{
			const int16_t lp = in_lp[(r * current_w) + (c + 0)];
			const int16_t hp = in_hp[(r * current_w) + (c + 0)];

			int16_t hp_l1;
			if (c > 0)
				hp_l1 = in_hp[(r * current_w) + (c - 1)];
			else
				switch (wrap)
				{
				case AKO_WRAP_CLAMP: // falltrough
				case AKO_WRAP_MIRROR: hp_l1 = hp; break;
				case AKO_WRAP_REPEAT: hp_l1 = in_hp[(r * current_w) + (current_w - 1 + c)]; break;
				case AKO_WRAP_ZERO: hp_l1 = 0; break;
				}

			out[(r * out_stride) + (c * 2 + 0)] = sEven(lp, hp_l1, hp);
		}

		// Odd, first value
		{
			const size_t c = 0;
			const int16_t hp = in_hp[(r * current_w) + (c + 0)];
			const int16_t even = out[(r * out_stride) + (c * 2 + 0)];
			const int16_t even_p1 = out[(r * out_stride) + (c * 2 + 2)];

			out[(r * out_stride) + (c * 2 + 1)] = sOdd(hp, even, even_p1);
		}

		// Middle values
		for (size_t c = (ODD_DELAY + 1); c < (current_w - 1); c++)
		{
			const int16_t lp = in_lp[(r * current_w) + (c + 0)];
			const int16_t hp_l1 = in_hp[(r * current_w) + (c - 1)];
			const int16_t hp = in_hp[(r * current_w) + (c + 0)];

			out[(r * out_stride) + (c * 2 + 0)] = sEven(lp, hp_l1, hp);
		}

		for (size_t c = (ODD_DELAY + 1); c < (current_w - 1); c++)
		{
			const int16_t hp = in_hp[(r * current_w) + (c + 0) - ODD_DELAY];
			const int16_t even = out[(r * out_stride) + (c * 2 + 0) - ODD_DELAY * 2];
			const int16_t even_p1 = out[(r * out_stride) + (c * 2 + 2) - ODD_DELAY * 2];

			out[(r * out_stride) + (c * 2 + 1) - ODD_DELAY * 2] = sOdd(hp, even, even_p1);
		}

		// Even, last value
		{
			const size_t c = (current_w - 1);
			const int16_t lp = in_lp[(r * current_w) + (c + 0)];
			const int16_t hp_l1 = in_hp[(r * current_w) + (c - 1)];
			const int16_t hp = in_hp[(r * current_w) + (c + 0)];

			out[(r * out_stride) + (c * 2 + 0)] = sEven(lp, hp_l1, hp);
		}

		// Odd, last values
		for (size_t c = (current_w - 1 - ODD_DELAY); c < current_w; c++)
		{
			if (c == (current_w - 1) && ignore_last == 1)
				break;

			const int16_t hp = in_hp[(r * current_w) + (c + 0)];
			const int16_t even = out[(r * out_stride) + (c * 2 + 0)];

			int16_t even_p1;
			if (c < current_w - 1)
				even_p1 = out[(r * out_stride) + (c * 2 + 2)];
			else
				switch (wrap)
				{
				case AKO_WRAP_CLAMP: // falltrough
				case AKO_WRAP_MIRROR: even_p1 = even; break;
				case AKO_WRAP_REPEAT: even_p1 = out[(r * out_stride) + ((c + 0) % (current_w - 1)) * 2]; break;
				case AKO_WRAP_ZERO: even_p1 = 0; break;
				}

			out[(r * out_stride) + (c * 2 + 1)] = sOdd(hp, even, even_p1);
		}
	}
}


void akoCdf53InPlaceishUnliftV(enum akoWrap wrap, size_t current_w, size_t current_h, const int16_t* in_lp,
                               const int16_t* in_hp, int16_t* out_lp, int16_t* out_hp)
{
	// Even, first value
	{
		const size_t r = 0;
		for (size_t c = 0; c < current_w; c++)
		{
			const int16_t lp = in_lp[(r + 0) * current_w + c];
			const int16_t hp = in_hp[(r + 0) * current_w + c];

			int16_t hp_l1;
			switch (wrap)
			{
			case AKO_WRAP_CLAMP: // falltrough
			case AKO_WRAP_MIRROR: hp_l1 = hp; break;
			case AKO_WRAP_REPEAT: hp_l1 = in_hp[(current_h - 1) * current_w + c]; break;
			case AKO_WRAP_ZERO: hp_l1 = 0; break;
			}

			out_lp[(r * current_w) + c] = sEven(lp, hp_l1, hp);
		}
	}

	// Even, remaining values
	for (size_t r = 1; r < current_h; r++)
	{
		for (size_t c = 0; c < current_w; c++)
		{
			const int16_t lp = in_lp[(r + 0) * current_w + c];
			const int16_t hp_l1 = in_hp[(r - 1) * current_w + c];
			const int16_t hp = in_hp[(r + 0) * current_w + c];

			out_lp[(r * current_w) + c] = sEven(lp, hp_l1, hp);
		}
	}

	// Odd, except last
	for (size_t r = 0; r < (current_h - 1); r++)
	{
		for (size_t c = 0; c < current_w; c++)
		{
			const int16_t hp = in_hp[(r + 0) * current_w + c];
			const int16_t even = out_lp[(r + 0) * current_w + c];
			const int16_t even_p1 = out_lp[(r + 1) * current_w + c];

			out_hp[(r * current_w) + c] = sOdd(hp, even, even_p1);
		}
	}

	// Odd, last value
	{
		const size_t r = current_h - 1;
		for (size_t c = 0; c < current_w; c++)
		{
			const int16_t hp = in_hp[(r + 0) * current_w + c];
			const int16_t even = out_lp[(r + 0) * current_w + c];

			int16_t even_p1;
			switch (wrap)
			{
			case AKO_WRAP_CLAMP: // falltrough
			case AKO_WRAP_MIRROR: even_p1 = even; break;
			case AKO_WRAP_REPEAT: even_p1 = out_lp[c]; break;
			case AKO_WRAP_ZERO: even_p1 = 0; break;
			}

			out_hp[(r * current_w) + c] = sOdd(hp, even, even_p1);
		}
	}
}
