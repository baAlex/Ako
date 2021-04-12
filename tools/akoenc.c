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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <png.h>
#include <zlib.h>

#include "ako.h"


extern void DevBenchmarkStart(const char* name);
extern void DevBenchmarkStop();
extern void DevBenchmarkTotal();


static void sPngErrorHandler(png_structp pngs, png_const_charp msg)
{
	(void)pngs;
	printf("LibPng error: \"%s\"\n", msg);
	exit(1); // Bye cruel world!
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


static int sReadArguments(int argc, const char* argv[], const char** out_input_filename,
                          const char** out_output_filename, struct AkoSettings* out_settings)
{
	int setting_set = 0;
	float quality_ratio = 0.0f;

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
		else if (strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "-gate") == 0 || strcmp(argv[i], "-detailgate") == 0)
		{
			if ((i = i + 1) == argc)
			{
				fprintf(stderr, "Missing detail gate value\n");
				return 1;
			}

			unsigned long temp = strtoul(argv[i], NULL, 10);
			temp = (temp > UINT32_MAX) ? UINT32_MAX : temp;
			out_settings->detail_gate[0] = (float)temp;
			out_settings->detail_gate[1] = (float)temp;
			out_settings->detail_gate[2] = (float)temp;
			out_settings->detail_gate[3] = (float)temp;
			setting_set++;

			printf("Detail gate: %lu px\n", temp);
		}
		else if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "-uniform") == 0 || strcmp(argv[i], "-uniformgate") == 0)
		{
			if ((i = i + 1) == argc)
			{
				fprintf(stderr, "Missing uniform gate value\n");
				return 1;
			}

			unsigned long temp = strtoul(argv[i], NULL, 10);
			temp = (temp > UINT32_MAX) ? UINT32_MAX : temp;
			out_settings->uniform_gate[0] = (float)temp;
			out_settings->uniform_gate[1] = (float)temp;
			out_settings->uniform_gate[2] = (float)temp;
			out_settings->uniform_gate[3] = (float)temp;
			setting_set++;

			printf("Uniform gate: %lu px\n", temp);
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

	if (setting_set != 0)
		printf("\n");

	return 0;
}


int main(int argc, const char* argv[])
{
	const char* input_filename = NULL;
	const char* output_filename = NULL;

	struct AkoSettings settings = {.uniform_gate = {0.0f, 0.0f, 0.0f, 0.0f}, // Default settings
	                               .detail_gate = {16.0f, 16.0f, 16.0f, 16.0f}};

	if (sReadArguments(argc, argv, &input_filename, &output_filename, &settings) != 0)
		return EXIT_FAILURE;

	// Load Png
	size_t dimension = 0;
	size_t channels = 0;
	void* png = NULL;

	DevBenchmarkStart("LibPng load (from file API)");
	png = sImageLoadPng(input_filename, &dimension, &channels);
	assert(png != NULL);
	DevBenchmarkStop();
	DevBenchmarkTotal();

	// Save Ako
	uint8_t* blob = NULL;
	size_t blob_size = 0;

	blob_size = AkoEncode(dimension, channels, &settings, png, &blob);
	assert(blob_size != 0);

	FILE* fp = fopen(output_filename, "wb");
	assert(fp != NULL);

	fwrite(blob, blob_size, 1, fp);
	printf("%.2f kB -> %.2f kB, %.2f%%\n\n",
	       (double)(dimension * dimension * channels + sizeof(struct AkoHead)) / 1000.0f, (double)blob_size / 1000.0f,
	       (double)(dimension * dimension * channels + sizeof(struct AkoHead)) / (double)blob_size);

	// Bye!
	fclose(fp);
	free(png);
	free(blob);

	return EXIT_SUCCESS;
}
