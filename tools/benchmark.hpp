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


#ifndef BENCHMARK_HPP
#define BENCHMARK_HPP

#include <chrono>
#include <iostream>

extern "C"
{
#include "ako.h"
}


class Stopwatch
{
  private:
	std::chrono::steady_clock::time_point last_point;
	std::chrono::duration<double, std::milli> duration;

  public:
	void start(bool fresh_start)
	{
		last_point = std::chrono::steady_clock::now();

		if (fresh_start == true)
			duration = duration.zero();
	}

	void pause_stop(bool print, const std::string& name, const std::string& suffix = "")
	{
		const auto now = std::chrono::steady_clock::now();
		duration += (now - last_point);

		if (print == true)
			std::cout << name << duration.count() << " ms" << suffix << std::endl;
	}
};


struct EventsData
{
	Stopwatch format;
	Stopwatch wavelet;
	Stopwatch compression;
};


void EventsCallback(size_t tile_no, size_t total_tiles, akoEvent e, void* raw_data)
{
	EventsData* stopwatches = (EventsData*)raw_data;

	if (e == AKO_EVENT_FORMAT_START)
		stopwatches->format.start((bool)(tile_no == 0));
	else if (e == AKO_EVENT_WAVELET_START)
		stopwatches->wavelet.start((bool)(tile_no == 0));
	else if (e == AKO_EVENT_COMPRESSION_START)
		stopwatches->compression.start((bool)(tile_no == 0));

	else if (e == AKO_EVENT_FORMAT_END)
		stopwatches->format.pause_stop((bool)(tile_no == total_tiles - 1), " - Format: ");
	else if (e == AKO_EVENT_WAVELET_END)
		stopwatches->wavelet.pause_stop((bool)(tile_no == total_tiles - 1), " - Wavelet transformation: ");
	else if (e == AKO_EVENT_COMPRESSION_END)
		stopwatches->compression.pause_stop((bool)(tile_no == total_tiles - 1), " - Compression: ");
}

#endif
