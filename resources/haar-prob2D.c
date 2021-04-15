/*-----------------------------

 [haar-prob2d.c]
 - Alexander Brandt 2021
-----------------------------*/

// gcc haar-prob2d.c -lm -o prob
// clang haar-prob2d.c -lm -o prob


#include <math.h>
#include <stdint.h>
#include <stdio.h>


static void sSavePgm(size_t dimension, const float* data, const char* prefix)
{
	char filename[256];
	sprintf(filename, "%s%zu.pgm", prefix, dimension);

	FILE* fp = fopen(filename, "wb");
	fprintf(fp, "P5\n%zu\n%zu\n255\n", dimension, dimension);

	for (size_t i = 0; i < (dimension * dimension); i++)
	{
		uint8_t p = (uint8_t)truncf(data[i]);
		fwrite(&p, 1, 1, fp);
	}

	fclose(fp);
}


static void sPrint(size_t len, const float* in)
{
	printf("> ");

	for (size_t i = 0; i < (len - 1); i++)
		printf("%2.2f,\t", in[i]);

	printf("%2.2f\n", in[len - 1]);
}


static void sLift(size_t len, size_t total_len, size_t dimension, const float* in, float* out)
{
	if (total_len > len)
	{
		for (size_t i = len; i < total_len; i++) // Clang is intelligent enough to handle this
			out[i] = in[i];
	}

	len = len / 4;

	for (size_t i = 0; i < len; i++)
	{
		const size_t row = (i / (dimension / 2)) * 2;
		const size_t col = (i % (dimension / 2)) * 2;

		const float a = in[(row * dimension) + col];
		const float b = in[(row * dimension) + col + 1];
		const float c = in[((row + 1) * dimension) + col];
		const float d = in[((row + 1) * dimension) + col + 1];

		const float ab = (a + b) / 2.0f;
		const float cd = (c + d) / 2.0f;

		out[i] = (a + b + c + d) / 4.0f; // Lowpass
		out[len + i] = out[i] - ab;      // Delta from lowpass to midpoint
		out[len * 2 + i] = a - ab;       // Delta from midpoint AB to A/B
		out[len * 3 + i] = c - cd;       // Delta from midpoint CD to C/D
	}
}


static void sUnLift(size_t len, size_t total_len, size_t dimension, const float* in, float* out)
{
	if (total_len > len)
	{
		for (size_t i = len; i < total_len; i++)
			out[i] = in[i];
	}

	len = len / 4;

	for (size_t i = 0; i < len; i++)
	{
		const size_t row = (i / (dimension / 2)) * 2;
		const size_t col = (i % (dimension / 2)) * 2;

		// From lowpass to midpoint AB, then to A/B
		out[(row * dimension) + col] = (in[i] - in[len + i]) + in[(len * 2) + i];     // A
		out[(row * dimension) + col + 1] = (in[i] - in[len + i]) - in[(len * 2) + i]; // B

		// Ditto, to C/D
		out[((row + 1) * dimension) + col] = (in[i] + in[len + i]) + in[(len * 3) + i];     // C
		out[((row + 1) * dimension) + col + 1] = (in[i] + in[len + i]) - in[(len * 3) + i]; // D
	}
}


#define WIDTH 16
#define LEN (WIDTH * WIDTH)

int main()
{
	float a[LEN];
	float b[LEN];
	float* lifted = NULL;

	for (size_t i = 0; i < LEN; i++)
		a[i] = roundf(((float)(i + 1) / (float)LEN) * 255.0f);

	sPrint(LEN, a);

	// Lift
	{
		size_t total = LEN;
		size_t to_process = LEN;
		size_t dimension = WIDTH;
		float* in = a;
		float* out = b;
		while (to_process != 1)
		{
			sLift(to_process, total, dimension, in, out);
			sPrint(total, out);
			to_process = to_process / 4;
			dimension = dimension / 2;

			sSavePgm(dimension, out, "l");

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
		size_t to_process = 4;
		size_t dimension = 2;
		float* in = lifted;
		float* out = (lifted == a) ? b : a;
		while (to_process <= total)
		{
			sUnLift(to_process, total, dimension, in, out);
			sPrint(total, out);

			sSavePgm(dimension, out, "u");

			to_process = to_process * 4;
			dimension = dimension * 2;

			float* temp = out;
			out = in;
			in = temp;
		}
	}

	return 0;
}
