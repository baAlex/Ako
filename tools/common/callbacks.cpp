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

#include <cstdio>
#include <string>

#include "callbacks.hpp"
#include "developer.hpp"


// TODO, copy-paste hell


void CallbackGenericEvent(ako::GenericEvent e, unsigned arg_a, unsigned arg_b, unsigned arg_c, ako::GenericType arg_d,
                          void* user_data)
{
	auto& data = *reinterpret_cast<CallbacksData*>(user_data);

	// Gather information
	{
		switch (e)
		{
		// Image info
		case ako::GenericEvent::ImageDimensions:
			data.image_width = arg_a;
			data.image_height = arg_b;
			data.image_events += 1;
			break;
		case ako::GenericEvent::ImageChannels:
			data.channels = arg_a;
			data.image_events += 1;
			break;
		case ako::GenericEvent::ImageDepth:
			data.depth = arg_a;
			data.image_events += 1;
			break;

		// Tiles info
		case ako::GenericEvent::TilesNo:
			data.tiles_no = arg_a;
			data.tiles_events += 1;
			break;
		case ako::GenericEvent::TilesDimension:
			data.tiles_dimension = arg_a;
			data.tiles_events += 1;
			break;

		// Memory info
		case ako::GenericEvent::WorkAreaSize:
			data.work_area_size = arg_d.u;
			data.memory_events += 1;
			break;

		// Tile info
		case ako::GenericEvent::TileDimensions:
			data.current_tile = arg_a;
			data.tile_width = arg_b;
			data.tile_height = arg_c;
			data.print_tile_info += 1;
			break;
		case ako::GenericEvent::TilePosition:
			data.current_tile = arg_a;
			data.tile_x = arg_b;
			data.tile_y = arg_c;
			data.print_tile_info += 1;
			break;
		case ako::GenericEvent::TileDataSize:
			data.current_tile = arg_a;
			data.tile_data_size = arg_d.u;
			data.print_tile_info += 1;
			break;

		// Lift info
		case ako::GenericEvent::LiftLowpassDimensions:
			if (data.print == true)
				printf("%s\t\t - LowpassCh%u %ux%u px (%li)\n", data.prefix, arg_c, arg_a, arg_b, arg_d.s);
			break;
		case ako::GenericEvent::LiftHighpassesDimensions:
			if (data.print == true)
				printf("%s\t\t - Highpass %ux%u px\n", data.prefix, arg_a, arg_b);
			break;

		// Ratio
		case ako::GenericEvent::RatioIteration:
			if (data.print == true)
				printf("%s\t\t - Ratio iteration, q: %.3f\n", data.prefix, arg_d.f);
			break;
		}
	}

	// Print
	if (data.print == true)
	{
		// Image info
		if (data.image_events == 3)
		{
			printf("%s\tImage, %ux%u px, %u channel(s), %u bpp depth\n", data.prefix, data.image_width,
			       data.image_height, data.channels, data.depth);
			data.image_events = 0;
		}

		// Tiles info
		if (data.tiles_events == 2)
		{
			printf("%s\t\t - %u Tiles, %ux%u px\n", data.prefix, data.tiles_no, data.tiles_dimension,
			       data.tiles_dimension);
			data.tiles_events = 0;
		}

		// Memory info
		if (data.memory_events == 1)
		{
			printf("%s\t\t - Work area size: %zu byte(s)\n", data.prefix, data.work_area_size);
			data.memory_events = 0;
		}
	}
}


static void sCommon(CallbacksData& data, const void* image_data)
{
	using namespace std;

	// Tile information shouldn't be print until is completely gathered, so,
	// we do it now when all tile info is there
	if (data.print == true && data.print_tile_info >= 3)
	{
		printf("%s\tTile %u of %u, [%u, %u], %ux%u px (%zu byte(s))\n", data.prefix, data.current_tile + 1,
		       data.tiles_no, data.tile_x, data.tile_y, data.tile_width, data.tile_height, data.tile_data_size);
		data.print_tile_info = 0;
	}

	// Benchmark
	if (image_data == nullptr)
		data.clock = chrono::steady_clock::now();
}


void CallbackFormatEvent(unsigned tile_no, ako::Color color, const void* image_data, void* user_data)
{
	using namespace std;
	auto& data = *reinterpret_cast<CallbacksData*>(user_data);

	sCommon(data, image_data);

	// Start event
	if (image_data == nullptr)
	{
		if (data.print == true && tile_no == 0)
			printf("%s\t\t - Color transformation: %s\n", data.prefix, ako::ToString(color));
	}

	// End event
	else
	{
		// Benchmark
		const auto diff = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - data.clock);
		data.format_duration += diff;
		data.total_duration += diff;
	}
}


