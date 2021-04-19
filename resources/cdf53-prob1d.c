/*-----------------------------

 [cdf53-prob1d.c]
 - Alexander Brandt 2021

[1] Al-Azawi, Abbas & Jidin (2014). «Low Complexity Multidimensional CDF 5/3 DWT Architecture».
    9th International Symposium on Communication Systems, Networks & Digital Sign (CSNDSP).

[2] Hedge & Ramachandran (2014). «Implementation of CDF 5/3 Wavelet Transform».
    International Journal of Electrical, Electronics and Data Communication

[3] Angelopoulou, Cheung, Masselos, Andreopoulos (2007). «Implementation and Comparison of
    the 5/3 Lifting 2D Discrete Wavelet Transform Computation Schedules on FPGAs».
    Journal of VLSI Signal Processing.

-----------------------------*/

// gcc cdf53-prob1d.c -lm -o prob
// clang cdf53-prob1d.c -lm -o prob


#include <math.h>
#include <stdio.h>


static void sPrint(size_t len, const float* in)
{
	printf("> ");

	for (size_t i = 0; i < (len - 1); i++)
		printf("%+2.1f\t", in[i]);

	printf("%+2.1f\n", in[len - 1]);
}


static void sLift(size_t len, size_t total_len, const float* in, float* out)
{
	// [1] CDF 5/3 formula:
	// 'd[n] = d0[n] - (1.0 / 2.0) * (s0[n] + s0[n + 1])'
	// 's[n] = s0[n] + (1.0 / 4.0) * (d[n] + d[n - 1])'
	// Where:
	// - 'd0' and 's0' are the odd and even input samples
	// - 'd[n]' and 's[n]' the low and high output coefficients

	// [2] Replaces '(1.0 / 4.0)' with '1.0' (it removes it),
	// works of course, but the lowpass ends funny. With a value
	// above 255. In any case, I'm not sure if the same thing
	// happens on [1] under a different input.

	if (total_len > len)
	{
		for (size_t i = len; i < total_len; i++)
			out[i] = in[i];
	}

	len = len / 2;

	for (size_t i = 0; i < len; i++)
	{
		// <RESOLVED TODO>
		// clang-format off

		// Seems that the formula in [1] confuses 'high' with 'low'-pass,
		// since values on the right halve ends being too big to actually be
		// details. Or **I'm doing this wrong** (Nope, see the update).

		// UPDATE: Hedge & Ramachandran [2] seems to be on my side.
		// UPDATE: Angelopoulou Et Al. [3] the same.
		// UPDATE: Fixed formulas below.

		// clang-format on
		// </RESOLVED TODO>

		// Highpass
		// hp[i] = odd[i] - (1.0 / 2.0) * (even[i] + even[i + 1])
		if (i < len - 2)
			out[len + i] = in[(i * 2) + 1] - (1.0f / 2.0f) * (in[(i * 2)] + in[(i * 2) + 2]);
		else
			out[len + i] = in[(i * 2) + 1] - (1.0f / 2.0f) * (in[(i * 2)] + in[(i * 2)]); // Fake last value,
			                                                                              // [3] mirrors the samples

		// Lowpass
		// lp[i] = even[i] + (1.0 / 4.0) * (hp[i] + hp[i - 1])
		if (i > 0)
			out[i] = in[(i * 2)] + (1.0f / 4.0f) * (out[len + i] + out[len + i - 1]);
		else
			out[i] = in[(i * 2)] + (1.0f / 4.0f) * (out[len + i] + out[len + i]); // Fake first value
	}
}


static void sUnLift(size_t len, size_t total_len, const float* in, float* out)
{
	if (total_len > len)
	{
		for (size_t i = len; i < total_len; i++)
			out[i] = in[i];
	}

	len = len / 2;

	// Even
	for (size_t i = 0; i < len; i++)
	{
		if (i > 0) // I can remove this conditional by moving it outside the 'for' and starting it from 1
			out[i * 2] = in[i] - (1.0f / 4.0f) * (in[len + i] + in[len + i - 1]);
		else
			out[i * 2] = in[i] - (1.0f / 4.0f) * (in[len + i] + in[len + i]);
	}

	// Odd
	// Two loops because 'out[(i + 1) * 2]' requires the even values beforehand
	for (size_t i = 0; i < len; i++)
	{
		if (i < len - 2) // Same as above
			out[i * 2 + 1] = in[len + i] + (1.0f / 2.0f) * (out[i * 2] + out[(i + 1) * 2]);
		else
			out[i * 2 + 1] = in[len + i] + (1.0f / 2.0f) * (out[i * 2] + out[i * 2]);
	}
}


#define LEN 16

int main()
{
	float a[LEN];
	float b[LEN];
	float* lifted = NULL;

	// Create data
	{
		float average = 0.0f;
		for (size_t i = 0; i < LEN; i++)
		{
			a[i] = roundf(((float)(i + 1) / (float)LEN) * 255.0f);
			average += a[i];
		}
		printf("Average: %.2f\n", average / (float)LEN);
	}

	sPrint(LEN, a);

	// Lift
	{
		size_t total = LEN;
		size_t to_process = LEN;
		float* in = a;
		float* out = b;
		while (to_process != 1)
		{
			sLift(to_process, total, in, out);
			to_process = to_process / 2;

			if (to_process == 1)
				printf("\n");

			sPrint(total, out);

			// Swap buffers
			lifted = out;
			out = in;
			in = lifted;
		}
	}

	printf("\n");

	// Unlift
	{
		size_t total = LEN;
		size_t to_process = 2;
		float* in = lifted;
		float* out = (lifted == a) ? b : a;
		while (to_process <= total)
		{
			sUnLift(to_process, total, in, out);
			sPrint(total, out);
			to_process = to_process * 2;

			float* temp = out;
			out = in;
			in = temp;
		}
	}

	return 0;
}
