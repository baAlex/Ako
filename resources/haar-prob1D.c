/*-----------------------------

 [haar-prob1d.c]
 - Alexander Brandt 2021

https://en.wiktionary.org/wiki/probo#Latin
https://en.wiktionary.org/wiki/prove#English
https://en.wiktionary.org/wiki/proof#English
https://en.wiktionary.org/wiki/prueba#Spanish

https://en.wiktionary.org/wiki/probation#English
«probation: (archaic) The act of testing; proof» >:(

-----------------------------*/

// gcc haar-prob1d.c -o prob
// clang haar-prob1d.c -o prob


#include <stdio.h>


static void sPrint(size_t len, const float* in)
{
	printf("> ");

	for (size_t i = 0; i < (len - 1); i++)
		printf("%2.2f,\t", in[i]);

	printf("%2.2f\n", in[len - 1]);
}


static void sLift(size_t len, size_t total_len, const float* in, float* out)
{
	if (total_len > len)
	{
		for (size_t i = len; i < total_len; i++) // A memcpy should be better
			out[i] = in[i];
	}

	len = len / 2;

	for (size_t i = 0; i < len; i++)
	{
		// Lowpass
		out[i] = (in[i * 2] + in[i * 2 + 1]) / 2.0f;

		// Highpass / Gradient
		out[len + i] = (in[i * 2]) - (out[i]);
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

	for (size_t i = 0; i < len; i++)
	{
		out[i * 2] = in[i] + in[len + i];
		out[i * 2 + 1] = in[i] - in[len + i];
	}
}


#define LEN 8

int main()
{
	float a[LEN] = {1.0f, 10.0f, 15.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
	float b[LEN];
	float* lifted = NULL;

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
			sPrint(total, out);
			to_process = to_process / 2;

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
