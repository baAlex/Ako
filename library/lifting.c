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


static void s2dLift(enum akoWavelet wavelet, enum akoWrap wrap, size_t in_stride, size_t current_w, size_t current_h,
                    size_t target_w, size_t target_h, int16_t* lp, int16_t* aux)
{
	// Horizontal lift
	{
		const int plus_one_col = (current_w % 2 == 0) ? 0 : 1;

		for (size_t r = 0; r < current_h; r++) // TODO, move loop inside akoHaarLift()
			akoHaarLiftH(target_w, plus_one_col, lp + (r * in_stride), aux + (r * target_w * 2));

		if (current_h % 2 != 0)
			akoHaarLiftH(target_w, plus_one_col, lp + ((current_h - 1) * in_stride), aux + (current_h * target_w * 2));
	}

	// Vertical lift
	{
		for (size_t c = 0; c < target_w * 2; c++) // TODO, move loop inside akoHaarLift()
			akoHaarLiftV(target_h, target_w * 2, aux + c, lp + c);
	}
}


static void s2dUnlift(enum akoWavelet wavelet, enum akoWrap wrap, size_t current_w, size_t current_h, size_t target_w,
                      size_t target_h, int16_t* lp, int16_t* hps, int16_t* aux)
{
	int16_t* hp_c = hps + (current_w * current_h) * 0;
	int16_t* hp_b = hps + (current_w * current_h) * 1;
	int16_t* hp_d = hps + (current_w * current_h) * 2;

	// Vertical unlift
	{
		for (size_t c = 0; c < current_w; c++)
			akoHaarInPlaceishUnliftV(current_h, current_w, lp + c, hp_c + c, aux + c, hp_c + c);

		for (size_t c = 0; c < current_w; c++)
			akoHaarInPlaceishUnliftV(current_h, current_w, hp_b + c, hp_d + c, hp_b + c, hp_d + c);
	}

	// Horizontal unlift
	{
		const int less_one_col = (current_w * 2) - target_w;
		const size_t less_one_row = (current_h * 2) - target_h;

		for (size_t r = 0; r < current_h; r++)
			akoHaarUnliftH(current_w, less_one_col, aux + (r * current_w), hp_b + (r * current_w),
			               lp + ((r * 2) * target_w));

		for (size_t r = 0; r < current_h - less_one_row; r++)
			akoHaarUnliftH(current_w, less_one_col, hp_c + (r * current_w), hp_d + (r * current_w),
			               lp + ((r * 2 + 1) * target_w));
	}
}


//


static inline void s2dMemcpy(size_t w, size_t h, size_t in_stride, const int16_t* in, int16_t* out)
{
	for (size_t r = 0; r < h; r++)
	{
		for (size_t c = 0; c < w; c++)
			out[c] = in[c];

		in += in_stride;
		out += w;
	}
}

void akoLift(size_t tile_no, const struct akoSettings* s, size_t channels, size_t tile_w, size_t tile_h,
             size_t planes_space, int16_t* in, int16_t* output)
{
	// Protip: everything here operates in reverse

	size_t target_w = tile_w;
	size_t target_h = tile_h;

	uint8_t* out = (uint8_t*)output + akoTileDataSize(tile_w, tile_h) * channels; // Output end

	// Highpasses
	while (target_w > 2 && target_h > 2)
	{
		const size_t current_w = target_w;
		const size_t current_h = target_h;
		target_w = akoDividePlusOneRule(target_w);
		target_h = akoDividePlusOneRule(target_h);

		// Developers, developers, developers
		if (tile_no == 0)
		{
			if (current_w != tile_w)
				AKO_DEV_PRINTF("E\tLiftHeadCh0 at %zu\n", (size_t)(out - (uint8_t*)output));

			AKO_DEV_PRINTF("E\t%zux%zu -> %zux%zu (%li, %li)\n", current_w, current_h, target_w, target_h,
			               (target_w * 2 - current_w), (target_h * 2 - current_h));
		}

		// Iterate in Vuy order
		for (size_t ch = (channels - 1); ch < channels; ch--) // Yes, underflows
		{
			// 1. Lift
			int16_t* lp = in + (tile_w * tile_h + planes_space) * ch;

			if (current_w == tile_w)
			{
				int16_t* aux = output;
				s2dLift(s->wavelet, s->wrap, current_w * 1, current_w, current_h, target_w, target_h, lp, aux);
			}
			else
			{
				int16_t* aux = lp + tile_w * (tile_h / 2 + 1);
				s2dLift(s->wavelet, s->wrap, current_w * 2, current_w, current_h, target_w, target_h, lp, aux);
			}

			// 2. Write coefficients
			out -= (target_w * target_h) * sizeof(int16_t) * 3; // Three highpasses...

			s2dMemcpy(target_w, target_h, (target_w * 2),
			          lp + target_w * target_h * 2,               //
			          (int16_t*)out + (target_w * target_h) * 0); // C

			s2dMemcpy(target_w, target_h, (target_w * 2),
			          lp + target_w,                              //
			          (int16_t*)out + (target_w * target_h) * 1); // B

			s2dMemcpy(target_w, target_h, (target_w * 2),
			          lp + target_w + target_h * (target_w * 2),  //
			          (int16_t*)out + (target_w * target_h) * 2); // D

			// 3. Write lift head
			out -= sizeof(struct akoLiftHead); // One lift head...
			((struct akoLiftHead*)out)->quantization = 1;

			// Developers, developers, developers
			// if (tile_no == 0)
			// {
			// 	char filename[64];
			// 	snprintf(filename, 64, "LiftV%zux%zuCh%zuE.pgm", target_w * 2, target_h * 2, ch);
			// 	akoSavePgmI16(target_w * 2, target_h * 2, target_w * 2, lp, filename);
			// }
		}
	}

	// Lowpasses
	if (tile_no == 0)
		AKO_DEV_PRINTF("D\t%zux%zu\n", target_w, target_h);

	for (size_t ch = (channels - 1); ch < channels; ch--)
	{
		out -= (target_w * target_h) * sizeof(int16_t); // ... And one lowpass

		int16_t* lp = in + (tile_w * tile_h + planes_space) * ch;
		s2dMemcpy(target_w, target_h, (target_w * 2), lp, (int16_t*)out); // LP

		// Developers, developers, developers
		if (tile_no == 0 && ch == 0)
		{
			for (size_t i = 0; i < (target_w * target_h); i++)
				AKO_DEV_PRINTF("E\tLPCh0 %i\n", ((int16_t*)out)[i]);
		}
	}
}


