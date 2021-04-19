/*-----------------------------

 [cdf53-prob2d.c]
 - Alexander Brandt 2021
-----------------------------*/

// gcc cdf53-prob2d.c -lm -o prob
// clang cdf53-prob2d.c -lm -o prob


#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define WIDTH 512
#define GATE 48.0f


static void sSavePgm(size_t dimension, const float* data, const char* filename_format, ...)
{
	char filename1[256];
	char filename2[256];
	va_list args;

	va_start(args, filename_format);
	vsprintf(filename1, filename_format, args);
	va_end(args);

	sprintf(filename2, "%s-%zu.pgm", filename1, dimension);

	FILE* fp = fopen(filename2, "wb");
	fprintf(fp, "P5\n%zu\n%zu\n255\n", dimension, dimension);

	for (size_t i = 0; i < (dimension * dimension); i++)
	{
		float f = truncf(data[i]);
		f = (f > 0.0f) ? (f < 255.0f) ? f : 255.0f : 0.0f;

		uint8_t p = (uint8_t)f;
		fwrite(&p, 1, 1, fp);
	}

	fclose(fp);
}


static void sLift1d(size_t len, size_t initial_len, const float* in, float* out)
{
	for (size_t i = 0; i < len; i++)
	{
		// Highpass
		// hp[i] = odd[i] - (1.0 / 2.0) * (even[i] + even[i + 1])
		if (i < len - 2)
			out[len + i] = in[(i * 2) + 1] - (1.0f / 2.0f) * (in[(i * 2)] + in[(i * 2) + 2]);
		else
			out[len + i] = in[(i * 2) + 1] - (1.0f / 2.0f) * (in[(i * 2)] + in[(i * 2)]); // Fake last value

		// Lowpass
		// lp[i] = even[i] + (1.0 / 4.0) * (hp[i] + hp[i - 1])
		if (i > 0)
			out[i] = in[(i * 2)] + (1.0f / 4.0f) * (out[len + i] + out[len + i - 1]);
		else
			out[i] = in[(i * 2)] + (1.0f / 4.0f) * (out[len + i] + out[len + i]); // Fake first value
	}

	// Degrade highpass
	const float gate = GATE * ((float)len / (float)initial_len);

	for (size_t i = 0; i < len; i++)
	{
		// Squish from [-255, 255] to [-128, 127]
		out[len + i] = roundf(out[len + i] / 2.0f);
		if (out[len + i] < (float)INT8_MIN)
			out[len + i] = (float)INT8_MIN;
		if (out[len + i] > (float)INT8_MAX)
			out[len + i] = (float)INT8_MAX;

		// Gate
		if (out[len + i] > -gate && out[len + i] < gate)
			out[len + i] = 0.0f;
	}
}


static void sUnlift1d(size_t len, const float* in, float* out)
{
	// Even
	for (size_t i = 0; i < len; i++)
	{
		const float hp = in[len + i] * 2.0f;          // De-squish
		const float hp_prev = in[len + i - 1] * 2.0f; // Ditto

		if (i > 0)
			out[i * 2] = in[i] - (1.0f / 4.0f) * (hp + hp_prev);
		else
			out[i * 2] = in[i] - (1.0f / 4.0f) * (hp + hp);
	}

	// Odd
	for (size_t i = 0; i < len; i++)
	{
		const float hp = in[len + i] * 2.0f;

		if (i < len - 2)
			out[i * 2 + 1] = hp + (1.0f / 2.0f) * (out[i * 2] + out[(i + 1) * 2]);
		else
			out[i * 2 + 1] = hp + (1.0f / 2.0f) * (out[i * 2] + out[i * 2]);
	}
}


