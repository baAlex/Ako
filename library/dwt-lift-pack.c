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

 [dwt-lift-pack.c]
 - Alexander Brandt 2021
-----------------------------*/

#include <assert.h>
#include <string.h>

#include "ako.h"
#include "dwt.h"


static void sLift1d(const struct DwtLiftSettings* s, size_t len, size_t initial_len, const int16_t* in, int16_t* out)
{
	// Highpass
	// Haar:  hp[i] = odd[i] - (even[i] + odd[i]) / 2
	// CDF53: hp[i] = odd[i] - (even[i] + even[i + 1]) / 2
	// 97DD:  hp[i] = odd[i] - (-even[i - 1] + 9 * even[i] + 9 * even[i + 1] - even[i + 2] + 8) / 16)

#if (AKO_WAVELET == 0)
	// Haar
	for (size_t i = 0; i < len; i++)
		out[len + i] = in[(i * 2) + 1] - ((in[(i * 2)] + in[(i * 2) + 1] + 1) >> 1);
#endif

#if (AKO_WAVELET == 1)
	// CDF53
	for (size_t i = 0; i < len; i++)
	{
		if (i < len - 2)
			out[len + i] = in[(i * 2) + 1] - ((in[(i * 2)] + in[(i * 2) + 2] + 1) >> 1);
		else
			out[len + i] = in[(i * 2) + 1] - ((in[(i * 2)] + in[(i * 2)] + 1) >> 1); // Fake last value
	}
#endif

#if (AKO_WAVELET == 2)
	// 97DD
	if (len > 4) // Needs 4 samples
	{
		for (size_t i = 0; i < len; i++)
		{
			int16_t even_il1 = in[(i * 2) - 2];
			int16_t even_i = in[(i * 2)];
			int16_t even_ip1 = in[(i * 2) + 2];
			int16_t even_ip2 = in[(i * 2) + 4];

			if (i < 1)
				even_il1 = even_i;
			if (i > len - 2)
				even_ip1 = even_i;
			if (i > len - 4)
				even_ip2 = even_ip1;

			out[len + i] = in[(i * 2) + 1] - ((-even_il1 + 9 * even_i + 9 * even_ip1 - even_ip2 + 8) >> 4);
		}
	}
	else // Fallback to CDF53
	{
		for (size_t i = 0; i < len; i++)
		{
			if (i < len - 2)
				out[len + i] = in[(i * 2) + 1] - ((in[(i * 2)] + in[(i * 2) + 2] + 1) >> 1);
			else
				out[len + i] = in[(i * 2) + 1] - ((in[(i * 2)] + in[(i * 2)] + 1) >> 1); // Fake last value
		}
	}
#endif

	// Lowpass
	// CDF53, 97DD: lp[i] = even[i] + (hp[i] + hp[i - 1]) / 4
	// Haar:        lp[i] = (even[i] + odd[i]) / 2
	for (size_t i = 0; i < len; i++)
	{
#if (AKO_WAVELET != 0)
		// CD53, 97DD
		if (i > 0)
			out[i] = in[(i * 2)] + ((out[len + i] + out[len + i - 1] + 2) >> 2);
		else
			out[i] = in[(i * 2)] + ((out[len + i] + out[len + i] + 2) >> 2); // Fake first value
#else
		// Haar
		out[i] = (in[(i * 2)] + in[(i * 2) + 1] + 1) >> 1;
#endif
	}

	// Degrade highpass
	for (size_t i = 0; i < len; i++)
	{
		// Remove high frequency levels
		if (s->limit != 0 && (len * 2) > s->limit)
			out[len + i] = 0;

		// Gate
		const float gate_mk1 = (s->detail_gate * 4.0f * ((float)len / (float)initial_len));
		// const float gate_mk2 = (s->detail_gate * 1.0f * sqrtf((float)len / (float)initial_len));
		const float gate = gate_mk1;

		if ((float)out[len + i] > -gate && (float)out[len + i] < gate)
			out[len + i] = 0;
	}
}


