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
#include <string>

#include "shared.hpp"


// TODO, copy-paste hell


void CallbackGenericEvent(ako::GenericEvent e, unsigned a, unsigned b, unsigned c, size_t d, void* user_data)
{
	auto& data = *reinterpret_cast<CallbacksData*>(user_data);

	// Gather information
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
			data.workarea_size = d;
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
		case ako::GenericEvent::TileDataSize:
			data.current_tile = a;
			data.tile_data_size = d;
			break;
		}
	}

	// Print
	if (data.print == true)
	{
		// Image info
		if (data.image_events == 3)
		{
			printf("%s\t-Image: %ux%u px, %u channel(s), %u bpp depth\n", data.prefix.c_str(), data.image_width,
			       data.image_height, data.channels, data.depth);
			data.image_events = 0;
		}

		// Tiles info
		if (data.tiles_events == 2)
		{
			printf("%s\t- %u Tiles, %ux%u px\n", data.prefix.c_str(), data.tiles_no, data.tiles_dimension,
			       data.tiles_dimension);
			data.tiles_events = 0;
		}

		// Memory info
		if (data.memory_events == 1)
		{
			printf("%s\t- Workarea size: %zu byte(s)\n", data.prefix.c_str(), data.workarea_size);
			data.memory_events = 0;
		}
	}
}


void CallbackFormatEvent(ako::Color color, unsigned tile_no, const void* image_data, void* user_data)
{
	using namespace std;
	auto& data = *reinterpret_cast<CallbacksData*>(user_data);

	// Tile information shouldn't be print until is completely gathered, so,
	// we do it now as being in a format event means that all tile info is there
	if (data.print == true && data.current_tile == tile_no)
	{
		printf("%s\tTile %u of %u, [%u, %u], %ux%u px (%zu byte(s))\n", data.prefix.c_str(), data.current_tile,
		       data.tiles_no, data.tile_x, data.tile_y, data.tile_width, data.tile_height, data.tile_data_size);
		data.current_tile = 0;
	}

	// Start event
	if (image_data == nullptr)
	{
		// Benchmark
		data.clock = chrono::steady_clock::now();

		// Format info
		if (data.print == true && tile_no == 1)
			printf("%s\t\t- Color transformation: %s\n", data.prefix.c_str(), ako::ToString(color));
	}

	// End event
	else
	{
		// Benchmark
		const auto diff = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - data.clock);
		data.format_duration += diff;
		data.total_duration += diff;

		// Developers, developers, developers
		/*if (data.prefix == "E |")
			SavePlanarPgm(tile_no, data.tile_width, data.tile_height, data.channels, data.depth, data.tile_width,
			              image_data, data.prefix + " Format");
		else
			SaveInterleavedPgm(tile_no, data.tile_width, data.tile_height, data.channels, data.depth, data.tile_width,
			                   image_data, data.prefix + " Format");*/
	}
}


void CallbackLiftingEvent(ako::Wavelet wavelet, ako::Wrap wrap, unsigned tile_no, const void* image_data,
                          void* user_data)
{
	using namespace std;
	auto& data = *reinterpret_cast<CallbacksData*>(user_data);

	// As above
	if (data.print == true && data.current_tile == tile_no)
	{
		printf("%s\t- Tile %u of %u, [%u, %u], %ux%u px (%zu byte(s))\n", data.prefix.c_str(), data.current_tile,
		       data.tiles_no, data.tile_x, data.tile_y, data.tile_width, data.tile_height, data.tile_data_size);
		data.current_tile = 0;
	}

	// Start event
	if (image_data == nullptr)
	{
		// Benchmark
		data.clock = chrono::steady_clock::now();

		// Lifting info
		if (data.print == true && tile_no == 1)
		{
			printf("%s\t\t- Wavelet transformation: %s\n", data.prefix.c_str(), ako::ToString(wavelet));
			printf("%s\t\t- Wrap mode: %s\n", data.prefix.c_str(), ako::ToString(wrap));
		}
	}

	// End event
	else
	{
		// Benchmark
		const auto diff = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - data.clock);
		data.lifting_duration += diff;
		data.total_duration += diff;
	}
}


void CallbackCompressionEvent(ako::Compression compression, unsigned tile_no, const void* image_data, void* user_data)
{
	using namespace std;
	auto& data = *reinterpret_cast<CallbacksData*>(user_data);

	// As above
	if (data.print == true && data.current_tile == tile_no)
	{
		printf("%s\t- Tile %u of %u, [%u, %u], %ux%u px (%zu byte(s))\n", data.prefix.c_str(), data.current_tile,
		       data.tiles_no, data.tile_x, data.tile_y, data.tile_width, data.tile_height, data.tile_data_size);
		data.current_tile = 0;
	}

	// Start event
	if (image_data == nullptr)
	{
		// Benchmark
		data.clock = chrono::steady_clock::now();

		// Compression info
		if (data.print == true && tile_no == 1)
			printf("%s\t\t- Compression method: %s\n", data.prefix.c_str(), ako::ToString(compression));
	}

	// End event
	else
	{
		// Benchmark
		const auto diff = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - data.clock);
		data.compression_duration += diff;
		data.total_duration += diff;
	}
}