static void sLiftTargetDimensions(size_t current_w, size_t current_h, size_t tile_w, size_t tile_h, size_t* out_w,
                                  size_t* out_h)
{
	size_t prev_tile_w = tile_w;
	size_t prev_tile_h = tile_h;

	while (tile_w > current_w && tile_h > current_h)
	{
		prev_tile_w = tile_w;
		prev_tile_h = tile_h;

		if (tile_w <= 2 || tile_h <= 2)
			break;

		tile_w = akoDividePlusOneRule(tile_w);
		tile_h = akoDividePlusOneRule(tile_h);
	}

	*out_w = prev_tile_w;
	*out_h = prev_tile_h;
}

void akoUnlift(size_t tile_no, const struct akoSettings* s, size_t channels, size_t tile_w, size_t tile_h,
               size_t planes_space, int16_t* input, int16_t* out)
{
	// Protip: this function is the inverse of Lift()

	size_t target_w = 0;
	size_t target_h = 0;
	sLiftTargetDimensions(0, 0, tile_w, tile_h, &target_w, &target_h);

	uint8_t* in = (uint8_t*)input;

	// Lowpasses
	if (tile_no == 0)
		AKO_DEV_PRINTF("D\t%zux%zu\n", target_w, target_h);

	for (size_t ch = 0; ch < channels; ch++)
	{
		int16_t* lp = out + (tile_w * tile_h + planes_space) * ch;

		memcpy(lp, in, (target_w * target_h) * sizeof(int16_t));
		in += (target_w * target_h) * sizeof(int16_t); // One lowpass...

		// Developers, developers, developers
		if (tile_no == 0 && ch == 0)
		{
			for (size_t i = 0; i < (target_w * target_h); i++)
				AKO_DEV_PRINTF("D\tLPCh0 %i\n", lp[i]);
		}
	}

	// Highpasses
	while (target_w < tile_w && target_h < tile_h)
	{
		const size_t current_w = target_w;
		const size_t current_h = target_h;
		sLiftTargetDimensions(target_w, target_h, tile_w, tile_h, &target_w, &target_h);

		// Developers, developers, developers
		if (tile_no == 0)
		{
			AKO_DEV_PRINTF("D\tLiftHeadCh0 at %zu\n", (size_t)(in - (uint8_t*)input));
			AKO_DEV_PRINTF("D\t%zux%zu <- %zux%zu (%li, %li)\n", target_w, target_h, current_w, current_h,
			               (current_w * 2 - target_w), (current_h * 2 - target_h));
		}

		// Interate in Yuv order
		for (size_t ch = 0; ch < channels; ch++)
		{
			// 1. Read lift head
			const int16_t q = ((struct akoLiftHead*)in)->quantization;
			in += sizeof(struct akoLiftHead); // One lift head...

			// 2. Unlift
			int16_t* lp = out + (tile_w * tile_h + planes_space) * ch;
			int16_t* hps = (int16_t*)in;

			s2dUnlift(s->wavelet, s->wrap, current_w, current_h, target_w, target_h, lp, hps, input);

			in += (current_w * current_h) * sizeof(int16_t) * 3; // ... And three highpasses
		}
	}
}
