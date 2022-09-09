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
static void sFormatToRgb(const Color& color_transformation, unsigned width, unsigned height, unsigned channels,
                         size_t output_stride, const TIn* input, TOut* output)
{
	output_stride = output_stride * channels;
	const auto plane_offset = width * height;

	// Color transformation, and interleave first 3 channels
	if (channels >= 3)
	{
		auto in = input;
		auto out = output;

		switch (color_transformation)
		{
		case Color::YCoCg:
			for (unsigned row = 0; row < height; row += 1)
			{
				for (unsigned col = 0; col < width; col += 1)
				{
					const auto y = in[plane_offset * 0 + col];
					const auto u = in[plane_offset * 1 + col];
					const auto v = in[plane_offset * 2 + col];

					const TIn temp = y - (v / 2);
					const TIn g = v + temp;
					const TIn b = temp - (u / 2);
					const TIn r = b + u;

					out[col * channels + 0] = Saturate<TIn, TOut>(r);
					out[col * channels + 1] = Saturate<TIn, TOut>(g);
					out[col * channels + 2] = Saturate<TIn, TOut>(b);
				}

				in += width;
				out += output_stride;
			}
			break;

		case Color::SubtractG:
			for (unsigned row = 0; row < height; row += 1)
			{
				for (unsigned col = 0; col < width; col += 1)
				{
					const auto y = in[plane_offset * 0 + col];
					const auto u = in[plane_offset * 1 + col];
					const auto v = in[plane_offset * 2 + col];

					const TIn r = u + y;
					const TIn g = y;
					const TIn b = v + y;

					out[col * channels + 0] = Saturate<TIn, TOut>(r);
					out[col * channels + 1] = Saturate<TIn, TOut>(g);
					out[col * channels + 2] = Saturate<TIn, TOut>(b);
				}

				in += width;
				out += output_stride;
			}
			break;

		case Color::None: // We still need to saturate it
			for (unsigned row = 0; row < height; row += 1)
			{
				for (unsigned col = 0; col < width; col += 1)
				{
					const auto r = in[plane_offset * 0 + col];
					const auto g = in[plane_offset * 1 + col];
					const auto b = in[plane_offset * 2 + col];

					out[col * channels + 0] = Saturate<TIn, TOut>(r);
					out[col * channels + 1] = Saturate<TIn, TOut>(g);
					out[col * channels + 2] = Saturate<TIn, TOut>(b);
				}

				in += width;
				out += output_stride;
			}
		}
	}

	// Interleave remaining channels
	if (channels != 3)
	{
		const unsigned from_channel = (channels > 3) ? 3 : 0;
		auto in = input;
		auto out = output;

		for (unsigned row = 0; row < height; row += 1)
		{
			for (unsigned col = 0; col < width; col += 1)
			{
				for (unsigned ch = from_channel; ch < channels; ch += 1)
					out[col * channels + ch] = Saturate<TIn, TOut>(in[plane_offset * ch + col]);
			}

			in += width;
			out += output_stride;
		}
	}
}


template <>
void FormatToRgb(const Color& color_transformation, unsigned width, unsigned height, unsigned channels,
                 size_t output_stride, const int16_t* input, uint8_t* output)
{
	sFormatToRgb(color_transformation, width, height, channels, output_stride, input, output);
}

template <>
void FormatToRgb(const Color& color_transformation, unsigned width, unsigned height, unsigned channels,
                 size_t output_stride, const int32_t* input, uint16_t* output)
{
	sFormatToRgb(color_transformation, width, height, channels, output_stride, input, output);
}

} // namespace ako
