

#ifndef MODERN_APP_CPP
#define MODERN_APP_CPP

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#include "thirdparty/lodepng.h"

extern "C"
{
#include "ako.h"
}


#define TOOLS_VERSION_MAJOR 0
#define TOOLS_VERSION_MINOR 2
#define TOOLS_VERSION_PATCH 0


namespace Modern // Cpp newbie here! (a sarcastic one)
{
struct Error
{
	std::string info;

	Error(const std::string info = "Error with no information provided")
	{
		this->info = info;
	}
};


inline int CheckArgumentSingle(const std::string arg1, const std::string arg2,
                               const std::vector<std::string>::const_iterator& it)
{
	if (*it == arg1 || *it == arg2)
		return 0;

	return 1;
}


inline int CheckArgumentPair(const std::string arg1, const std::string arg2,
                             std::vector<std::string>::const_iterator& it,
                             const std::vector<std::string>::const_iterator& it_end)
{
	if (*it == arg1 || *it == arg2)
	{
		if ((++it) != it_end)
			return 0;
		else
			throw Modern::Error("No value provided for '" + arg1 + "/" + arg2 + "'");
	}

	return 1;
}


inline void WriteBlob(const std::string filename, const void* blob, size_t size)
{
	auto fp = std::make_unique<std::fstream>(filename, std::ios::binary | std::ios_base::out);

	fp->write((char*)blob, size);

	if (fp->fail() == true)
		throw Modern::Error("Write error");
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

	void pause_stop(bool print, const char* name, const char* suffix = "")
	{
		const auto now = std::chrono::steady_clock::now();
		duration += (now - last_point); // std::chrono::duration<double, std::milli>

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


uint32_t Adler32(const uint8_t* data, size_t len)
{
	// Borrowed from LodePng
	uint32_t s1 = 1u & 0xffffu;
	uint32_t s2 = (1u >> 16u) & 0xffffu;

	while (len != 0u)
	{
		uint32_t i;

		// (Lode) At least 5552 sums can be done before the sums overflow, saving a lot of module divisions
		uint32_t amount = len > 5552u ? 5552u : len;
		len -= amount;

		for (i = 0; i != amount; ++i)
		{
			s1 += (*data++);
			s2 += s1;
		}

		s1 %= 65521u;
		s2 %= 65521u;
	}

	return (s2 << 16u) | s1;
}
} // namespace Modern

#endif
