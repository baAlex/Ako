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

 [akoenc.c]
 - Alexander Brandt 2021
-----------------------------*/

#include <assert.h>
#include <math.h>
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


static uint8_t* sImageLoadPng(const char* filename, size_t* out_dimension, size_t* out_channels)
{
	FILE* fp = fopen(filename, "rb");
	assert(fp != NULL);

	// Init things
	png_struct* pngs = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, sPngErrorHandler, NULL);
	assert(pngs != NULL);
	png_info* pngi = png_create_info_struct(pngs);
	assert(pngi != NULL);

	png_init_io(pngs, fp);

	// Read head/info
	png_uint_32 width;
	png_uint_32 height;
	png_uint_32 channels;
	int bit_depth;
	int color_type;

	png_read_info(pngs, pngi); // Reads everything except pixel data
	png_get_IHDR(pngs, pngi, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);
	assert(width == height);

	switch (color_type)
	{
	case PNG_COLOR_TYPE_GRAY: channels = 1; break;
	case PNG_COLOR_TYPE_GRAY_ALPHA: channels = 2; break;
	case PNG_COLOR_TYPE_RGB: channels = 3; break;
	case PNG_COLOR_TYPE_RGB_ALPHA: channels = 4; break;

	case PNG_COLOR_TYPE_PALETTE: assert(1 == 0);
	default: assert(1 == 0);
	}

	// Transformations
	if (bit_depth < 8)
		png_set_packing(pngs); // Convert from u1, u2, u4 to u8
	if (bit_depth > 8)
		png_set_strip_16(pngs); // Converts u16 to u8

	if (png_get_valid(pngs, pngi, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(pngs); // Applies transparency defined in the metadata (chunks)

	png_read_update_info(pngs, pngi); // Applies transformations?, configures the decoder? (?)

	// Alloc image
	uint8_t* image = malloc(width * height * channels);
	assert(image != NULL);

	if (out_dimension != NULL)
		*out_dimension = (size_t)width;
	if (out_channels != NULL)
		*out_channels = (size_t)channels;

	// Read data
	png_bytepp row_pointers = (png_bytepp)png_malloc(pngs, height * sizeof(png_bytep));

	for (size_t i = 0; i < height; i++)
		row_pointers[i] = (image + (i * width * channels));

	png_read_image(pngs, row_pointers);

	// Bye!
	png_free(pngs, row_pointers);
	png_destroy_info_struct(pngs, &pngi);
	png_destroy_read_struct(&pngs, NULL, NULL);
	fclose(fp);

	return image;
}


struct EncoderSettings
{
	const char* input_filename;
	const char* output_filename;
	bool print_version;
	bool quiet;
	bool use_stdout;
};

static int sReadArguments(int argc, const char* argv[], struct EncoderSettings* enco_s, struct AkoSettings* codec_s)
{
	bool gate_set = false;
	bool ratio_set = false;
	float ratio = 1.0f;

	// Read/set arguments
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--input") == 0)
		{
			if ((i = i + 1) == argc)
			{
				fprintf(stderr, "Missing input filename\n");
				return 1;
			}

			enco_s->input_filename = argv[i];
		}
		else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0)
		{
			if ((i = i + 1) == argc)
			{
				fprintf(stderr, "Missing output filename\n");
				return 1;
			}

			enco_s->output_filename = argv[i];
		}
		else if (strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "--gate") == 0)
		{
			if ((i = i + 1) == argc)
			{
				fprintf(stderr, "Missing gate threshold\n");
				return 1;
			}

			gate_set = true;

			float temp = strtof(argv[i], NULL);
			codec_s->detail_gate[0] = temp;
			codec_s->detail_gate[1] = temp;
			codec_s->detail_gate[2] = temp;
			codec_s->detail_gate[3] = temp;
		}
		else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--ratio") == 0)
		{
			if ((i = i + 1) == argc)
			{
				fprintf(stderr, "Missing luma/chroma gate ratio\n");
				return 1;
			}

			ratio_set = true;

			if (fabsf(ratio = strtof(argv[i], NULL)) < 1.0f)
				ratio = 1.0f;
		}
		else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0)
		{
			enco_s->print_version = true;
		}
		else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0)
		{
			enco_s->quiet = true;
		}
		else if (strcmp(argv[i], "-stdout") == 0 || strcmp(argv[i], "--stdout") == 0)
		{
			enco_s->use_stdout = true;
		}
		else
		{
			fprintf(stderr, "Unknown argument '%s'\n", argv[i]);
			return 1;
		}
	}

	// These two are mandatory!
	if (enco_s->print_version == false)
	{
		if (enco_s->input_filename == NULL)
		{
			fprintf(stderr, "No input filename specified\n");
			return 1;
		}

		if (enco_s->output_filename == NULL && enco_s->use_stdout == false)
		{
			fprintf(stderr, "No output filename specified\n");
			return 1;
		}
	}

	// Print settings
	if (enco_s->quiet == false && enco_s->use_stdout == false)
	{
		if (gate_set == true)
			printf("Gate threshold: %.0f\n", codec_s->detail_gate[0]);

		if (ratio_set == true)
		{
			if (ratio > 0.0f)
				printf("Luma/chroma gate ratio: 1:%.2f\n", ratio);
			else
				printf("Luma/chroma gate ratio: %.2f:1\n", fabsf(ratio));
		}

		if (gate_set || ratio_set)
			printf("\n");
	}

	// Ratio is not part of the codec settings,
	// is manufactured by us (the encoder)
	if (ratio > 0.0f)
	{
		codec_s->detail_gate[1] = codec_s->detail_gate[1] * ratio;
		codec_s->detail_gate[2] = codec_s->detail_gate[2] * ratio;
	}
	else
	{
		codec_s->detail_gate[0] = codec_s->detail_gate[0] * fabsf(ratio);
		codec_s->detail_gate[3] = codec_s->detail_gate[3] * fabsf(ratio);
	}

	return 0;
}