void CallbackLiftingEvent(unsigned tile_no, ako::Wavelet wavelet, ako::Wrap wrap, const void* image_data,
                          void* user_data)
{
	using namespace std;
	auto& data = *reinterpret_cast<CallbacksData*>(user_data);

	// As above
	sCommon(data, image_data);

	// Start event
	if (image_data == nullptr)
	{
		if (data.print == true && tile_no == 0)
		{
			printf("%s\t\t - Wavelet transformation: %s\n", data.prefix, ako::ToString(wavelet));
			printf("%s\t\t - Wrap mode: %s\n", data.prefix, ako::ToString(wrap));
		}
	}

	// End event
	else
	{
		// Benchmark
		const auto diff = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - data.clock);
		data.lifting_duration += diff;
		data.total_duration += diff;

		// SavePlanarPgm(tile_no, data.tile_width, data.tile_height, data.channels, 8, data.tile_width, image_data,
		//               std::string(data.prefix));
	}
}


void CallbackCompressionEvent(unsigned tile_no, ako::Compression compression, const void* image_data, void* user_data)
{
	(void)tile_no;

	using namespace std;
	auto& data = *reinterpret_cast<CallbacksData*>(user_data);

	// As above
	sCommon(data, image_data);

	// End event
	if (image_data != nullptr)
	{
		if (data.print == true)
			printf("%s\t\t - Compression method: %s\n", data.prefix, ako::ToString(compression));

		// Benchmark
		const auto diff = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - data.clock);
		data.compression_duration += diff;
		data.total_duration += diff;
	}
}


#include <algorithm>

void CallbackHistogramEvent(const ako::Histogram* histogram, unsigned length, void* user_data)
{
	using namespace std;
	auto& data = *reinterpret_cast<CallbacksData*>(user_data);

	unsigned c_last = 0;
	unsigned c_peak_value = 0;
	unsigned c_peak_at = 0;

	unsigned d_last = 0;
	unsigned d_peak_value = 0;
	unsigned d_peak_at = 0;

	if (histogram == nullptr || length == 0)
		return;

	// Find peaks
	for (unsigned i = 0; i < length; i += 1)
	{
		if (histogram[i].c != 0)
		{
			c_last = i;
			if (histogram[i].c > c_peak_value)
			{
				c_peak_value = histogram[i].c;
				c_peak_at = i;
			}
		}

		if (histogram[i].d != 0)
		{
			d_last = i;
			if (histogram[i].d > d_peak_value)
			{
				d_peak_value = histogram[i].d;
				d_peak_at = i;
			}
		}
	}

	// Feedback
	if (data.print == true)
	{
		printf("%s\t\t - Histogram:\n", data.prefix);
		printf("%s\t\t\t - C, last: %u, peak value: %u, at: %u\n", data.prefix, c_last + 1, c_peak_value,
		       c_peak_at + 1);
		printf("%s\t\t\t - D, last: %u, peak value: %u, at: %u\n", data.prefix, d_last + 1, d_peak_value,
		       d_peak_at + 1);
	}

	// Write to file
	if (data.histogram_output != "")
	{
		// Make a filename
		auto filename = data.histogram_output;
		{
			// TODO, spooky, strings are hard :(

			const auto new_ext = ((data.tiles_no > 1) ? ("-" + std::to_string(data.current_tile + 1)) : "") + ".csv";

			auto lcase_filename = data.histogram_output;
			std::transform(lcase_filename.begin(), lcase_filename.end(), lcase_filename.begin(),
			               [](unsigned char c) { return std::tolower(c); });

			const auto ext_pos = lcase_filename.rfind(".csv");
			if (ext_pos != std::string::npos)
				filename.replace(ext_pos, 4, new_ext);
			else
				filename += new_ext;
		}

		// Write
		auto fp = fopen(filename.c_str(), "w");
		if (fp != nullptr)
		{
			for (unsigned i = 0; i < std::max(c_last, d_last) + 1; i += 1)
				fprintf(fp, "%u,%u\n", histogram[i].c, histogram[i].d);
			fclose(fp);

			if (data.print == true)
				printf("%s\t\t\t - Wrote CSV '%s'\n", data.prefix, filename.c_str());
		}
	}
}
