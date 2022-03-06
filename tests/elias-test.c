

#undef NDEBUG

#include "ako-private.h"
#include <assert.h>
#include <stdio.h>


static void sTest(size_t len, size_t buffer_size, uint16_t callback_data,
                  uint16_t (*callback)(size_t, uint16_t, uint16_t))
{
	uint8_t* buffer = malloc(buffer_size);
	assert(buffer != NULL);

	// Encode
	size_t encoded_size = 0;
	{
		struct akoEliasState e = {0};
		size_t total_bits = 0;

		uint16_t value = 0;
		uint8_t* out = buffer;

		for (size_t i = 0; i < len; i++)
		{
			value = callback(i, value, callback_data);
			const int bits = akoEliasEncodeStep(&e, value, &out, buffer + buffer_size);
			total_bits += (size_t)bits;

			// Check
			printf("E%zu %u -> %i bits\t(%.2f total bytes)\n", i, value, bits, (float)total_bits / 8.0F);
			assert(bits != 0);
		}

		encoded_size = akoEliasEncodeEnd(&e, &out, buffer + buffer_size, buffer);

		printf("E %zu total bytes\n\n", encoded_size);
		assert(encoded_size != 0);
	}

	// Decode
	{
		struct akoEliasState d = {0};
		size_t total_bits = 0;

		uint16_t prev_value = 0;
		const uint8_t* in = buffer;

		for (size_t i = 0; i < len; i++)
		{
			int bits = 0;
			const uint16_t value = akoEliasDecodeStep(&d, &in, buffer + encoded_size, &bits);
			total_bits += (size_t)bits;

			// Check
			printf("D%zu %u <- %i bits\t(%.2f total bytes)\n", i, value, bits, (float)total_bits / 8.0F);
			assert(value == callback(i, prev_value, callback_data));
			prev_value = value;
		}

		printf("\n");
	}

	free(buffer);
}


static uint16_t sCallbackLinear(size_t i, uint16_t prev, uint16_t callback_data)
{
	return (uint16_t)(i + (size_t)callback_data);
}

static uint16_t sCallbackConstant(size_t i, uint16_t prev, uint16_t callback_data)
{
	return callback_data;
}

static uint16_t sCallbackRandom(size_t i, uint16_t prev, uint16_t callback_data)
{
	uint16_t x = prev + callback_data + (uint16_t)i;
	x ^= (uint16_t)(x << 7);
	x ^= (uint16_t)(x >> 9);
	x ^= (uint16_t)(x << 8);
	return AKO_ELIAS_MIN + (x % (AKO_ELIAS_MAX - AKO_ELIAS_MIN));
}


int main()
{
	sTest(8, 512, 1, sCallbackLinear);
	sTest(8, 512, AKO_ELIAS_MAX, sCallbackConstant);

	sTest(8, 512, 60, sCallbackLinear);

	sTest(64, 512, 1, sCallbackConstant);
	sTest(64, 512, 666, sCallbackRandom);

	sTest(8, 512, 1, sCallbackLinear);

	return 0;
}
