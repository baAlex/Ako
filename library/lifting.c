/*

MIT License

Copyright (c) 2021-2022 Alexander Brandt

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


static void sInverseQuantization(int16_t q, size_t w, size_t h, int16_t* inout)
{
	if (q <= 1)
		return;

	for (size_t r = 0; r < h; r++)
	{
		for (size_t c = 0; c < w; c++)
			inout[(r * w) + c] = inout[(r * w) + c] * q;
	}
}


static void sLift2d(enum akoWavelet wavelet, enum akoWrap wrap, size_t in_stride, size_t current_w, size_t current_h,
                    size_t target_w, size_t target_h, int16_t* lp, int16_t* aux)
{
	const size_t fake_last_col = (target_w * 2) - current_w;
	const size_t fake_last_row = (target_h * 2) - current_h;

	if (wavelet == AKO_WAVELET_HAAR)
	{
		akoHaarLiftH(current_h, target_w, fake_last_col, in_stride, lp, aux);
		if (fake_last_row != 0)
			akoHaarLiftH(1, target_w, fake_last_col, 0, lp + in_stride * (current_h - 1),
			             aux + current_h * target_w * 2);

		akoHaarLiftV(target_w * 2, target_h, aux, lp);
	}
	else if (wavelet == AKO_WAVELET_CDF53 || target_w < 8 || target_h < 8)
	{
		akoCdf53LiftH(wrap, current_h, target_w, fake_last_col, in_stride, lp, aux);
		if (fake_last_row != 0)
			akoCdf53LiftH(wrap, 1, target_w, fake_last_col, 0, lp + in_stride * (current_h - 1),
			              aux + current_h * target_w * 2);

		akoCdf53LiftV(wrap, target_w * 2, target_h, aux, lp);
	}
	else
	{
		akoDd137LiftH(wrap, current_h, target_w, fake_last_col, in_stride, lp, aux);
		if (fake_last_row != 0)
			akoDd137LiftH(wrap, 1, target_w, fake_last_col, 0, lp + in_stride * (current_h - 1),
			              aux + current_h * target_w * 2);

		akoDd137LiftV(wrap, target_w * 2, target_h, aux, lp);
	}
}


struct akoUnliftCallbackData
{
	coeff_t* out;
	size_t out_planes_space;
	size_t tile_no;
};

static void s2dUnliftLp(const struct akoSettings* s, size_t ch, size_t tile_w, size_t tile_h, size_t target_w,
                        size_t target_h, coeff_t* input, void* raw_data)
{
	(void)s;
	struct akoUnliftCallbackData* data = raw_data;
	coeff_t* lp = data->out + (tile_w * tile_h + data->out_planes_space) * ch;

	for (size_t i = 0; i < (target_w * target_h); i++) // Just copy it
		lp[i] = input[i];

	// if (data->tile_no == 0 && ch == 0)
	// {
	// 	printf("D\t%zux%zu\n", target_w, target_h);
	// 	for (size_t i = 0; i < (target_w * target_h); i++)
	// 		printf("D\t - Lp: %i\n", lp[i]);
	// }
}

static void s2dUnliftHp(const struct akoSettings* s, size_t ch, size_t tile_w, size_t tile_h,
                        const struct akoLiftHead* head, size_t current_w, size_t current_h, size_t target_w,
                        size_t target_h, coeff_t* aux, coeff_t* hp_c, coeff_t* hp_b, coeff_t* hp_d, void* raw_data)
{
	struct akoUnliftCallbackData* data = raw_data;
	coeff_t* lp = data->out + (tile_w * tile_h + data->out_planes_space) * ch;

	const size_t ignore_last_col = (current_w * 2) - target_w;
	const size_t ignore_last_row = (current_h * 2) - target_h;

	sInverseQuantization(head->quantization, current_w, current_h, hp_b);
	sInverseQuantization(head->quantization, current_w, current_h, hp_c);
	sInverseQuantization(head->quantization, current_w, current_h, hp_d);

	if (s->wavelet == AKO_WAVELET_HAAR)
	{
		akoHaarInPlaceishUnliftV(current_w, current_h, lp, hp_c, aux, hp_c);
		akoHaarInPlaceishUnliftV(current_w, current_h, hp_b, hp_d, hp_b, hp_d);

		akoHaarUnliftH(current_w, current_h, target_w * 2, ignore_last_col, aux, hp_b, lp + 0);
		akoHaarUnliftH(current_w, current_h - ignore_last_row, target_w * 2, ignore_last_col, hp_c, hp_d,
		               lp + target_w);
	}
	else if (s->wavelet == AKO_WAVELET_CDF53 || current_w < 8 || current_h < 8)
	{
		akoCdf53InPlaceishUnliftV(s->wrap, current_w, current_h, lp, hp_c, aux, hp_c);
		akoCdf53InPlaceishUnliftV(s->wrap, current_w, current_h, hp_b, hp_d, hp_b, hp_d);

		akoCdf53UnliftH(s->wrap, current_w, current_h, target_w * 2, ignore_last_col, aux, hp_b, lp + 0);
		akoCdf53UnliftH(s->wrap, current_w, current_h - ignore_last_row, target_w * 2, ignore_last_col, hp_c, hp_d,
		                lp + target_w);
	}
	else
	{
		akoDd137InPlaceishUnliftV(s->wrap, current_w, current_h, lp, hp_c, aux, hp_c);
		akoDd137InPlaceishUnliftV(s->wrap, current_w, current_h, hp_b, hp_d, hp_b, hp_d);

		akoDd137UnliftH(s->wrap, current_w, current_h, target_w * 2, ignore_last_col, aux, hp_b, lp + 0);
		akoDd137UnliftH(s->wrap, current_w, current_h - ignore_last_row, target_w * 2, ignore_last_col, hp_c, hp_d,
		                lp + target_w);
	}

	// if (data->tile_no == 0 && ch == 0)
	// 	printf("D\t%zux%zu <- %zux%zu (%li, %li)\n", target_w, target_h, current_w, current_h, ignore_last_col,
	// 	       ignore_last_row);
}


//


static inline void s2dMemcpy(int16_t q, int16_t g, size_t w, size_t h, size_t in_stride, const int16_t* in,
                             int16_t* out)
{
	if (q < 1)
		q = 1;

	for (size_t r = 0; r < h; r++)
	{
		for (size_t c = 0; c < w; c++)
			out[c] = (in[c] < -g || in[c] > +g) ? (in[c] / q) : 0;

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
			AKO_DEV_PRINTF("E\t%zux%zu -> %zux%zu (%li, %li)\n", current_w, current_h, target_w, target_h,
			               (target_w * 2 - current_w), (target_h * 2 - current_h));
		}

		// Iterate in Vuy order
		for (size_t ch = (channels - 1); ch < channels; ch--) // Yes, underflows
		{
			int16_t q = 0;
			int16_t g = 0;

			if (ch == 0)
			{
				q = akoQuantization(s->quantization, 1, tile_w, tile_h, current_w, current_h);
				g = akoGate(s->gate, 1, tile_w, tile_h, current_w, current_h);
			}
			else
			{
				q = akoQuantization(s->quantization, s->chroma_loss + 1, tile_w, tile_h, current_w, current_h);
				g = akoGate(s->gate, s->chroma_loss + 1, tile_w, tile_h, current_w, current_h);
			}

			// 1. Lift
			int16_t* lp = in + (tile_w * tile_h + planes_space) * ch;

			if (current_w != tile_w)
			{
				// PLACE OF INTEREST, take you screenshot, lovely code tourist!:

				// What follows determines what akoPlanesSpacing() should return, the idea is that:
				// 1. Planes don't overlap when they add one to they dimensions
				// 2. Vertical/horizontal lifts don't overlap either
				// 3. Recycle memory :)

				int16_t* aux = lp + ((target_w * 2) * (target_h * 2)) * 2; // Cache friendly

				// Almost all encoding errors so far happened because of the above pointer

				// And don't worry, this shouldn't happen on the decoder side as it
				// uses in-place lifting (without recycling memory)

				// Here a shame-showcase of past pointer mathematics:
				// lp + ((tile_w + 1) * (tile_h + 1)) / 2;       // Safest, ruins cache locality
				// lp + tile_w * (tile_h / 2 + 1);               // Ditto
				// lp + ((current_w + 1) * (current_h + 1)) * 2; // Cache friendly, but always
				//                                                  accounts for an extra col/row

				sLift2d(s->wavelet, s->wrap, current_w * 2, current_w, current_h, target_w, target_h, lp, aux);

				// END OF PLACE OF INTEREST
			}
			else
			{
				int16_t* aux = output; // First lift is to big to allow us to use the
				                       // same workarea as auxiliary memory. Luckily
				                       // since is the first one, 'output' is empty
				sLift2d(s->wavelet, s->wrap, current_w * 1, current_w, current_h, target_w, target_h, lp, aux);
			}

			// 2. Write coefficients
			out -= (target_w * target_h) * sizeof(int16_t) * 3; // Three highpasses...

			s2dMemcpy(q, g, target_w, target_h, (target_w * 2),
			          lp + target_w * target_h * 2,               //
			          (int16_t*)out + (target_w * target_h) * 0); // C

			s2dMemcpy(q, g, target_w, target_h, (target_w * 2),
			          lp + target_w,                              //
			          (int16_t*)out + (target_w * target_h) * 1); // B

			s2dMemcpy(q, g, target_w, target_h, (target_w * 2),
			          lp + target_w + target_h * (target_w * 2),  //
			          (int16_t*)out + (target_w * target_h) * 2); // D

			// 3. Write lift head
			out -= sizeof(struct akoLiftHead); // One lift head...
			((struct akoLiftHead*)out)->quantization = q;

			// Developers, developers, developers
			if (tile_no == 0)
			{
				if (ch == 0)
					AKO_DEV_PRINTF("E\tLiftHeadCh0 at %zu\n", (size_t)(out - (uint8_t*)output));

				// char filename[64];
				// snprintf(filename, 64, "LiftV%zux%zuCh%zuE.pgm", target_w * 2, target_h * 2, ch);
				// akoSavePgmI16(target_w * 2, target_h * 2, target_w * 2, lp, filename);
			}
		}
	}

	// Lowpasses
	if (tile_no == 0)
		AKO_DEV_PRINTF("D\t%zux%zu\n", target_w, target_h);

	for (size_t ch = (channels - 1); ch < channels; ch--)
	{
		out -= (target_w * target_h) * sizeof(int16_t); // ... And one lowpass

		int16_t* lp = in + (tile_w * tile_h + planes_space) * ch;
		s2dMemcpy(1, 0, target_w, target_h, (target_w * 2), lp, (int16_t*)out); // LP

		// Developers, developers, developers
		if (tile_no == 0 && ch == 0)
		{
			for (size_t i = 0; i < (target_w * target_h); i++)
				AKO_DEV_PRINTF("E\tLPCh0 %i\n", ((int16_t*)out)[i]);
		}
	}
}


void akoUnlift(const struct akoSettings* s, size_t channels, size_t tile_no, size_t tile_w, size_t tile_h,
               size_t out_planes_space, coeff_t* input, coeff_t* out)
{
	struct akoUnliftCallbackData data = {0};
	data.out = out;
	data.out_planes_space = out_planes_space;
	data.tile_no = tile_no;

	akoIterateLifts(s, channels, tile_w, tile_h, input, s2dUnliftLp, s2dUnliftHp, &data);
}
