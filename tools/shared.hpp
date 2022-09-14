/*

MIT License

Copyright (c) 2021-2022 Alexander Brandt

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

#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "ako.hpp"

#include "CLI/CLI.hpp"
#include "lodepng.h"


#ifndef SHARED_HPP
#define SHARED_HPP


inline int ReadFile(const std::string& input_filename, std::vector<uint8_t>& out)
{
	// Open file
	FILE* fp = fopen(input_filename.c_str(), "rb"); // Only C let me use the seek/tell hack, in C++ is unspecified
	if (fp == nullptr)
	{
		std::cout << "Failed to open '" << input_filename << "'\n";
		return 1;
	}

	// Get file size
	fseek(fp, 0, SEEK_END);
	const auto size = ftell(fp);

	// Read
	out.resize(static_cast<size_t>(size));

	fseek(fp, 0, SEEK_SET);
	if (fread(out.data(), static_cast<size_t>(size), 1, fp) != 1)
	{
		std::cout << "Read error\n";
		return 1;
	}

	// Bye!
	fclose(fp);
	return 0;
}


inline int WriteFile(const std::string& filename, size_t size, const void* data)
{
	FILE* fp = fopen(filename.c_str(), "wb");
	if (fp == nullptr)
	{
		std::cout << "Failed to create '" << filename << "'\n";
		return 1;
	}

	if (fwrite(data, size, 1, fp) != 1)
	{
		std::cout << "Write error\n";
		return 1;
	}

	fclose(fp);
	return 0;
}


inline void PrintSettings(const ako::Settings& s, const std::string& side = "encoder-side")
{
	std::cout << "[" << ako::ToString(s.color);
	std::cout << ", " << ako::ToString(s.wavelet);
	std::cout << ", " << ako::ToString(s.wrap);
	std::cout << ", " << ako::ToString(s.compression);
	std::cout << ", t:" << std::to_string(s.tiles_dimension);

	if (side == "encoder-side")
	{
		std::cout << ", q:" << std::to_string(s.quantization);
		std::cout << ", g:" << std::to_string(s.gate);
		std::cout << ", l:" << std::to_string(s.chroma_loss);
		std::cout << ", d:" << ((s.discard) ? "true" : "false");
	}

	std::cout << "]\n";
}


int DecodePng(size_t in_size, const void* in, unsigned& out_width, unsigned& out_height, unsigned& out_channels,
              unsigned& out_depth, void** out_image);

size_t EncodePng(int effort, unsigned width, unsigned height, unsigned channels, unsigned depth, const void* in,
                 void** out_blob);

void SavePlanarPgm(unsigned tile_no, unsigned width, unsigned height, unsigned channels, unsigned depth,
                   unsigned in_stride, const void* in, const std::string& basename);

void SaveInterleavedPgm(unsigned tile_no, unsigned width, unsigned height, unsigned channels, unsigned depth,
                        unsigned in_stride, const void* in, const std::string& basename);


struct CallbacksData
{
	bool print;
	std::string prefix;

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
	size_t workarea_size;

	unsigned current_tile;
	unsigned tile_width;
	unsigned tile_height;
	unsigned tile_x;
	unsigned tile_y;
	size_t tile_data_size;
};

void CallbackGenericEvent(ako::GenericEvent e, unsigned a, unsigned b, unsigned c, size_t d, void* user_data);

void CallbackFormatEvent(ako::Color, unsigned tile_no, const void* image_data, void* user_data);
void CallbackLiftingEvent(ako::Wavelet, ako::Wrap, unsigned tile_no, const void* image_data, void* user_data);
void CallbackCompressionEvent(ako::Compression, unsigned tile_no, const void* data, void* user_data);

#endif
