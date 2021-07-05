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
#include "misc.h"


extern void DevPrintf(const char* format, ...);
extern void DevSaveGrayPgm(size_t width, size_t height, size_t in_pitch, const int16_t* data,
                           const char* filename_format, ...);


static inline void sUnlift1d(size_t len, const int16_t* lp, const int16_t* hp, int16_t* out)
{
	// Even
	// CDF53, 97DD: even[i] = lp[i] - (hp[i] + hp[i - 1]) / 4
	// Haar:        even[i] = lp[i] - hp[i]
	for (size_t i = 0; i < len; i++)
	{
#if (AKO_WAVELET != 1)
		// CDF53, 97DD
		if (i > 0)
			out[(i * 2)] = lp[i] - ((hp[i] + hp[i - 1]) / 4);
		else
			out[(i * 2)] = lp[i] - ((hp[i] + hp[i]) / 4); // Fake first value
#else
		// Haar
		out[(i * 2)] = lp[i] - hp[i];
#endif
	}

	// Odd
	// Haar:  odd[i] = lp[i] + hp[i]
	// CDF53: odd[i] = hp[i] + (even[i] + even[i + 1]) / 2
	// 97DD:  odd[i] = hp[i] + (-(even[i - 1] + even[i + 2]) + 9 * (even[i] + even[i + 1])) / 16

#if (AKO_WAVELET == 1)
	// Haar
	for (size_t i = 0; i < len; i++)
		out[(i * 2) + 1] = lp[i] + hp[i];
#endif

#if (AKO_WAVELET == 2)
	// CDF53
	for (size_t i = 0; i < len; i++)
	{
		if (i < len - 2)
			out[(i * 2) + 1] = hp[i] + ((out[(i * 2)] + out[(i * 2) + 2]) / 2);
		else
			out[(i * 2) + 1] = hp[i] + ((out[(i * 2)] + out[(i * 2)]) / 2); // Fake last value
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

			out[(i * 2) + 1] = hp[i] + (-(even_il1 + even_ip2) + 9 * (even_i + even_ip1)) / 16;
		}
	}
	else // Fallback to CDF53
	{
		for (size_t i = 0; i < len; i++)
		{
			if (i < len - 2)
				out[(i * 2) + 1] = hp[i] + ((out[(i * 2)] + out[(i * 2) + 2]) / 2);
			else
				out[(i * 2) + 1] = hp[i] + ((out[(i * 2)] + out[(i * 2)]) / 2); // Fake last value
		}
	}
#endif
}


static inline size_t sMax(size_t a, size_t b)
{
	return (a > b) ? a : b;
}


static inline void sUnlift2d(size_t current_w, size_t current_h, size_t target_w, size_t target_h, size_t lp_pitch,
                             int16_t* aux, int16_t* lp, int16_t* hp)
{
	(void)target_w;
	int16_t* b = aux + (sMax(current_w, current_h) * 2) * 4;

	// Columns
	// Left halve, LP and C
	for (size_t col = 0; col < current_w; col++)
	{
		int16_t* temp = aux;
		for (size_t row = 0; row < current_h; row++)
		{
			*temp = lp[lp_pitch * row + col];
			temp = temp + 1;
		}

		sUnlift1d(current_h, aux, hp, aux + current_h); // Hp == C
		hp = hp + current_h;

		temp = aux + current_h;
		for (size_t row = 0; row < target_h; row++)
		{
			b[current_w * row + col] = *temp;
			temp = temp + 1;
		}
	}

	// Columns
	// Right halve, B and D
	for (size_t col = 0; col < current_w; col++)
	{
		int16_t* temp = aux;

		sUnlift1d(current_h, hp, hp + (current_w * current_h), temp); // Hp == B
		hp = hp + current_h;

		for (size_t row = 0; row < target_h; row++)
		{
			lp[current_w * 2 * row + col] = *temp;
			temp = temp + 1;
		}
	}

	// Rows
	for (size_t row = 0; row < target_h; row++)
	{
		memcpy(aux, lp + (current_w * 2) * row, sizeof(int16_t) * current_w);
		sUnlift1d(current_w, b + current_w * row, aux, lp + (current_w * 2) * row);
	}
}


static inline void sCurrentSize(size_t total_lifts, size_t lift, size_t final_w, size_t final_h, size_t* out_current_w,
                                size_t* out_current_h, size_t* out_target_w, size_t* out_target_h)
{
	*out_target_w = final_w;
	*out_target_h = final_h;

	for (size_t i = 1; i < (total_lifts - lift); i++)
	{
		*out_target_w = (*out_target_w % 2 == 0) ? (*out_target_w / 2) : ((*out_target_w + 1) / 2);
		*out_target_h = (*out_target_h % 2 == 0) ? (*out_target_h / 2) : ((*out_target_h + 1) / 2);
	}

	*out_current_w = (*out_target_w % 2 == 0) ? (*out_target_w / 2) : ((*out_target_w + 1) / 2);
	*out_current_h = (*out_target_h % 2 == 0) ? (*out_target_h / 2) : ((*out_target_h + 1) / 2);
}


void InverseDwtTransform(size_t tile_w, size_t tile_h, size_t channels, size_t planes_space, int16_t* aux_memory,
                         int16_t* in, int16_t* out)
{
#if (AKO_WAVELET == 0)
	for (size_t ch = 0; ch < channels; ch++)
	{
		int16_t* temp = out + ((tile_w * tile_h) + planes_space) * ch;
		memcpy(temp, in, sizeof(int16_t) * tile_w * tile_h);
		in = in + tile_w * tile_h;
	}
	return;
#endif

	const size_t total_lifts = TileTotalLifts(tile_w, tile_h);

	size_t current_w = 0;
	size_t current_h = 0;
	size_t target_w = 0;
	size_t target_h = 0;

	int16_t* input_cursor = in;
	size_t lp_pitch = 0;

	// Lowpasses (one per-channel)
	for (size_t ch = 0; ch < channels; ch++)
	{
		sCurrentSize(total_lifts, 0, tile_w, tile_h, &current_w, &current_h, &target_w, &target_h);
		int16_t* lp = out + ((tile_w * tile_h) + planes_space) * ch;

		for (size_t r = 0; r < current_h; r++)
		{
			memcpy(lp + (current_w * r), input_cursor, sizeof(int16_t) * current_w);
			input_cursor = input_cursor + current_w; // One lowpass...
		}
	}

	// Highpasses
	for (size_t unlift = 0; unlift < total_lifts; unlift++)
	{
		sCurrentSize(total_lifts, unlift, tile_w, tile_h, &current_w, &current_h, &target_w, &target_h);

		for (size_t ch = 0; ch < channels; ch++)
		{
			// Unlift
			int16_t* lp = out + ((tile_w * tile_h) + planes_space) * ch;

			lp_pitch = ((current_w % 2) == 0 || unlift == 0) ? current_w : current_w + 1;

			sUnlift2d(current_w, current_h, target_w, target_h, lp_pitch, aux_memory, lp, input_cursor);
			input_cursor = input_cursor + (current_w * current_h) * 3; // ...And three highpasses

			// Developers, developers, developers
			DevSaveGrayPgm(current_w * 2, current_h * 2, current_w * 2, lp, "/tmp/unlift-ch%zu-%zu.pgm", ch, unlift);

			// if (ch == 0)
			//	DevPrintf("###\t - Unlift %zu, %zux%zu -> %zux%zu px\n", unlift, current_w, current_h, target_w,
			//	          target_h);
		}
	}
}
