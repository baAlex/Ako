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

#include "shared.hpp"


template <typename T>
static void sSavePlanarPgm(unsigned width, unsigned height, unsigned in_stride, const T* in,
                           const std::string& output_filename)
{
	FILE* fp = fopen(output_filename.c_str(), "wb");
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
                                const std::string& output_filename)
{
	in_stride = in_stride * channels;

	FILE* fp = fopen(output_filename.c_str(), "wb");
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


template <>
void SavePlanarPgm(unsigned width, unsigned height, unsigned in_stride, const int16_t* in,
                   const std::string& output_filename)
{
	sSavePlanarPgm(width, height, in_stride, in, output_filename);
}

template <>
void SaveInterleavedPgm(unsigned width, unsigned height, unsigned channels, unsigned in_stride, const uint8_t* in,
                        const std::string& output_filename)
{
	sSaveInterleavedPgm(width, height, channels, in_stride, in, output_filename);
}