static void sLift2d(const struct DwtLiftSettings* s, size_t dimension, size_t initial_dimension, void* aux_buffer,
                    int16_t* inout)
{
	int16_t* buffer_a = (int16_t*)aux_buffer;
	int16_t* buffer_b = (int16_t*)aux_buffer + initial_dimension;
	int16_t* temp = inout;

	// Rows
	for (size_t i = 0; i < dimension; i++)
	{
		memcpy(buffer_a, temp, sizeof(int16_t) * dimension);
		sLift1d(s, dimension / 2, initial_dimension, buffer_a, temp);

		temp = temp + initial_dimension;
	}

	// Columns
	for (size_t i = 0; i < dimension; i++)
	{
		temp = inout + i;
		for (size_t u = 0; u < dimension; u++)
		{
			buffer_a[u] = *temp;
			temp = temp + initial_dimension;
		}

		sLift1d(s, dimension / 2, initial_dimension, buffer_a, buffer_b);

		temp = inout + i;
		for (size_t u = 0; u < dimension; u++)
		{
			*temp = buffer_b[u];
			temp = temp + initial_dimension;
		}
	}
}


void DwtLiftPlane(const struct DwtLiftSettings* s, size_t dimension, void** aux_buffer, int16_t* inout)
{
	size_t current_dimension = dimension;
	size_t initial_dimension = dimension;

	*aux_buffer = realloc(*aux_buffer, sizeof(int16_t) * dimension * 2);
	assert(*aux_buffer != NULL);

	while (current_dimension != 2)
	{
		sLift2d(s, current_dimension, initial_dimension, *aux_buffer, inout);
		current_dimension = current_dimension / 2;
	}
}


static void s2dToLinearH(size_t w, size_t h, size_t pitch, const int16_t* in, int16_t* out)
{
	for (size_t r = 0; r < h; r++)
	{
		for (size_t c = 0; c < w; c++)
		{
			*out = in[(r * pitch) + c];
			out = out + 1;
		}
	}
}


static void s2dToLinearV(size_t w, size_t h, size_t pitch, const int16_t* in, int16_t* out)
{
	for (size_t c = 0; c < w; c++)
	{
		for (size_t r = 0; r < h; r++)
		{
			*out = in[c + (r * pitch)];
			out = out + 1;
		}
	}
}


#define MAX_LEVELS 128 // TODO, value depends on dimension

void DwtPackImage(size_t dimension, size_t channels, int16_t* inout)
{
	int16_t* level[MAX_LEVELS] = {NULL};
	int16_t* lp = NULL;
	size_t l = 0;

	// Lowpass (2x2 px)
	lp = calloc(1, sizeof(int16_t) * 2 * 2 * channels);

	for (size_t ch = 0; ch < channels; ch++)
	{
		const int16_t* in_plane = inout + dimension * dimension * ch;
		s2dToLinearH(2, 2, dimension, in_plane, lp + 2 * 2 * ch);
	}

	// Highpasses (from LP to 4x4, to 8x8, to 16x16, to 32x32...)
	for (size_t current = 2; current < dimension; current = current * 2)
	{
		const size_t length = current * current * channels * 3; // Space for B, C and D
		level[l] = calloc(1, sizeof(int16_t) * length);

		for (size_t ch = 0; ch < channels; ch++)
		{
			const int16_t* in_plane = inout + dimension * dimension * ch;

			s2dToLinearV(current, current, dimension,
			             in_plane + dimension * current, // Higpass C
			             (level[l] + current * current * 3 * ch) + (current * current * 0));
			s2dToLinearV(current, current * 2, dimension,
			             in_plane + current, // Highpasses B and D
			             (level[l] + current * current * 3 * ch) + (current * current * 1));
		}

		l = l + 1;
	}

	// Pack in correct order
	memset(inout, 0, sizeof(int16_t) * dimension * dimension * channels);
	memcpy(inout, lp, sizeof(int16_t) * 2 * 2 * channels);

	inout = inout + 2 * 2 * channels;
	l = 0;

	for (size_t current = 2; current < dimension; current = current * 2)
	{
		const size_t length = current * current * channels * 3;
		memcpy(inout, level[l], sizeof(int16_t) * length);
		free(level[l]);

		inout = inout + length;
		l = l + 1;
	}
}
