

#undef NDEBUG

#include "ako-private.h"
#include <assert.h>
#include <stdio.h>


void sTest(size_t len, size_t buffer_size, uint16_t callback_data,
           uint16_t (*data_callback)(size_t, uint16_t, uint16_t))
{
	uint8_t* buffer = malloc(buffer_size);
	assert(buffer != NULL);

	// Encode
	size_t encoded_size = 0;
	{
		struct akoEliasStateE e = akoEliasEncodeSet(buffer, buffer_size);
		size_t total_bits = 0;

		uint16_t value = 0;

		for (size_t i = 0; i < len; i++)
		{
			value = data_callback(i, value, callback_data);
			const int bits = akoEliasEncodeStep(&e, value);
			total_bits += (size_t)bits;

			// Check
			printf("E%zu %u -> %i bits\t(%.2f total bytes)\n", i, value, bits, (float)total_bits / 8.0F);
			assert(bits != 0);
		}

		encoded_size = akoEliasEncodeEnd(&e, buffer);

		printf("E %zu total bytes\n\n", encoded_size);
		assert(encoded_size != 0);
	}

	// Decode
	{
		struct akoEliasStateD d = akoEliasDecodeSet(buffer, encoded_size);
		size_t total_bits = 0;

		uint16_t prev_value = 0;

		for (size_t i = 0; i < len; i++)
		{
			uint16_t value = 0;
			const int bits = akoEliasDecodeStep(&d, &value);
			total_bits += (size_t)bits;

			// Check
			printf("D%zu %u <- %i bits\t(%.2f total bytes)\n", i, value, bits, (float)total_bits / 8.0F);
			assert(value == data_callback(i, prev_value, callback_data));
			prev_value = value;
		}

		printf("\n");
	}

	free(buffer);
}


uint16_t sCallbackLinear(size_t i, uint16_t prev, uint16_t callback_data)
{
	return (uint16_t)(i + (size_t)callback_data);
}

uint16_t sCallbackConstant(size_t i, uint16_t prev, uint16_t callback_data)
{
	return callback_data;
}

uint16_t sCallbackRandom(size_t i, uint16_t prev, uint16_t callback_data)
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
