

#undef NDEBUG

#include "ako-private.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>


static void sSingleTest(size_t(encode_callback)(size_t, size_t, size_t, size_t, const ucoeff_t*, void*),
                        size_t(decode_callback)(size_t, size_t, size_t, const void*, ucoeff_t*), size_t blocks_len,
                        size_t in_len, const ucoeff_t* in)
{
	printf("(len: %zu) ", in_len);
	for (size_t i = 0; i < in_len; i++)
		printf("%u, ", in[i]);
	printf("\n");

	const size_t buffer_size = sizeof(ucoeff_t) * in_len * 2;
	ucoeff_t* buffer_a = malloc(buffer_size);
	ucoeff_t* buffer_b = malloc(buffer_size);
	assert(buffer_a != NULL);
	assert(buffer_b != NULL);

	// Encode
	size_t compressed_size;
	{
		compressed_size = encode_callback(blocks_len, AKO_RLE_DEFAULT_TRIGGER_DELAY, sizeof(ucoeff_t) * in_len,
		                                  buffer_size, in, buffer_a);

		assert(compressed_size > 1);
		assert((compressed_size % 2) == 0);

		printf("(len: %zu) ", compressed_size / 2);
		for (size_t i = 0; i < compressed_size / 2; i++)
			printf("%u, ", buffer_a[i]);
		printf("\n");
	}

	// Decode
	{
		const size_t decompressed_size =
		    decode_callback(blocks_len, compressed_size, sizeof(ucoeff_t) * in_len, buffer_a, buffer_b);

		assert(decompressed_size != 0);
		assert((decompressed_size % 2) == 0);

		printf("(len: %zu) ", decompressed_size / 2);
		for (size_t i = 0; i < decompressed_size / 2; i++)
			printf("%u, ", buffer_b[i]);
		printf("\n");
	}

	// Test
	for (size_t i = 0; i < in_len; i++)
		assert(in[i] == buffer_b[i]);

	// Bye!
	printf("\n");
	free(buffer_a);
	free(buffer_b);
}


static int sFileTest(size_t(encode_callback)(size_t, size_t, size_t, size_t, const ucoeff_t*, void*),
                     size_t(decode_callback)(size_t, size_t, size_t, const void*, ucoeff_t*), int argc,
                     const char* argv[])
{
	clock_t start;
	clock_t end;

	// Read file
	FILE* input_fp = fopen(argv[2], "rb");
	assert(input_fp != NULL);

	fseek(input_fp, 0, SEEK_END);
	const long input_size = ftell(input_fp);
	assert(input_size != -1L);

	printf("'%s': %.2f Kb -> ", argv[2], (double)input_size / 1000.0);

	void* buffer1 = malloc(input_size);
	void* buffer2 = malloc(input_size);
	void* buffer3 = malloc(input_size);
	assert(buffer1 != NULL);
	assert(buffer2 != NULL);
	assert(buffer3 != NULL);

	fseek(input_fp, 0, SEEK_SET);
	if (fread(buffer1, input_size, 1, input_fp) != 1)
		assert(1 == 2);

	// Encode
	akoZigZagToUnsigned(input_size / sizeof(ucoeff_t), buffer1);

	start = clock();
	const size_t compressed_size = encode_callback(AKO_RLE_BLOCKS_LEN_MAX, AKO_RLE_DEFAULT_TRIGGER_DELAY, input_size,
	                                               input_size, buffer1, buffer2);
	end = clock();

	printf("%.2f Kb, ratio: %.2f:1, encoding: %.2f ms, ", (double)compressed_size / 1000.0,
	       (double)input_size / (double)compressed_size, (double)(end - start) / (double)(CLOCKS_PER_SEC / 1000.0));
	assert(compressed_size != 0);

	// Decode
	start = clock();
	const size_t decompressed_size =
	    decode_callback(AKO_RLE_BLOCKS_LEN_MAX, compressed_size, input_size, buffer2, buffer3);
	end = clock();

	printf("decoding: %.2f ms\n", (double)(end - start) / (double)(CLOCKS_PER_SEC / 1000.0));
	assert(decompressed_size != 0);

	akoZigZagToSigned(input_size / sizeof(ucoeff_t), buffer3);

	// Save decoded file
	if (argc > 3)
	{
		FILE* output_fp = fopen(argv[3], "wb");
		fwrite(buffer3, decompressed_size, 1, output_fp);
		fclose(output_fp);
	}

	// Bye!
	fclose(input_fp);
	free(buffer1);
	free(buffer2);
	free(buffer3);

	return 0;
}


void sGenericTests(size_t(encode_callback)(size_t, size_t, size_t, size_t, const ucoeff_t*, void*),
                   size_t(decode_callback)(size_t, size_t, size_t, const void*, ucoeff_t*))
{
	const ucoeff_t data1[10] = {1, 2, 3, 4, 5, 6, 6, 6, 6, 6};
	sSingleTest(encode_callback, decode_callback, AKO_RLE_BLOCKS_LEN_MAX, 10, data1);

	const ucoeff_t data2[16] = {1, 2, 3, 4, 5, 6, 7, 8, 2, 2, 2, 2, 1, 2, 3, 4};
	sSingleTest(encode_callback, decode_callback, AKO_RLE_BLOCKS_LEN_MAX, 16, data2);

	const ucoeff_t data3[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8};
	sSingleTest(encode_callback, decode_callback, AKO_RLE_BLOCKS_LEN_MAX, 16, data3);

	const ucoeff_t data4[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
	sSingleTest(encode_callback, decode_callback, AKO_RLE_BLOCKS_LEN_MAX, 16, data4);

	const ucoeff_t data5[1] = {5};
	sSingleTest(encode_callback, decode_callback, AKO_RLE_BLOCKS_LEN_MAX, 1, data5);

	const ucoeff_t data6[2] = {5, 6};
	sSingleTest(encode_callback, decode_callback, AKO_RLE_BLOCKS_LEN_MAX, 2, data6);

	// Test overflow mechanism, limiting rle and literal sequences to a length of 4
	// To avoid overflows/wrap-arounds the encoder will split sequences and emit literals of length zero to mark stops.
	// This indeed ruins compression but in real world sequences should had a maximum length of 2^16

	const ucoeff_t data7[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
	sSingleTest(encode_callback, decode_callback, 4, 16, data7);

	const ucoeff_t data8[16] = {1, 2, 3, 4, 4, 4, 4, 8, 9, 10, 11, 12, 13, 14, 15, 16};
	sSingleTest(encode_callback, decode_callback, 4, 16, data8);

	const ucoeff_t data9[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
	sSingleTest(encode_callback, decode_callback, 4, 16, data9);

	const ucoeff_t data10[18] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
	sSingleTest(encode_callback, decode_callback, 4, 17, data10);

	const ucoeff_t data11[18] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
	sSingleTest(encode_callback, decode_callback, 4, 18, data11);
}


int main(int argc, const char* argv[])
{
	if (argc > 2)
	{
		if (argv[1][0] == 'n')
		{
			printf("# NEW RLE:\n");
			return sFileTest(akoRleEncode, akoRleDecode, argc, argv);
		}
		else if (argv[1][0] == 'o')
		{
			printf("# OLD RLE:\n");
			return sFileTest(akoOldRleEncode, akoOldRleDecode, argc, argv);
		}
	}

	printf("# NEW RLE:\n\n");
	sGenericTests(akoRleEncode, akoRleDecode);

	printf("# OLD RLE:\n\n");
	sGenericTests(akoOldRleEncode, akoOldRleDecode);

	return 0;
}
