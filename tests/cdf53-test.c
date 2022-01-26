

#undef NDEBUG

#include "ako-private.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>


// It's dangerous to go alone! Take this:
// def hp(odd, even, evenp1) return odd - (even + evenp1) / 2 end
// def lp(even, hpl1, hp) return even + (hpl1 + hp) / 4 end


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


static void sHorizontalTest(size_t len, int16_t q, int16_t callback_data, int16_t (*callback)(size_t, int16_t, int16_t))
{
	const size_t plus_one_rule = (len % 2 != 0) ? 1 : 0;
	int16_t* buffer_a = malloc((len) * sizeof(int16_t));
	int16_t* buffer_b = malloc((len + plus_one_rule) * sizeof(int16_t));
	int16_t* buffer_c = malloc((len) * sizeof(int16_t));
	assert(buffer_a != NULL);
	assert(buffer_b != NULL);
	assert(buffer_c != NULL);

	printf("\nCdf53 Horizontal (len: %zu):\n", len);

	// Generate data
	{
		int16_t value = 0;
		for (size_t i = 0; i < len; i++)
		{
			value = callback(i, value, callback_data);
			buffer_a[i] = value;
		}

		sHorizontalPrint(sMin(len, 16), 1, "   \t", "\t", buffer_a);
	}

	// DWT
	{
		akoCdf53LiftH(AKO_WRAP_CLAMP, q, 0, 1, (len + plus_one_rule) / 2, plus_one_rule, 0, buffer_a, buffer_b);

		sHorizontalPrint(sMin((len + plus_one_rule) / 2, 8), 1, "Lp:\t", "\t\t", buffer_b);
		sHorizontalPrint(sMin((len + plus_one_rule) / 2, 8), 1, "Hp:\t", "\t\t", buffer_b + (len + plus_one_rule) / 2);
	}

#if 1
	// Inverse DWT
	{
		akoCdf53UnliftH(AKO_WRAP_CLAMP, q, (len + plus_one_rule) / 2, 1, 0, plus_one_rule, buffer_b,
		                buffer_b + (len + plus_one_rule) / 2, buffer_c);
		sHorizontalPrint(sMin(len, 16), 1, "   \t", "\t", buffer_c);
	}

	// Check data (if coefficients were not quantized)
	if (q <= 1)
	{
		int16_t value = 0;
		for (size_t i = 0; i < len; i++)
		{
			value = callback(i, value, callback_data);
			assert(buffer_a[i] == value);
		}
	}
#endif

	free(buffer_a);
	free(buffer_b);
	free(buffer_c);
}


static void sVerticalTest(size_t height, int16_t q, int16_t callback_data,
                          int16_t (*callback)(size_t, int16_t, int16_t))
{
	const size_t width = 3;
	int16_t* buffer_a = malloc(height * width * sizeof(int16_t));
	int16_t* buffer_b = malloc(height * width * sizeof(int16_t));
	int16_t* buffer_c = malloc(height * sizeof(int16_t));
	assert(buffer_a != NULL);
	assert(buffer_b != NULL);
	assert(buffer_c != NULL);

	printf("\nCdf53 Vertical (len: %zu):\n", height);

	// Generate data
	{
		int16_t value = 0;
		for (size_t r = 0; r < height; r++)
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

		sVerticalPrint(sMin(height, 16), width, 1, "   \t", "\t", buffer_a);
	}

	// DWT
	{
		akoCdf53LiftV(AKO_WRAP_CLAMP, q, 0, width, height / 2, buffer_a, buffer_b);

		sVerticalPrint(sMin(height / 2, 8), width, 1, "Lp:\t", "\t\t", buffer_b);
		sVerticalPrint(sMin(height / 2, 8), width, 1, "Hp:\t", "\t\t", buffer_b + (width * (height / 2)));
	}

#if 1
	// Inverse DWT
	{
		akoCdf53InPlaceishUnliftV(AKO_WRAP_CLAMP, q, width, height / 2, buffer_b, buffer_b + (width * (height / 2)),
		                          buffer_b, buffer_b + (width * (height / 2)));

		size_t i = 0;
		for (size_t r = 0; r < (height / 2); r++)
		{
			buffer_c[i++] = buffer_b[width * r];
			buffer_c[i++] = buffer_b[width * (r + height / 2)];
		}

		sHorizontalPrint(sMin(height, 16), 1, "   \t", "\t", buffer_c);
	}

	// Check data (if coefficients were not quantized)
	if (q <= 1)
	{
		int16_t value = 0;
		for (size_t i = 0; i < height; i++)
		{
			value = callback(i, value, callback_data);
			assert(buffer_c[i] == value);
		}
	}
#endif

	free(buffer_a);
	free(buffer_b);
	free(buffer_c);
}


static int16_t sCallbackLinear(size_t i, int16_t prev, int16_t callback_data)
{
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
	sHorizontalTest(16, 1, 1, sCallbackLinear);
	sVerticalTest(16, 1, 1, sCallbackLinear);

	sHorizontalTest(16, 1, 1, sCallbackRandom);
	sVerticalTest(16, 1, 1, sCallbackRandom);

	sHorizontalTest(8, 1, 4, sCallbackRandom);
	sVerticalTest(8, 1, 4, sCallbackRandom);

	sHorizontalTest(128, 1, 4, sCallbackRandom);
	sVerticalTest(128, 1, 4, sCallbackRandom);

	// sHorizontalTest(14, 1, 1, sCallbackLinear);
	// sHorizontalTest(13, 1, 1, sCallbackLinear);

	// sHorizontalTest(16, 1, 1, sCallbackRandom);
	// sHorizontalTest(16, 2, 1, sCallbackRandom);
	// sHorizontalTest(512, 1, 5, sCallbackRandom);
	return 0;
}
