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


template <typename T>
static void sSavePlanarPgm(unsigned width, unsigned height, unsigned in_stride, const T* in,
                           const std::string& filename)
{
	FILE* fp = fopen(filename.c_str(), "wb");
	if (fp != nullptr)
	{
		fprintf(fp, "P5\n%u\n%u\n255\n", width, height);

		for (unsigned row = 0; row < height; row += 1)
			for (unsigned col = 0; col < width; col += 1)
			{
				const auto u8 = static_cast<uint8_t>(abs(in[row * in_stride + col]));
				fwrite(&u8, 1, 1, fp);
			}

		fclose(fp);
	}
}


template <typename T>
static void sSaveInterleavedPgm(unsigned width, unsigned height, unsigned channels, unsigned in_stride, const T* in,
                                const std::string& filename)
{
	in_stride = in_stride * channels;

	FILE* fp = fopen(filename.c_str(), "wb");
	if (fp != nullptr)
	{
		fprintf(fp, "P5\n%u\n%u\n255\n", width, height);

		for (unsigned row = 0; row < height; row += 1)
			for (unsigned col = 0; col < width; col += 1)
			{
				const auto u8 = static_cast<uint8_t>(in[row * in_stride + col * channels]);
				fwrite(&u8, 1, 1, fp);
			}

		fclose(fp);
	}
}


static void sEventPrintPrefix(const CallbacksData& data, unsigned indent)
{
	printf("%s |", (data.side == "encoder-side") ? "E" : "D");

	for (unsigned i = 0; i < indent; i += 1)
		printf("    ");

	printf("- ");
}


static void sEventPrintTile(const CallbacksData& data)
{
	sEventPrintPrefix(data, 1);
	printf("Tile %u of %u, [%u, %u], %ux%u px (%zu byte(s))\n", data.current_tile, data.tiles_no, data.tile_x,
	       data.tile_y, data.tile_width, data.tile_height, data.tile_data_size);
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
		sEventPrintPrefix(data, 1);
		printf("Image: %ux%u px, %u channel(s), %u bpp depth\n", data.image_width, data.image_height, data.channels,
		       data.depth);
		data.image_events = 0;
	}

	if (data.tiles_events == 2) // Tiles info
	{
		sEventPrintPrefix(data, 1);
		printf("%u Tiles, %ux%u px\n", data.tiles_no, data.tiles_dimension, data.tiles_dimension);
		data.tiles_events = 0;
	}

	if (data.memory_events == 1) // Memory info
	{
		sEventPrintPrefix(data, 1);
		printf("Workarea size: %zu byte(s)\n", data.workarea_size);
		data.memory_events = 0;
	}
}


void CallbackFormatEvent(ako::Color color, unsigned tile_no, const void* image_data, void* user_data)
{
	auto& data = *reinterpret_cast<CallbacksData*>(user_data);

	if (data.current_tile == tile_no)
	{
		sEventPrintTile(data);
		data.current_tile = 0;
	}

	if (tile_no == 1 && image_data == nullptr)
	{
		sEventPrintPrefix(data, 2);
		printf("Color transformation: %s\n", ako::ToString(color));
	}

#if 0
	// Developers, developers, developers
	if (image_data != nullptr && data.depth <= 8)
	{
		for (unsigned ch = 0; ch < data.channels; ch += 1)
		{
			if (data.side == "decoder-side")
			{
				SaveInterleavedPgm(
				    data.tile_width, data.tile_height, data.channels, data.tile_width,
				    reinterpret_cast<const uint8_t*>(image_data) + ch,
				    ("t" + std::to_string(tile_no) + "ch" + std::to_string(ch) + "-format-d.pgm").c_str());
			}
			else
			{
				SavePlanarPgm(data.tile_width, data.tile_height, data.tile_width,
				              reinterpret_cast<const int16_t*>(image_data) + (data.tile_width * data.tile_height * ch),
				              ("t" + std::to_string(tile_no) + "ch" + std::to_string(ch) + "-format-e.pgm").c_str());
			}
		}
	}
#else
	(void)image_data;
#endif
}


void CallbackCompressionEvent(ako::Compression method, unsigned tile_no, const void* image_data, void* user_data)
{
	auto& data = *reinterpret_cast<CallbacksData*>(user_data);
	(void)image_data;

	if (data.current_tile == tile_no)
	{
		sEventPrintTile(data);
		data.current_tile = 0;
	}

	if (tile_no == 1 && image_data == nullptr)
	{
		sEventPrintPrefix(data, 2);
		printf("Compression method: %s\n", ako::ToString(method));
	}

#if 0
	// Developers, developers, developers
	if (image_data != nullptr && data.side == "decoder-side")
	{
		if (data.depth <= 8)
		{
			for (unsigned ch = 0; ch < data.channels; ch += 1)
			{
				SavePlanarPgm(
				    data.tile_width, data.tile_height, data.tile_width,
				    reinterpret_cast<const int16_t*>(image_data) + (data.tile_width * data.tile_height * ch),
				    ("d-t" + std::to_string(tile_no) + "ch" + std::to_string(ch) + "-compression.pgm").c_str());
			}
		}
	}
#else
	(void)image_data;
#endif
}
