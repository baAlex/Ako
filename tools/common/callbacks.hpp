/*

MIT License

Copyright (c) 2021-2023 Alexander Brandt

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

#ifndef CALLBACKS_HPP
#define CALLBACKS_HPP

#include <chrono>
#include <cstdint>
#include <string>

#include "ako.hpp"

struct CallbacksData
{
	bool print;
	const char* prefix;
	std::string histogram_output;

	unsigned print_tile_info;

	std::chrono::steady_clock::time_point clock;
	std::chrono::microseconds format_duration;
	std::chrono::microseconds lifting_duration;
	std::chrono::microseconds compression_duration;
	std::chrono::microseconds total_duration;

	unsigned image_events;
	unsigned image_width;
	unsigned image_height;
	unsigned channels;
	unsigned depth;

	unsigned tiles_events;
	unsigned tiles_no;
	unsigned tiles_dimension;

	unsigned memory_events;
	size_t work_area_size;

	unsigned current_tile;
	unsigned tile_width;
	unsigned tile_height;
	unsigned tile_x;
	unsigned tile_y;
	size_t tile_data_size;
};

void CallbackGenericEvent(ako::GenericEvent e, unsigned a, unsigned b, unsigned c, ako::GenericType d, void* user_data);

void CallbackFormatEvent(unsigned tile_no, ako::Color, const void* image_data, void* user_data);
void CallbackLiftingEvent(unsigned tile_no, ako::Wavelet, ako::Wrap, const void* image_data, void* user_data);
void CallbackCompressionEvent(unsigned tile_no, ako::Compression, const void* data, void* user_data);
void CallbackHistogramEvent(const ako::Histogram*, unsigned, void* user_data);

#endif
