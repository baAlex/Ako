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

 [developer.c]
 - Alexander Brandt 2021
-----------------------------*/

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "ako.h"


#if (AKO_DEV_TINY_BENCHMARK == 1)

static clock_t s_benchmark_start;
static const char* s_benchmark_name;
static clock_t s_benchmark_total;

void DevBenchmarkStart(const char* name)
{
	s_benchmark_start = clock();
	s_benchmark_name = name;
}

void DevBenchmarkStop()
{
	clock_t current = clock();
	s_benchmark_total = s_benchmark_total + (current - s_benchmark_start);

	double ms = ((double)(current - s_benchmark_start) / (double)CLOCKS_PER_SEC) * 1000.0f;
	double sec = ((double)(current - s_benchmark_start) / (double)CLOCKS_PER_SEC);

	if (ms < 1000.0f)
		printf("%.2f\tms\t%s\n", ms, s_benchmark_name);
	else
		printf("%.2f\tsec\t%s\n", sec, s_benchmark_name);
}

void DevBenchmarkTotal()
{
	printf("-----\n");

	double ms = ((double)s_benchmark_total / (double)CLOCKS_PER_SEC) * 1000.0f;
	double sec = ((double)s_benchmark_total / (double)CLOCKS_PER_SEC);

	if (ms < 1000.0f)
		printf("%.2f\tms\tTotal\n\n", ms);
	else
		printf("%.2f\tsec\tTotal\n\n", sec);

	s_benchmark_total = 0;
}
#else
void DevBenchmarkStart(const char* name)
{
	(void)name;
}
void DevBenchmarkStop() {}
void DevBenchmarkTotal() {}
#endif


#if (AKO_DEV_SAVE_IMAGES == 1)
void DevSaveGrayPgm(size_t width, size_t height, const int16_t* data, const char* filename_format, ...)
{
	char filename[256];
	va_list args;

	va_start(args, filename_format);
	vsprintf(filename, filename_format, args);
	va_end(args);

	FILE* fp = fopen(filename, "wb");
	fprintf(fp, "P5\n%zu\n%zu\n255\n", width, height);

	for (size_t i = 0; i < (width * height); i++)
	{
		uint8_t p = (data[i] > 0) ? (data[i] < 255) ? (uint8_t)data[i] : 255 : 0;
		fwrite(&p, 1, 1, fp);
	}

	fclose(fp);
}
#else
void DevSaveGrayPgm(size_t width, size_t height, const int16_t* data, const char* filename_format, ...)
{
	(void)width;
	(void)height;
	(void)data;
	(void)filename_format;
}
#endif


#if (AKO_DEV_PRINTF_DEBUG == 1)
void DevPrintf(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}
#else
void DevPrintf(const char* format, ...)
{
	(void)format;
}
#endif
