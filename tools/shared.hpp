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


struct EventsData
{
	std::string prefix;

	int image_events;
	unsigned image_width;
	unsigned image_height;
	unsigned channels;
	unsigned depth;

	int tiles_events;
	unsigned tiles_no;
	unsigned tiles_dimension;

	int memory_events;
	unsigned workarea_size; // TODO

	unsigned current_tile;
	unsigned tile_width;
	unsigned tile_height;
	unsigned tile_x;
	unsigned tile_y;
	unsigned tile_size; // TODO
};

inline void sCompressionEventCallback(ako::Compression method, unsigned tile_no, unsigned a, void* user_data)
{
	auto& data = *reinterpret_cast<EventsData*>(user_data);
	(void)a;

	if (data.current_tile == tile_no)
	{
		std::printf("%s    - ", data.prefix.c_str());
		std::printf("Tile %u of %u, [%u, %u], %ux%u px\n", data.current_tile, data.tiles_no, data.tile_x, data.tile_y,
		            data.tile_width, data.tile_height);
	}

	if (tile_no == 1)
	{
		std::printf("%s        - ", data.prefix.c_str());
		std::printf("Compression: %s\n", ako::ToString(method));
	}
}

inline void sFormatEventCallback(ako::Color color, unsigned tile_no, unsigned a, void* user_data)
{
	auto& data = *reinterpret_cast<EventsData*>(user_data);
	(void)a;

	if (data.current_tile == tile_no)
	{
		std::printf("%s    - ", data.prefix.c_str());
		std::printf("Tile %u of %u, [%u, %u], %ux%u px\n", data.current_tile, data.tiles_no, data.tile_x, data.tile_y,
		            data.tile_width, data.tile_height);
	}

	if (tile_no == 1)
	{
		std::printf("%s        - ", data.prefix.c_str());
		std::printf("Format: %s\n", ako::ToString(color));
	}
}

inline void sGenericEventCallback(ako::GenericEvent e, unsigned a, unsigned b, unsigned c, void* user_data)
{
	auto& data = *reinterpret_cast<EventsData*>(user_data);

	// Recollect events
	{
		switch (e)
		{
		// Image info
		case ako::GenericEvent::ImageDimensions:
			data.image_width = a;
			data.image_height = b;
			data.image_events += 1;
			break;
		case ako::GenericEvent::ImageChannels:
			data.channels = a;
			data.image_events += 1;
			break;
		case ako::GenericEvent::ImageDepth:
			data.depth = a;
			data.image_events += 1;
			break;

		// Tiles info
		case ako::GenericEvent::TilesNo:
			data.tiles_no = a;
			data.tiles_events += 1;
			break;
		case ako::GenericEvent::TilesDimension:
			data.tiles_dimension = a;
			data.tiles_events += 1;
			break;

		// Memory info
		case ako::GenericEvent::WorkareaSize:
			data.workarea_size = a;
			data.memory_events += 1;
			break;

		// Tile info
		case ako::GenericEvent::TileDimensions:
			data.current_tile = a;
			data.tile_width = b;
			data.tile_height = c;
			break;
		case ako::GenericEvent::TilePosition:
			data.current_tile = a;
			data.tile_x = b;
			data.tile_y = c;
			break;
		case ako::GenericEvent::TileSize:
			data.current_tile = a;
			data.tile_size = b;
			break;
		}
	}

	// Print
	if (data.image_events == 3) // Image info
	{
		std::printf("%s    - ", data.prefix.c_str());
		std::printf("Image: %ux%u px, %u channel(s), %u bpp depth\n", data.image_width, data.image_height,
		            data.channels, data.depth);
		data.image_events = -1;
	}

	if (data.tiles_events == 2) // Tiles info
	{
		std::printf("%s    - ", data.prefix.c_str());
		std::printf("%u Tiles, %ux%u px\n", data.tiles_no, data.tiles_dimension, data.tiles_dimension);
		data.tiles_events = -1;
	}

	if (data.memory_events == 1) // Memory info
	{
		std::printf("%s    - ", data.prefix.c_str());
		std::printf("Workarea size: %u byte(s)\n", data.workarea_size);
		data.memory_events = -1;
	}
}
