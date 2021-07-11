/*-----------------------------

 [developer.h]
 - Alexander Brandt 2021
-----------------------------*/

#ifndef DEVELOPER_H
#define DEVELOPER_H

#include <time.h>

struct Benchmark
{
	clock_t start;
	clock_t elapsed;
	const char* name;
};

struct Benchmark DevBenchmarkCreate(const char* name);
void DevBenchmarkStartResume(struct Benchmark* benchmark);
void DevBenchmarkPause(struct Benchmark* benchmark);
void DevBenchmarkStop(struct Benchmark* benchmark);
void DevBenchmarkPrint(struct Benchmark* benchmark);

void DevSaveGrayPgm(size_t width, size_t height, size_t in_pitch, const int16_t* data, const char* filename_format,
                    ...);
void DevPrintf(const char* format, ...);

#endif
