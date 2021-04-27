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
#include <stdbool.h>
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
	fprintf(stderr, "LibPng error: \"%s\"\n", msg);
	exit(EXIT_FAILURE); // Bye cruel world!
}


void ImageSavePng(size_t dimension, size_t channels, const uint8_t* data, FILE* fp)
{
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
}


struct DecoderSettings
{
	const char* input_filename;
	const char* output_filename;
	bool print_version;
	bool use_stdout;
};

static int sReadArguments(int argc, const char* argv[], struct DecoderSettings* deco_s)
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

			deco_s->input_filename = argv[i];
		}
		else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "-output") == 0)
		{
			if ((i = i + 1) == argc)
			{
				fprintf(stderr, "Missing output filename\n");
				return 1;
			}

			deco_s->output_filename = argv[i];
		}
		else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0)
		{
			deco_s->print_version = true;
		}
		else if (strcmp(argv[i], "-stdout") == 0 || strcmp(argv[i], "--stdout") == 0)
		{
			deco_s->use_stdout = true;
		}
		else
		{
			fprintf(stderr, "Unknown argument '%s'\n", argv[i]);
			return 1;
		}
	}

	// These two are mandatory!
	if (deco_s->print_version == false)
	{
		if (deco_s->input_filename == NULL)
		{
			fprintf(stderr, "No input filename specified\n");
			return 1;
		}

		if (deco_s->output_filename == NULL && deco_s->use_stdout == false)
		{
			fprintf(stderr, "No output filename specified\n");
			return 1;
		}
	}

	return 0;
}


static const char* s_usage_message = "\
Usage: akodec [options] -i <input filename> -o <output filename>\n\
Usage: akodec [options] -i <input filename> -stdout\n\
\n\
Ako decoding tool.\n\
\n\
General options:\n\
 -v, --version      Show version information\n\
";

int main(int argc, const char* argv[])
{
	struct DecoderSettings deco_s = {0};

	if (argc == 1)
	{
		printf("%s\n", s_usage_message);
		return EXIT_SUCCESS;
	}

	if (sReadArguments(argc, argv, &deco_s) != 0)
		return EXIT_FAILURE;

	if (deco_s.print_version == true)
	{
		printf("Ako decoding tool.\n");
		printf(" - %s, format %u\n", AkoVersionString(), AKO_FORMAT);
		printf(" - libpng %s (%u)\n", PNG_LIBPNG_VER_STRING, png_access_version_number());
		printf("\n");
		printf("Copyright (c) 2021 Alexander Brandt\n");
		return EXIT_SUCCESS;
	}

	// Load file blob
	FILE* fp = NULL;
	long blob_size = 0;
	void* blob = NULL;

	fp = fopen(deco_s.input_filename, "rb");
	assert(fp != NULL);

	fseek(fp, 0, SEEK_END);
	blob_size = ftell(fp);
	blob = malloc((size_t)blob_size);
	assert(blob != NULL);

	fseek(fp, 0, SEEK_SET);
	fread(blob, (size_t)blob_size, 1, fp);
	fclose(fp);

	// Decode Ako
	size_t dimension = 0;
	size_t channels = 0;
	void* ako = NULL;

	ako = AkoDecode((size_t)blob_size, blob, &dimension, &channels);
	assert(ako != NULL);

	// Save Png
	if (deco_s.use_stdout == false)
	{
		fp = fopen(deco_s.output_filename, "wb");
		assert(fp != NULL);

		ImageSavePng(dimension, channels, ako, fp);
		fclose(fp);
	}
	else
		ImageSavePng(dimension, channels, ako, stdout);

	// Bye!
	free(ako);
	free(blob);

	return 0;
}
