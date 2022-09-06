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


static void sEventPrintPrefix(const char* str, unsigned indent)
{
	printf("%s", str);

	for (unsigned i = 0; i < indent; i += 1)
		printf("    ");

	printf("- ");
}

static void sEventPrintTile(const CallbacksData& data)
{
	sEventPrintPrefix(data.prefix.c_str(), 1);
	printf("Tile %u of %u, [%u, %u], %ux%u px (%zu byte(s))\n", data.current_tile, data.tiles_no, data.tile_x,
	       data.tile_y, data.tile_width, data.tile_height, data.tile_data_size);
}


void CallbackCompressionEvent(ako::Compression method, unsigned tile_no, unsigned a, void* user_data)
{
	auto& data = *reinterpret_cast<CallbacksData*>(user_data);
	(void)a;

	if (data.current_tile == tile_no)
	{
		sEventPrintTile(data);
		data.current_tile = 0;
	}

	if (tile_no == 1)
	{
		sEventPrintPrefix(data.prefix.c_str(), 2);
		printf("Compression: %s\n", ako::ToString(method));
	}
}


void CallbackFormatEvent(ako::Color color, unsigned tile_no, unsigned a, void* user_data)
{
	auto& data = *reinterpret_cast<CallbacksData*>(user_data);
	(void)a;

	if (data.current_tile == tile_no)
	{
		sEventPrintTile(data);
		data.current_tile = 0;
	}

	if (tile_no == 1)
	{
		sEventPrintPrefix(data.prefix.c_str(), 2);
		printf("Format: %s\n", ako::ToString(color));
	}
}


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
	if (data.image_events == 3) // Image info
	{
		sEventPrintPrefix(data.prefix.c_str(), 1);
		printf("Image: %ux%u px, %u channel(s), %u bpp depth\n", data.image_width, data.image_height, data.channels,
		       data.depth);
		data.image_events = 0;
	}

	if (data.tiles_events == 2) // Tiles info
	{
		sEventPrintPrefix(data.prefix.c_str(), 1);
		printf("%u Tiles, %ux%u px\n", data.tiles_no, data.tiles_dimension, data.tiles_dimension);
		data.tiles_events = 0;
	}

	if (data.memory_events == 1) // Memory info
	{
		sEventPrintPrefix(data.prefix.c_str(), 1);
		printf("Workarea size: %zu byte(s)\n", data.workarea_size);
		data.memory_events = 0;
	}
}
