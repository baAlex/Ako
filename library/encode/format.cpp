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

#include "ako-private.hpp"

namespace ako
{

template <typename TIn, typename TOut>
static void sFormatToInternal(const Color& color_transformation, bool discard, unsigned width, unsigned height,
                              unsigned channels, size_t input_stride, const TIn* input, TOut* output)
{
	input_stride = input_stride * channels;
	const auto plane_offset = width * height;

	// To planar (deinterleave)
	{
		auto in = input;
		auto out = output;

		for (unsigned row = 0; row < height; row += 1)
		{
			for (unsigned col = 0; col < width; col += 1)
			{
				for (unsigned ch = 0; ch < channels; ch += 1)
					out[plane_offset * ch + col] = static_cast<TOut>(in[col * channels + ch]);
			}

			in += input_stride;
			out += width;
		}
	}

	// Discard pixels under transparent areas
	if (discard == true)
	{
		const unsigned alpha_channel = (channels == 2) ? 1 : 3; // Assumes GrayA or RgbA
		auto data = output;

		if (channels > alpha_channel)
		{
			for (unsigned i = 0; i < (width * height); i += 1)
				if (data[plane_offset * alpha_channel + i] == 0)
				{
					for (unsigned ch = 0; ch < alpha_channel; ch += 1)
						data[plane_offset * ch + i] = 0;
				}
		}
	}

	// Format
	if (channels >= 3)
	{
		auto data = output;

		switch (color_transformation)
		{
		case Color::YCoCg:
			for (unsigned i = 0; i < (width * height); i += 1)
			{
				const auto r = data[plane_offset * 0 + i];
				const auto g = data[plane_offset * 1 + i];
				const auto b = data[plane_offset * 2 + i];

				const auto temp = static_cast<TOut>(b + (r - b) / 2);

				data[plane_offset * 1 + i] = static_cast<TOut>(r - b);
				data[plane_offset * 2 + i] = static_cast<TOut>(g - temp);
				data[plane_offset * 0 + i] = static_cast<TOut>(temp + (g - temp) / 2);
			}
			break;

		case Color::SubtractG:
			for (unsigned i = 0; i < (width * height); i += 1)
			{
				const auto r = data[plane_offset * 0 + i];
				const auto g = data[plane_offset * 1 + i];
				const auto b = data[plane_offset * 2 + i];

				data[plane_offset * 0 + i] = static_cast<TOut>(g);
				data[plane_offset * 1 + i] = static_cast<TOut>(r - g);
				data[plane_offset * 2 + i] = static_cast<TOut>(b - g);
			}
			break;

		default: break;
		}
	}
}


template <>
void FormatToInternal(const Color& color_transformation, bool discard, unsigned width, unsigned height,
                      unsigned channels, size_t input_stride, const uint8_t* input, int16_t* output)
{
	sFormatToInternal(color_transformation, discard, width, height, channels, input_stride, input, output);
}

template <>
void FormatToInternal(const Color& color_transformation, bool discard, unsigned width, unsigned height,
                      unsigned channels, size_t input_stride, const uint16_t* input, int32_t* output)
{
	sFormatToInternal(color_transformation, discard, width, height, channels, input_stride, input, output);
}

} // namespace ako