static void sLift2d(size_t dimension, size_t initial_dimension, float* input, float* output)
{
	union
	{
		float *row, *col;
	} in;
	union
	{
		float *row, *col;
	} out;

	float* buffer_a = input;                     // To recycle later
	float* buffer_b = input + initial_dimension; // Ditto

	// Rows
	in.row = input;
	out.row = output;

	for (size_t i = 0; i < dimension; i++)
	{
		sLift1d(dimension / 2, initial_dimension, in.row, out.row);
		in.row = in.row + initial_dimension;
		out.row = out.row + initial_dimension;
	}

	// Columns
	for (size_t i = 0; i < dimension; i++)
	{
		in.col = output + i;
		for (size_t u = 0; u < dimension; u++) // TODO, think on something... better
		{
			buffer_a[u] = *in.col;
			in.col = in.col + initial_dimension;
		}

		sLift1d(dimension / 2, initial_dimension, buffer_a, buffer_b);

		out.col = output + i;
		for (size_t u = 0; u < dimension; u++)
		{
			*out.col = buffer_b[u];
			out.col = out.col + initial_dimension;
		}
	}

	memcpy(input, output, sizeof(float) * (initial_dimension * initial_dimension)); // Lazy
}


static void sUnlift2d(size_t dimension, size_t final_dimension, float* input, float* output)
{
	union
	{
		float *row, *col;
	} in;
	union
	{
		float *row, *col;
	} out;

	float buffer_a[WIDTH]; // Ahhh!
	float buffer_b[WIDTH]; // Aaahhh!!!

	// Columns
	for (size_t i = 0; i < dimension; i++)
	{
		in.col = input + i;
		for (size_t u = 0; u < dimension; u++)
		{
			buffer_a[u] = *in.col;
			in.col = in.col + final_dimension;
		}

		sUnlift1d(dimension / 2, buffer_a, buffer_b);

		out.col = output + i;
		for (size_t u = 0; u < dimension; u++)
		{
			*out.col = buffer_b[u];
			out.col = out.col + final_dimension;
		}
	}

	// Rows
	in.row = output;
	out.row = input;

	for (size_t i = 0; i < dimension; i++)
	{
		sUnlift1d(dimension / 2, in.row, out.row);
		in.row = in.row + final_dimension;
		out.row = out.row + final_dimension;
	}
}


#define LEN (WIDTH * WIDTH)
static float a[LEN];
static float b[LEN];

int main()
{
	// Create data
	for (size_t i = 0; i < LEN; i++)
	{
		const size_t col = (i % WIDTH);
		const size_t row = (i / WIDTH);

		a[i] = truncf(((float)(i + 1) / (float)LEN) * 255.0f);
		a[i] = a[i] + ((float)rand() / (float)RAND_MAX) * 48.0f;

		if (sqrtf(powf(((float)WIDTH / 2.0f - (float)col), 2.0f) + powf(((float)WIDTH / 2.0 - (float)row), 2.0f)) <
		    (float)WIDTH / 4.0f)
		{
			a[i] = 0.0f;
		}

		if (sqrtf(powf(((float)WIDTH / 2.0f - (float)col), 2.0f) + powf(((float)WIDTH / 2.0 - (float)row), 2.0f)) <
		    (float)WIDTH / 8.0f)
		{
			if ((col > WIDTH / 2 - 2 && col < WIDTH / 2 + 2) || (row > WIDTH / 2 - 2 && row < WIDTH / 2 + 2))
				a[i] = 255.0f;
		}
	}

	sSavePgm(WIDTH, a, "initial");

	// Lift
	{
		size_t current_dimension = WIDTH;
		size_t initial_dimension = WIDTH;
		size_t lift_no = 0;

		while (current_dimension != 2)
		{
			sLift2d(current_dimension, initial_dimension, a, b);
			current_dimension = current_dimension / 2;

			lift_no = lift_no + 1;
			sSavePgm(WIDTH, b, "lift-%zu", lift_no);
		}
	}

	// Unlift
	{
		size_t current_dimension = 4;
		size_t final_dimension = WIDTH;
		size_t unlift_no = 0;

		while (current_dimension <= final_dimension)
		{
			sUnlift2d(current_dimension, final_dimension, a, b); // Outputs in A
			current_dimension = current_dimension * 2;

			unlift_no = unlift_no + 1;
			sSavePgm(WIDTH, a, "unlift-%zu", unlift_no);
		}
	}

	return 0;
}
