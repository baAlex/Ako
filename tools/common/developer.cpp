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

#include <cstdint>
#include <cstdio>

#include "developer.hpp"


template <typename T>
static void sSavePlanarPgm(unsigned tile_no, unsigned width, unsigned height, unsigned channels, unsigned depth,
                           unsigned in_stride, const T* in, const std::string& basename)
{
	(void)depth;

	for (unsigned ch = 0; ch < channels; ch += 1)
	{
		FILE* fp =
		    fopen((basename + "Tile" + std::to_string(tile_no) + "Ch" + std::to_string(ch + 1) + ".pgm").c_str(), "wb");

		if (fp == nullptr)
			break;

		fprintf(fp, "P5\n%u\n%u\n255\n", width, height);

		for (unsigned row = 0; row < height; row += 1)
			for (unsigned col = 0; col < width; col += 1)
			{
				const auto u8 = static_cast<uint8_t>(abs(in[row * in_stride + col]));
				fwrite(&u8, 1, 1, fp);
			}

		fclose(fp);
		in += (in_stride * height);
	}
}


template <typename T>
static void sSaveInterleavedPgm(unsigned tile_no, unsigned width, unsigned height, unsigned channels, unsigned depth,
                                unsigned in_stride, const T* in, const std::string& basename)
{
	in_stride = in_stride * channels;
	(void)depth;

	for (unsigned ch = 0; ch < channels; ch += 1)
	{
		FILE* fp =
		    fopen((basename + "Tile" + std::to_string(tile_no) + "Ch" + std::to_string(ch + 1) + ".pgm").c_str(), "wb");

		if (fp == nullptr)
			break;

		fprintf(fp, "P5\n%u\n%u\n255\n", width, height);

		for (unsigned row = 0; row < height; row += 1)
			for (unsigned col = 0; col < width; col += 1)
			{
				const auto u8 = static_cast<uint8_t>(in[row * in_stride + col * channels]);
				fwrite(&u8, 1, 1, fp);
			}

		fclose(fp);
		in += 1;
	}
}


void SavePlanarPgm(unsigned tile_no, unsigned width, unsigned height, unsigned channels, unsigned depth,
                   unsigned in_stride, const void* in, const std::string& basename)
{
	if (depth <= 8)
		sSavePlanarPgm(tile_no, width, height, channels, depth, in_stride, reinterpret_cast<const int16_t*>(in),
		               basename);
}

void SaveInterleavedPgm(unsigned tile_no, unsigned width, unsigned height, unsigned channels, unsigned depth,
                        unsigned in_stride, const void* in, const std::string& basename)
{
	if (depth <= 8)
		sSaveInterleavedPgm(tile_no, width, height, channels, depth, in_stride, reinterpret_cast<const uint8_t*>(in),
		                    basename);
}
