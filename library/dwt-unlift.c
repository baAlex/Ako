/*-----------------------------

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

-------------------------------

 [dwt-unlift.c]
 - Alexander Brandt 2021
-----------------------------*/

#include <assert.h>
#include <string.h>

#include "ako.h"
#include "dwt.h"


static inline void sUnlift1d(size_t len, const int16_t* in, int16_t* out)
{
	// Even
	// CDF53, 97DD: even[i] = lp[i] - (hp[i] + hp[i - 1]) / 4
	// Haar:        even[i] = lp[i] - hp[i]
	for (size_t i = 0; i < len; i++)
	{
#if (AKO_WAVELET != 1)
		// CDF53, 97DD
		if (i > 0)
			out[(i * 2)] = in[i] - ((in[len + i] + in[len + i - 1] + 2) >> 2);
		else
			out[(i * 2)] = in[i] - ((in[len + i] + in[len + i] + 2) >> 2); // Fake first value
#else
		// Haar
		out[(i * 2)] = in[i] - in[len + i];
#endif
	}

	// Odd
	// Haar:  odd[i] = lp[i] + hp[i]
	// CDF53: odd[i] = hp[i] + (even[i] + even[i + 1]) / 2
	// 97DD:  odd[i] = hp[i] + (-even[i - 1] + 9 * even[i] + 9 * even[i + 1] - even[i + 2] + 8) / 16)

#if (AKO_WAVELET == 1)
	// Haar
	for (size_t i = 0; i < len; i++)
		out[(i * 2) + 1] = in[i] + in[len + i];
#endif

#if (AKO_WAVELET == 2)
	// CDF53
	for (size_t i = 0; i < len; i++)
	{
		if (i < len - 2)
			out[(i * 2) + 1] = in[len + i] + ((out[(i * 2)] + out[(i * 2) + 2] + 1) >> 1);
		else
			out[(i * 2) + 1] = in[len + i] + ((out[(i * 2)] + out[(i * 2)] + 1) >> 1); // Fake last value
	}
#endif

#if (AKO_WAVELET == 3)
	// 97DD
	if (len > 4) // Needs 4 samples
	{
		for (size_t i = 0; i < len; i++)
		{
			int16_t even_i = out[(i * 2)];
			int16_t even_il1 = (i >= 1) ? out[(i * 2) - 2] : even_i;
			int16_t even_ip1 = (i <= len - 2) ? out[(i * 2) + 2] : even_i;
			int16_t even_ip2 = (i <= len - 4) ? out[(i * 2) + 4] : even_ip1;

			out[(i * 2) + 1] = in[len + i] + ((-even_il1 + 9 * even_i + 9 * even_ip1 - even_ip2 + 8) >> 4);
		}
	}
	else // Fallback to CDF53
	{
		for (size_t i = 0; i < len; i++)
		{
			if (i < len - 2)
				out[(i * 2) + 1] = in[len + i] + ((out[(i * 2)] + out[(i * 2) + 2] + 1) >> 1);
			else
				out[(i * 2) + 1] = in[len + i] + ((out[(i * 2)] + out[(i * 2)] + 1) >> 1); // Fake last value
		}
	}
#endif
}


