

#include "ako.h"
#undef NDEBUG

#include "ako-private.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>


#define PRINT_MAX 22


// It's dangerous to go alone! Take this:
// def hp(odd, even_l1, even, even_p1, even_p2) odd + ((even_l1 + even_p2 - 9 * (even + even_p1)) / 16.0) end
// def lp(even, hp_l2, hp_l1, hp, hp_p1) even + ((-hp_l2 - hp_p1 + 9 * (hp_l1 + hp)) / 32.0) end


static void sEvenOddPrint(size_t len, const char* head, const char* separator)
{
	printf("%s", head);

	for (size_t i = 0; i < len; i++)
		printf("%s%s", (i & 1) ? "o" : "e", separator);

	printf("\n");
}

static void sHorizontalPrint(size_t len, size_t step, const char* head, const char* separator, const int16_t* in)
{
	printf("%s", head);

	for (size_t i = 0; i < len; i += step)
		printf("%i%s", in[i], separator);

	printf("\n");
}

static void sVerticalPrint(size_t len, size_t stride, size_t step, const char* head, const char* separator,
                           const int16_t* in)
{
	printf("%s", head);

	for (size_t i = 0; i < len; i += step)
		printf("%i%s", in[stride * i], separator);

	printf("\n");
}


static size_t sMin(size_t a, size_t b)
{
	return (a < b) ? a : b;
}


static void sHorizontalTest(size_t len, int16_t callback_data, int16_t (*callback)(size_t, int16_t, int16_t))
{
	const size_t plus_one_rule = (len % 2 != 0) ? 1 : 0;
	int16_t* buffer_a = malloc((len) * sizeof(int16_t));
	int16_t* buffer_b = malloc((len + plus_one_rule) * sizeof(int16_t));
	int16_t* buffer_c = malloc((len) * sizeof(int16_t));
	assert(buffer_a != NULL);
	assert(buffer_b != NULL);
	assert(buffer_c != NULL);

	printf("\n# Dd137 Horizontal (len: %zu):\n", len);

	// Generate data
	{
		int16_t value = 0;
		for (size_t i = 0; i < len; i++)
		{
			value = callback(i, value, callback_data);
			buffer_a[i] = value;
		}

		sEvenOddPrint(sMin(len, PRINT_MAX), "   \t", "\t");
		sHorizontalPrint(sMin(len, PRINT_MAX), 1, "   \t", "\t", buffer_a);
	}

	for (int i = 0; i < 4; i++)
	{
		// clang-format off
		enum akoWrap wr;
		switch (i)
		{
		case 0: wr = AKO_WRAP_CLAMP;  printf("[clamp]\n"); break;
		case 1: wr = AKO_WRAP_MIRROR; printf("[mirror]\n"); break;
		case 2: wr = AKO_WRAP_ZERO;   printf("[zero]\n"); break;
		case 3: wr = AKO_WRAP_REPEAT; printf("[repeat]\n"); break;
		}
		// clang-format on

		// DWT (buffer a to b)
		akoDd137LiftH(wr, 1, (len + plus_one_rule) / 2, plus_one_rule, 0, buffer_a, buffer_b);

		sHorizontalPrint(sMin((len + plus_one_rule) / 2, PRINT_MAX / 2), 1, "Lp:\t", "\t\t", buffer_b);
		sHorizontalPrint(sMin((len + plus_one_rule) / 2, PRINT_MAX / 2), 1, "Hp:\t", "\t\t",
		                 buffer_b + (len + plus_one_rule) / 2);

		// Inverse DWT (buffer b to c)
		akoDd137UnliftH(wr, (len + plus_one_rule) / 2, 1, 0, plus_one_rule, buffer_b,
		                buffer_b + (len + plus_one_rule) / 2, buffer_c);

#if 1
		// Check data
		int16_t value = 0;
		for (size_t i = 0; i < len; i++)
		{
			value = callback(i, value, callback_data);

			if (buffer_c[i] != value)
			{
				sHorizontalPrint(sMin(len, PRINT_MAX), 1, "   \t", "\t", buffer_c);
				printf("Error at offset %zu\n", i);
			}

			assert(buffer_c[i] == value);
		}
#else
		sHorizontalPrint(sMin(len, PRINT_MAX), 1, "   \t", "\t", buffer_c);
#endif
	}

	free(buffer_a);
	free(buffer_b);
	free(buffer_c);
}