static const char* s_usage_message = "\
Usage: akoenc [options] -i <input filename> -o <output filename>\n\
Usage: akoenc [options] -i <input filename> -stdout\n\
\n\
Ako encoding tool.\n\
\n\
General options:\n\
 -v, --version      Show version information\n\
 -q, --quiet        Quiet mode\n\
\n\
Encoding options:\n\
 -g, --gate n       Gate threshold, controls quality/size by discarding\n\
                    coefficients below the specified value.\n\
                    Examples:\n\
                    '-g 0'    Near lossless quality, poor compression\n\
                    '-g 16'   Good quality/size compromise (sic.) (default)\n\
\n\
 -r, --ratio n      Luma/chroma gate ratio, controls how the gate threshold\n\
                    affects components independently. Can be used to emulate\n\
                    a form of chroma subsampling.\n\
                    Examples:\n\
                    '-r 1'    Luma/chroma with equal gate thresholds (default)\n\
                    '-r 4'    Chroma gate threshold multiplied by four\n\
                    '-r 2.5'  Chroma gate threshold multiplied by two and half\n\
";

int main(int argc, const char* argv[])
{
	struct EncoderSettings enco_s = {0};
	struct AkoSettings codec_s = {.detail_gate = {16.0f, 16.0f, 16.0f, 16.0f}, // Default settings
	                              .limit = {0, 0, 0, 0}};

	if (argc == 1)
	{
		printf("%s\n", s_usage_message);
		return EXIT_SUCCESS;
	}

	if (sReadArguments(argc, argv, &enco_s, &codec_s) != 0)
		return EXIT_FAILURE;

	if (enco_s.print_version == true)
	{
		printf("Ako encoding tool.\n");
		printf(" - %s, format %u\n", AkoVersionString(), AKO_FORMAT);
		printf(" - libpng %s (%u)\n", PNG_LIBPNG_VER_STRING, png_access_version_number());
		printf("\n");
		printf("Copyright (c) 2021 Alexander Brandt\n");
		return EXIT_SUCCESS;
	}

	// Load Png
	size_t dimension = 0;
	size_t channels = 0;
	void* png = NULL;

	png = sImageLoadPng(enco_s.input_filename, &dimension, &channels);
	assert(png != NULL);

	// Save Ako
	void* blob = NULL;
	size_t blob_size = 0;

	blob_size = AkoEncode(dimension, channels, &codec_s, png, &blob);
	assert(blob_size != 0);

	if (enco_s.use_stdout == false)
	{
		FILE* fp = fopen(enco_s.output_filename, "wb");
		assert(fp != NULL);

		fwrite(blob, blob_size, 1, fp);
		fclose(fp);

		// Print little info
		if (enco_s.quiet == false)
		{
			printf("%.2f kB -> %.2f kB, %.2f%%\n\n",
			       (double)(dimension * dimension * channels + sizeof(struct AkoHead)) / 1000.0f,
			       (double)blob_size / 1000.0f,
			       (double)(dimension * dimension * channels + sizeof(struct AkoHead)) / (double)blob_size);
		}
	}
	else
		fwrite(blob, blob_size, 1, stdout);

	// Bye!
	free(png);
	free(blob);

	return EXIT_SUCCESS;
}
