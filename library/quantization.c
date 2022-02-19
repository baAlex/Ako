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


#define POWER 1.0F
#define SCALE 256.0F


static float sExponential(float factor, float tile_w, float tile_h, float current_w, float current_h)
{
	(void)tile_w;
	(void)tile_h;

	// const float area0 = __builtin_sqrtf(tile_w * tile_h);
	const float area = __builtin_sqrtf(current_w * current_h);

	// const float total_lifts = __builtin_log2f(area0) - 1.0F;
	const float current_lift = __builtin_log2f(area) - 1.0F;

	const float exp = __builtin_powf(2.0F, (current_lift - 1.0F));

	const float final = exp * (factor / (__builtin_powf(2.0F, POWER) * SCALE));
	// final = __builtin_roundf(__builtin_powf(final, POWER));

	// AKO_DEV_PRINTF("\t%.2f / %.2f -> %.2f -> %.2f\n", current_lift, total_lifts, exp, final);
	return final;
}


int16_t akoGate(int factor, size_t tile_w, size_t tile_h, size_t current_w, size_t current_h)
{
	if (factor <= 0)
		return 0;

	float g = sExponential((float)factor, (float)tile_w, (float)tile_h, (float)current_w, (float)current_h);

	if (g < 0.0F)
		g = 0.0F;
	if (g > 32765.0F)
		g = 32765.0F;

	return (int16_t)g;
}


int16_t akoQuantization(int factor, size_t tile_w, size_t tile_h, size_t current_w, size_t current_h)
{
	if (factor <= 0)
		return 1;

	float q = sExponential((float)factor, (float)tile_w, (float)tile_h, (float)current_w, (float)current_h);

	if (q < 1.0F)
		q = 1.0F;
	if (q > 32765.0F)
		q = 32765.0F;

	return (int16_t)q;
}
