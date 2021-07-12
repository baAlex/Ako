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

#include "ako.h"
#include "developer.h"


#if (AKO_DEV_TINY_BENCHMARK == 1)
inline struct Benchmark DevBenchmarkCreate(const char* name)
{
	return (struct Benchmark){.start = 0, .elapsed = 0, .name = name};
}

inline void DevBenchmarkStartResume(struct Benchmark* benchmark)
{
	if (benchmark->start == 0)
		benchmark->start = clock();
}

inline void DevBenchmarkPause(struct Benchmark* benchmark)
{
	if (benchmark->start != 0)
	{
		benchmark->elapsed = benchmark->elapsed + (clock() - benchmark->start);
		benchmark->start = 0;
	}
}

inline void DevBenchmarkStop(struct Benchmark* benchmark)
{
	if (benchmark->start != 0)
	{
		benchmark->elapsed = (clock() - benchmark->start);
		benchmark->start = 0;
	}
}

inline void DevBenchmarkPrint(struct Benchmark* benchmark)
{
	DevBenchmarkStop(benchmark);

	const double ms = (double)benchmark->elapsed / ((double)CLOCKS_PER_SEC / 1000.0f);
	const double sec = (double)benchmark->elapsed / (double)CLOCKS_PER_SEC;

	if (ms < 1000.0f)
		printf("###\t%7.2f ms | %s\n", ms, benchmark->name);
	else
		printf("###\t%7.2f s  | %s\n", sec, benchmark->name);
}
#else
inline struct Benchmark DevBenchmarkCreate(const char* name)
{
	(void)name;
	return (struct Benchmark){0};
}

inline void DevBenchmarkStartResume(struct Benchmark* benchmark)
{
	(void)benchmark;
}

inline void DevBenchmarkPause(struct Benchmark* benchmark)
{
	(void)benchmark;
}

inline void DevBenchmarkStop(struct Benchmark* benchmark)
{
	(void)benchmark;
}

inline void DevBenchmarkPrint(struct Benchmark* benchmark)
{
	(void)benchmark;
}
#endif


#if (AKO_DEV_SAVE_IMAGES == 1)
void DevSaveGrayPgm(size_t width, size_t height, size_t in_pitch, const int16_t* data, const char* filename_format, ...)
{
	char filename[256];
	va_list args;

	va_start(args, filename_format);
	vsprintf(filename, filename_format, args);
	va_end(args);

	FILE* fp = fopen(filename, "wb");
	fprintf(fp, "P5\n%zu\n%zu\n255\n", width, height);

	for (size_t row = 0; row < height; row++)
		for (size_t col = 0; col < width; col++)
		{
			const int16_t* in = &data[(row * in_pitch) + col];
			uint8_t p = (*in > 0) ? (*in < 255) ? (uint8_t)*in : 255 : 0;
			fwrite(&p, 1, 1, fp);
		}

	fclose(fp);
}
#else
void DevSaveGrayPgm(size_t width, size_t height, size_t in_pitch, const int16_t* data, const char* filename_format, ...)
{
	(void)width;
	(void)height;
	(void)in_pitch;
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
