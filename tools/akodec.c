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

 [akodec.c]
 - Alexander Brandt 2021
-----------------------------*/

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <png.h>
#include <zlib.h>

#include "ako.h"


static void sPngErrorHandler(png_structp pngs, png_const_charp msg)
{
	(void)pngs;
	printf("LibPng error: \"%s\"\n", msg);
	exit(1); // Bye cruel world!
}


void ImageSavePng(size_t dimension, size_t channels, const uint8_t* data, const char* filename)
{
	FILE* fp = fopen(filename, "wb");
	assert(fp != NULL);

	// Init things
	png_struct* pngs = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, sPngErrorHandler, NULL);
	assert(pngs != NULL);
	png_info* pngi = png_create_info_struct(pngs);
	assert(pngi != NULL);

	png_init_io(pngs, fp);

	// Configure
	png_set_filter(pngs, 0, PNG_ALL_FILTERS);
	png_set_compression_level(pngs, Z_BEST_COMPRESSION);

	int bit_depth = 8;
	int color_type = 0;

	switch (channels)
	{
	case 1: color_type = PNG_COLOR_TYPE_GRAY; break;
	case 2: color_type = PNG_COLOR_TYPE_GRAY_ALPHA; break;
	case 3: color_type = PNG_COLOR_TYPE_RGB; break;
	case 4: color_type = PNG_COLOR_TYPE_RGB_ALPHA; break;
	default: assert(1 == 0);
	}

	png_set_IHDR(pngs, pngi, (png_uint_32)dimension, (png_uint_32)dimension, bit_depth, color_type, PNG_INTERLACE_NONE,
	             PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	// Write
	png_bytepp row_pointers = (png_bytepp)png_malloc(pngs, dimension * sizeof(png_bytep));

	for (size_t i = 0; i < dimension; i++)
		row_pointers[i] = (unsigned char*)(data + i * dimension * channels);

	png_set_rows(pngs, pngi, row_pointers);
	png_write_png(pngs, pngi, PNG_TRANSFORM_IDENTITY, NULL);

	// Bye!
	png_free(pngs, row_pointers);
	png_destroy_info_struct(pngs, &pngi);
	png_destroy_write_struct(&pngs, NULL);
	fclose(fp);
}


static int sReadArguments(int argc, const char* argv[], const char** out_input_filename,
                          const char** out_output_filename)
{
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "-input") == 0)
		{
			if ((i = i + 1) == argc)
			{
				fprintf(stderr, "Missing input filename\n");
				return 1;
			}

			*out_input_filename = argv[i];
		}
		else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "-output") == 0)
		{
			if ((i = i + 1) == argc)
			{
				fprintf(stderr, "Missing output filename\n");
				return 1;
			}

			*out_output_filename = argv[i];
		}
		else
		{
			fprintf(stderr, "Unknown argument '%s'\n", argv[i]);
			return 1;
		}
	}

	if (*out_input_filename == NULL)
	{
		fprintf(stderr, "No input filename specified\n");
		return 1;
	}

	if (*out_output_filename == NULL)
	{
		fprintf(stderr, "No output filename specified\n");
		return 1;
	}

	return 0;
}


int main(int argc, const char* argv[])
{
	const char* input_filename = NULL;
	const char* output_filename = NULL;

	if (sReadArguments(argc, argv, &input_filename, &output_filename) != 0)
		return EXIT_FAILURE;

	// Load file blob
	FILE* fp = NULL;
	long blob_size = 0;
	void* blob = NULL;

	fp = fopen(input_filename, "rb");
	assert(fp != NULL);

	fseek(fp, 0, SEEK_END);
	blob_size = ftell(fp);
	blob = malloc((size_t)blob_size);
	assert(blob != NULL);

	fseek(fp, 0, SEEK_SET);
	fread(blob, (size_t)blob_size, 1, fp);

	// Decode Ako
	size_t dimension = 0;
	size_t channels = 0;
	void* ako = NULL;

	ako = AkoDecode((size_t)blob_size, blob, &dimension, &channels);
	assert(ako != NULL);

	// Save Png
	ImageSavePng(dimension, channels, ako, output_filename);

	// Bye!
	fclose(fp);
	free(ako);
	free(blob);

	return 0;
}
