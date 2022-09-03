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

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "ako.hpp"

#ifndef SHARED_HPP
#define SHARED_HPP


inline int ReadFile(const std::string& input_filename, std::vector<uint8_t>& out)
{
	// Open file
	FILE* fp = fopen(input_filename.c_str(), "rb"); // Only C let me use the seek/tell hack, in C++ is unspecified
	if (fp == NULL)
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


inline int WriteFile(const std::string& output_filename, size_t size, const void* data)
{
	FILE* fp = fopen(output_filename.c_str(), "wb");
	if (fp == NULL)
	{
		std::cout << "Failed to create '" << output_filename << "'\n";
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


struct CallbacksData
{
	std::string prefix;

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
	size_t tile_size;
};

void CallbackCompressionEvent(ako::Compression method, unsigned tile_no, unsigned a, void* user_data);
void CallbackFormatEvent(ako::Color color, unsigned tile_no, unsigned a, void* user_data);
void CallbackGenericEvent(ako::GenericEvent e, unsigned a, unsigned b, unsigned c, size_t d, void* user_data);

#endif
