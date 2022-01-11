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

		// Iterate in Vuy order
		for (size_t ch = (channels - 1); ch < channels; ch--) // Yes, underflows
		{
			// 1. Lift
			int16_t* lp = in + ((tile_w * tile_h) + planes_space) * ch;

			if (current_w == tile_w && current_h == tile_h) {}
			else
			{
			}

			// 2. Write coefficients
			out -= (target_w * target_h) * sizeof(int16_t) * 3; // Three highpasses...

			// 3. Write lift head
			out -= sizeof(struct akoLiftHead); // One lift head...
			                                   // ((struct akoLiftHead*)out)->quantization = 0;
		}

		// Developers, developers, developers
		if (tile_no == 0)
		{
			AKO_DEV_PRINTF("E\tLifHeadCh0 at %zu\n", (size_t)(out - (uint8_t*)output));
			AKO_DEV_PRINTF("E\t%zux%zu -> %zux%zu\n", current_w, current_h, target_w, target_h);
		}
	}

	// Lowpasses
	if (tile_no == 0)
		AKO_DEV_PRINTF("D\t%zux%zu\n", target_w, target_h);

	for (size_t ch = (channels - 1); ch < channels; ch--)
	{
		out -= (target_w * target_h) * sizeof(int16_t); // ... And one lowpass

		// Copy as is
		int16_t* lp = in + ((tile_w * tile_h) + planes_space) * ch;
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
		int16_t* lp = out + ((tile_w * tile_h) + planes_space) * ch;

		// Copy as is
		in += (target_w * target_h) * sizeof(int16_t); // One lowpass...
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
			AKO_DEV_PRINTF("D\t%zux%zu <- %zux%zu\n", target_w, target_h, current_w, current_h);
		}

		// Interate in Yuv order
		for (size_t ch = 0; ch < channels; ch++)
		{
			// 1. Read lift head
			// const int16_t q = ((struct akoLiftHead*)in)->quantization;
			in += sizeof(struct akoLiftHead); // One lift head...

			// 2. Unlift
			int16_t* lp = out + ((tile_w * tile_h) + planes_space) * ch;

			in += (current_w * current_h) * sizeof(int16_t) * 3; // ... And three highpasses...
		}
	}
}
