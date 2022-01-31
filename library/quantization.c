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


// To degrade high frequencies the most (a magic number)
#define GATE_POWER 4.0F
#define GATE_FACTOR_SCALE 2.0F

// Quantization share the same logic:
#define QUANTIZATION_POWER 4.0F
#define QUANTIZATION_FACTOR_SCALE 4.0F


static inline float sExponential(float power, size_t tile_w, size_t tile_h, size_t current_w, size_t current_h)
{
	const float area0 = __builtin_sqrtf((float)tile_w * (float)tile_h);
	const float area = __builtin_sqrtf((float)current_w * (float)current_h);

	const float total_lifts = __builtin_log2(area0) - 1.0F;
	const float lift = __builtin_log2(area) - 1.0F;

	const float lineal = (lift / total_lifts);
	const float exponential = __builtin_powf((lineal * 2.0F), power);

	// AKO_DEV_PRINTF("E\tGate [%.1f/%.1f] -> %.4f -> %.4f -> %.4f\n", lift, total_lifts, lineal, exponential, g);
	return exponential;
}


int16_t akoGate(int factor, size_t tile_w, size_t tile_h, size_t current_w, size_t current_h)
{
	const float exponential = sExponential(GATE_POWER, tile_w, tile_h, current_w, current_h);

	if (factor < 0)
		factor = -factor;

	float g = ((float)factor / (__builtin_powf(2.0F, GATE_POWER) * GATE_FACTOR_SCALE)) * exponential;
	if (g > 32765.0F)
		g = 32765.0F;

	return (int16_t)__builtin_roundf(g);
}


int16_t akoQuantization(int factor, size_t tile_w, size_t tile_h, size_t current_w, size_t current_h)
{
	const float exponential = sExponential(QUANTIZATION_POWER, tile_w, tile_h, current_w, current_h);

	if (factor < 0)
		factor = -factor;

	float q = ((float)factor / (__builtin_powf(2.0F, QUANTIZATION_POWER) * QUANTIZATION_FACTOR_SCALE)) * exponential;
	if (q > 32765.0F)
		q = 32765.0F;
	if (q < 1.0F)
		q = 1.0F;

	return (int16_t)__builtin_roundf(q);
}
