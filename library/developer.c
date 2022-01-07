/*

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
*/


#include "ako-private.h"


#if (AKO_FREESTANDING == 0)
void akoSavePgmU8(size_t width, size_t height, size_t in_stride, const uint8_t* in, const char* filename)
{
	FILE* fp = fopen(filename, "wb");
	if (fp != NULL)
	{
		fprintf(fp, "P5\n%zu\n%zu\n255\n", width, height);

		for (size_t row = 0; row < height; row++)
			fwrite(in + (row * in_stride), 1, width, fp);

		fclose(fp);
	}
}


void akoSavePgmI16(size_t width, size_t height, size_t in_stride, const int16_t* in, const char* filename)
{
	FILE* fp = fopen(filename, "wb");
	if (fp != NULL)
	{
		fprintf(fp, "P5\n%zu\n%zu\n255\n", width, height);

		for (size_t row = 0; row < height; row++)
			for (size_t col = 0; col < width; col++)
			{
				const int16_t i16 = in[(row * in_stride) + col];
				const uint8_t u8 = (i16 > 0) ? (i16 < 255) ? (uint8_t)i16 : 255 : 0;
				fwrite(&u8, 1, 1, fp);
			}

		fclose(fp);
	}
}
#endif