void DwtUnpackUnliftImage(size_t dimension, size_t channels, int16_t* aux_buffer, const int16_t* in, int16_t* out)
{
	// I'm deviating from papers nomenclature, so:

	// | ---- | ---- |      | ---- | ---- |
	// | LL   | LH   |      | LP   | B    |
	// | ---- | ---- |  ->  | ---- | ---- |
	// | HL   | HH   |      | C    | D    |
	// | ---- | ---- |      | ---- | ---- |

	// - LP as lowpass
	// - B, C, D as 'highpasses' or HP

	// --------

	// The encoder saves in this order:
	// [LP0 (2x2 px)], [C1, B1, D1], [C2, B2, D2], [C3, B3, D3]... etc.

	// Such bizarre thing because I'm resolving columns first.
	// LP with C, then B with D. Finally rows.

	// Even better, columns in C, B and D are packed as rows. Left
	// to right, ready to be consumed in a linear way with memcpy().

	// And finally, columns in B and D are bundled (or interleaved):
	// [Column 0B, Column 0D], [Column 1B, Column 1D]... etc.

	// --------

#define ITS_1993 1 // Dude wake up it's 1993!, let's unroll loops!.

	// Being serious, not only I'm unrolling loops, but also removing
	// multiplications, doing 64 bits reads and interleaving/ordering
	// operations that work on areas close each other in RAM... this
	// help the cache usage?, right?.

	// Nothing more than what a modern compiler already does.
	// And incredible, this kind of 90s optimizations saves 7 ms
	// on my old Celeron. So yeahh...

	// --------

	// Copy initial lowpass (2x2 px)
	for (size_t ch = 0; ch < channels; ch++)
		memcpy(out + dimension * dimension * ch, in + 2 * 2 * ch, sizeof(int16_t) * 2 * 2);

	// Unlift steps, aka: apply highpasses
	const int16_t* hp = in + (2 * 2 * channels); // HP starts after the initial 2x2 px LP

	for (size_t current = 2; current < dimension; current = current * 2)
	{
		const size_t current_x2 = current * 2; // Doesn't help the compiler, is just for legibility
		int16_t* lp = out;

		for (size_t ch = 0; ch < channels; ch++)
		{
			// Columns
			for (size_t col = 0; col < current; col++)
			{
#if (ITS_1993 == 1)
				int16_t* temp = lp + col;
				for (size_t i = 0; i < current; i = i + 2) // LP is a column
				{
					aux_buffer[i + 0] = *(temp);
					aux_buffer[i + 1] = *(temp + current);
					temp = temp + current_x2;
				}
#else
				int16_t* temp = lp + col;
				for (size_t i = 0; i < current; i++)
					aux_buffer[i] = temp[i * current];
#endif

				memcpy(aux_buffer + current, (hp + current * col),
				       sizeof(int16_t) * current); // Encoder packed C as a row, no weird loops here

				// Unlift LP and C (first halve)
				// Input first half of the buffer, output in the last half
				sUnlift1d(current, aux_buffer, aux_buffer + current_x2);

				// Unlift B and D (second halve)
				// - '(hp + current * current)' is C, we skip it
				// - '(current_x2 * col)' is the counterpart of '(hp + current * col)' above in C
				//   except that B and D are bundled together by the encoder, thus the x2
				sUnlift1d(current, (hp + current * current) + (current_x2 * col), aux_buffer);

				// Write unlift results back to LP
#if (ITS_1993 == 1)
				temp = lp + col;
				for (size_t i = 0; i < current_x2; i = i + 4)
				{
					// Hopefully the compiler will notice that the intention of my
					// bitshifts is to translate those into 'MOV [MEM], REG16' of a REG64
					const uint64_t data = *(uint64_t*)((uint16_t*)aux_buffer + i + current_x2);
					const uint64_t data2 = *(uint64_t*)((uint16_t*)aux_buffer + i);

					*(temp) = (int16_t)((data >> 0) & 0xFFFF);
					*(temp + current) = (int16_t)((data2 >> 0) & 0xFFFF);

					*(temp + current_x2) = (int16_t)((data >> 16) & 0xFFFF);
					*(temp + current_x2 + current) = (int16_t)((data2 >> 16) & 0xFFFF);

					*(temp + current_x2 * 2) = (int16_t)((data >> 32) & 0xFFFF);
					*(temp + current_x2 * 2 + current) = (int16_t)((data2 >> 32) & 0xFFFF);

					*(temp + current_x2 * 3) = (int16_t)((data >> 48));
					*(temp + current_x2 * 3 + current) = (int16_t)((data2 >> 48));

					temp = temp + (current_x2 << 2);
				}
#else
				temp = lp + col;
				for (size_t i = 0; i < current_x2; i++)
					temp[i * current_x2] = aux_buffer[current_x2 + i];

				temp = lp + current + col;
				for (size_t i = 0; i < current_x2; i++)
					temp[i * current_x2] = aux_buffer[i];
#endif
			}

			// Rows, operates just in LP
			for (size_t row = 0; row < current_x2; row++)
			{
				memcpy(aux_buffer, lp + row * current_x2, sizeof(int16_t) * current_x2);
				sUnlift1d(current, aux_buffer, lp + row * current_x2);
			}

			// Done with this channel
			hp = hp + current * current * 3; // Move from current B, C and D
			lp = lp + dimension * dimension;
		}
	}
}
