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


void akoHaarLiftH(size_t half_len, int fake_last, const int16_t* in, int16_t* out)
{
	// Iterate, omit last value if indicated
	for (size_t i = 0; i < (half_len - (size_t)fake_last); i++)
	{
		const int16_t even = in[(i << 1) + 0];
		const int16_t odd = in[(i << 1) + 1];

		out[i] = even;                  // LP
		out[half_len + i] = odd - even; // HP
	}

	// Our caller indicated that we need to fake the last value
	if (fake_last != 0)
	{
		const size_t i = (half_len - 1);

		const int16_t even = in[(i << 1) + 0];
		const int16_t odd = in[(i << 1) + 0]; // <- Here

		out[i] = even;                  // LP
		out[half_len + i] = odd - even; // HP
	}
}


void akoHaarLiftV(size_t target_w, size_t current_h, const int16_t* in, int16_t* out)
{
	for (size_t r = 0; r < current_h; r++)
	{
		for (size_t c = 0; c < target_w; c++)
		{
			const int16_t even = in[c + target_w * (r * 2 + 0)];
			const int16_t odd = in[c + target_w * (r * 2 + 1)];

			out[c + target_w * r] = even;                                // LP
			out[c + target_w * (current_h + r)] = (int16_t)(odd - even); // HP
		}
	}
}


void akoHaarUnliftH(size_t half_len, int ignore_last, const int16_t* in_lp, const int16_t* in_hp, int16_t* out)
{
	for (size_t i = 0; i < (half_len - (size_t)ignore_last); i++)
	{
		const int16_t lp = in_lp[i];
		const int16_t hp = in_hp[i];

		out[(i << 1) + 0] = lp;      // Even
		out[(i << 1) + 1] = lp + hp; // Odd
	}

	if (ignore_last != 0)
	{
		const size_t i = (half_len - 1);
		const int16_t lp = in_lp[i];

		out[(i << 1) + 0] = lp; // Just even
	}
}


void akoHaarInPlaceishUnliftV(size_t current_w, size_t current_h, const int16_t* lowpass, const int16_t* highpass,
                              int16_t* out_lowpass, int16_t* out_highpass)
{
	for (size_t r = 0; r < current_h; r++)
	{
		for (size_t c = 0; c < current_w; c++)
		{
			const int16_t lp0 = lowpass[c + current_w * r];
			const int16_t hp0 = highpass[c + current_w * r];

			out_lowpass[c + current_w * r] = lp0;                   // Even
			out_highpass[c + current_w * r] = (int16_t)(lp0 + hp0); // Odd
		}
	}
}