static void sVerticalTest(size_t height, int16_t callback_data, int16_t (*callback)(size_t, int16_t, int16_t))
{
	assert((height % 2) == 0); // Vertical functions doesn't follow the plus one rule

	const size_t width = 3; // To check boundaries errors
	int16_t* buffer_a = malloc(height * width * sizeof(int16_t));
	int16_t* buffer_b = malloc(height * width * sizeof(int16_t));
	int16_t* buffer_c = malloc(height * sizeof(int16_t));
	assert(buffer_a != NULL);
	assert(buffer_b != NULL);
	assert(buffer_c != NULL);

	printf("\n# Dd137 Vertical (len: %zu):\n", height);

	// Generate data
	{
		int16_t value = 0;
		for (size_t r = 0; r < height; r++)
		{
			for (size_t c = 0; c < width; c++)
			{
				if (c == 0)
				{
					value = callback(r, value, callback_data);
					buffer_a[width * r + c] = value;
				}
				else
					buffer_a[width * r + c] = 99;
			}
		}

		sEvenOddPrint(sMin(height, PRINT_MAX), "   \t", "\t");
		sVerticalPrint(sMin(height, PRINT_MAX), width, 1, "   \t", "\t", buffer_a);
	}

	for (int i = 0; i < 4; i++)
	{
		// clang-format off
		enum akoWrap w;
		switch (i)
		{
		case 0: w = AKO_WRAP_CLAMP;  printf("[clamp]\n"); break;
		case 1: w = AKO_WRAP_MIRROR; printf("[mirror]\n"); break;
		case 2: w = AKO_WRAP_ZERO;   printf("[zero]\n"); break;
		case 3: w = AKO_WRAP_REPEAT; printf("[repeat]\n"); break;
		}
		// clang-format on

		// DWT (buffer a to b)
		akoDd137LiftV(w, width, height / 2, buffer_a, buffer_b);

		sVerticalPrint(sMin(height / 2, PRINT_MAX / 2), width, 1, "Lp:\t", "\t\t", buffer_b);
		sVerticalPrint(sMin(height / 2, PRINT_MAX / 2), width, 1, "Hp:\t", "\t\t", buffer_b + (width * (height / 2)));

		// Inverse DWT (in place in buffer b)
		akoDd137InPlaceishUnliftV(w, width, height / 2, buffer_b, buffer_b + (width * (height / 2)), buffer_b,
		                          buffer_b + (width * (height / 2)));

		// Copy as row (buffer b to c)
		size_t i = 0;
		for (size_t r = 0; r < (height / 2); r++)
		{
			buffer_c[i++] = buffer_b[width * r];
			buffer_c[i++] = buffer_b[width * (r + height / 2)];
		}

#if 1
		// Check data
		int16_t value = 0;
		for (size_t i = 0; i < height; i++)
		{
			value = callback(i, value, callback_data);

			if (buffer_c[i] != value)
			{
				sHorizontalPrint(sMin(height, PRINT_MAX), 1, "   \t", "\t", buffer_c);
				printf("Error at offset %zu\n", i);
			}

			assert(buffer_c[i] == value);
		}
#else
		sHorizontalPrint(sMin(height, PRINT_MAX), 1, "   \t", "\t", buffer_c);
#endif
	}

	free(buffer_a);
	free(buffer_b);
	free(buffer_c);
}


static int16_t sCallbackLinear(size_t i, int16_t prev, int16_t callback_data)
{
	(void)prev;
	return (int16_t)(i + (size_t)callback_data) * 5;
}

static int16_t sCallbackRandom(size_t i, int16_t prev, int16_t callback_data)
{
	uint16_t x = (uint16_t)(prev + callback_data + (int16_t)i);
	x ^= (uint16_t)(x << 7);
	x ^= (uint16_t)(x >> 9);
	x ^= (uint16_t)(x << 8);
	return (int16_t)((AKO_ELIAS_MIN + (x % (AKO_ELIAS_MAX - AKO_ELIAS_MIN))) % 64);
}


int main()
{
	sHorizontalTest(22, 2, sCallbackRandom);
	sVerticalTest(22, 2, sCallbackRandom);

	sHorizontalTest(22, 1, sCallbackLinear);
	sVerticalTest(22, 1, sCallbackLinear);

#if 1
	sHorizontalTest(13, 3, sCallbackRandom);
	sHorizontalTest(17, 4, sCallbackRandom);

	sHorizontalTest(512, 5, sCallbackRandom);
	sVerticalTest(512, 5, sCallbackRandom);

	sHorizontalTest(150, 5, sCallbackRandom);
	sVerticalTest(150, 5, sCallbackRandom);

	sHorizontalTest(300, 5, sCallbackRandom);
	sVerticalTest(300, 5, sCallbackRandom);
#endif

	return 0;
}
