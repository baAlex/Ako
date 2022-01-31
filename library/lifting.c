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


static void sLift2d(int16_t q, enum akoWavelet wavelet, enum akoWrap wrap, size_t in_stride, size_t current_w,
                    size_t current_h, size_t target_w, size_t target_h, int16_t* lp, int16_t* aux)
{
	const size_t fake_last_col = (target_w * 2) - current_w;
	const size_t fake_last_row = (target_h * 2) - current_h;

	if (wavelet == AKO_WAVELET_HAAR)
	{
		akoHaarLiftH(q, current_h, target_w, fake_last_col, in_stride, lp, aux);
		if (fake_last_row != 0)
			akoHaarLiftH(q, 1, target_w, fake_last_col, 0, lp + in_stride * (current_h - 1),
			             aux + current_h * target_w * 2);

		akoHaarLiftV(q, target_w * 2, target_h, aux, lp);
	}
	else // if (wavelet == AKO_WAVELET_CDF53)
	{
		akoCdf53LiftH(wrap, q, current_h, target_w, fake_last_col, in_stride, lp, aux);
		if (fake_last_row != 0)
			akoCdf53LiftH(wrap, q, 1, target_w, fake_last_col, 0, lp + in_stride * (current_h - 1),
			              aux + current_h * target_w * 2);

		akoCdf53LiftV(wrap, q, target_w * 2, target_h, aux, lp);
	}
}


static void sUnlift2d(int16_t q, enum akoWavelet wavelet, enum akoWrap wrap, size_t current_w, size_t current_h,
                      size_t target_w, size_t target_h, int16_t* lp, int16_t* hps, int16_t* aux)
{
	const size_t ignore_last_col = (current_w * 2) - target_w;
	const size_t ignore_last_row = (current_h * 2) - target_h;

	int16_t* hp_c = hps + (current_w * current_h) * 0;
	int16_t* hp_b = hps + (current_w * current_h) * 1;
	int16_t* hp_d = hps + (current_w * current_h) * 2;

	if (q <= 0)
		q = 1;

	if (wavelet == AKO_WAVELET_HAAR)
	{
		akoHaarInPlaceishUnliftV(q, current_w, current_h, lp, hp_c, aux, hp_c);
		akoHaarInPlaceishUnliftV(q, current_w, current_h, hp_b, hp_d, hp_b, hp_d);

		akoHaarUnliftH(q, current_w, current_h, target_w * 2, ignore_last_col, aux, hp_b, lp + 0);
		akoHaarUnliftH(q, current_w, current_h - ignore_last_row, target_w * 2, ignore_last_col, hp_c, hp_d,
		               lp + target_w);
	}
	else // if (wavelet == AKO_WAVELET_CDF53)
	{
		akoCdf53InPlaceishUnliftV(wrap, q, current_w, current_h, lp, hp_c, aux, hp_c);
		akoCdf53InPlaceishUnliftV(wrap, q, current_w, current_h, hp_b, hp_d, hp_b, hp_d);

		akoCdf53UnliftH(wrap, q, current_w, current_h, target_w * 2, ignore_last_col, aux, hp_b, lp + 0);
		akoCdf53UnliftH(wrap, q, current_w, current_h - ignore_last_row, target_w * 2, ignore_last_col, hp_c, hp_d,
		                lp + target_w);
	}
}


//


static inline void s2dMemcpy(int16_t g, size_t w, size_t h, size_t in_stride, const int16_t* in, int16_t* out)
{
	for (size_t r = 0; r < h; r++)
	{
		for (size_t c = 0; c < w; c++)
			out[c] = (in[c] < -g || in[c] > +g) ? in[c] : 0;

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

		const int16_t q = akoQuantization(s->quantization, tile_w, tile_h, current_w, current_h);
		const int16_t g = akoGate(s->gate, tile_w, tile_h, current_w, current_h);

		// Developers, developers, developers
		if (tile_no == 0)
		{
			AKO_DEV_PRINTF("E\t%zux%zu -> %zux%zu (%li, %li)\n", current_w, current_h, target_w, target_h,
			               (target_w * 2 - current_w), (target_h * 2 - current_h));
		}

		// Iterate in Vuy order
		for (size_t ch = (channels - 1); ch < channels; ch--) // Yes, underflows
		{
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

				sLift2d(q, s->wavelet, s->wrap, current_w * 2, current_w, current_h, target_w, target_h, lp, aux);

				// END OF PLACE OF INTEREST
			}
			else
			{
				int16_t* aux = output; // First lift is to big to allow us to use the
				                       // same workarea as auxiliary memory. Luckily
				                       // since is the first one, 'output' is empty
				sLift2d(q, s->wavelet, s->wrap, current_w * 1, current_w, current_h, target_w, target_h, lp, aux);
			}

			// 2. Write coefficients
			out -= (target_w * target_h) * sizeof(int16_t) * 3; // Three highpasses...

			s2dMemcpy(g, target_w, target_h, (target_w * 2),
			          lp + target_w * target_h * 2,               //
			          (int16_t*)out + (target_w * target_h) * 0); // C

			s2dMemcpy(g, target_w, target_h, (target_w * 2),
			          lp + target_w,                              //
			          (int16_t*)out + (target_w * target_h) * 1); // B

			s2dMemcpy(g, target_w, target_h, (target_w * 2),
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
		s2dMemcpy(0, target_w, target_h, (target_w * 2), lp, (int16_t*)out); // LP

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

		for (size_t i = 0; i < (target_w * target_h); i++)
			lp[i] = ((int16_t*)in)[i];

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

			sUnlift2d(q, s->wavelet, s->wrap, current_w, current_h, target_w, target_h, lp, hps, input);

			in += (current_w * current_h) * sizeof(int16_t) * 3; // ... And three highpasses
		}
	}
}
