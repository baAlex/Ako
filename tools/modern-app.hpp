

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
	if (filename != "")
	{
		auto fp = std::make_unique<std::fstream>(filename, std::ios::binary | std::ios_base::out);

		fp->write((char*)blob, size);

		if (fp->fail() == true)
			throw Modern::Error("Write error");
	}
}


struct EventsData
{
	std::chrono::steady_clock::time_point last_format;
	std::chrono::steady_clock::time_point last_compression;
	std::chrono::duration<double, std::milli> duration_format;
	std::chrono::duration<double, std::milli> duration_compression;
};


void EventsCallback(size_t tile_no, size_t total_tiles, akoEvent e, void* raw_data)
{
	EventsData* data = (EventsData*)raw_data;
	const auto now = std::chrono::steady_clock::now();

	// Accumulate durations, print if apropiate
	if (e == AKO_EVENT_FORMAT_END)
	{
		data->duration_format += (now - data->last_format); // std::chrono::duration<double, std::milli>

		if (tile_no == total_tiles - 1)
			std::cout << "Benchmark | Format:      " << data->duration_format.count() << " ms" << std::endl;
	}
	else if (e == AKO_EVENT_COMPRESSION_END)
	{
		data->duration_compression += (now - data->last_compression);

		if (tile_no == total_tiles - 1)
			std::cout << "Benchmark | Compression: " << data->duration_compression.count() << " ms" << std::endl;
	}

	// Start clocks, reset durations if apropiate
	else if (e == AKO_EVENT_FORMAT_START)
	{
		data->last_format = now;

		if (tile_no == 0)
			data->duration_format = data->duration_format.zero();
	}
	else if (e == AKO_EVENT_COMPRESSION_START)
	{
		data->last_compression = now;

		if (tile_no == 0)
			data->duration_compression = data->duration_compression.zero();
	}
}
} // namespace Modern

#endif
