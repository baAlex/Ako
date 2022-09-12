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

template <size_t channels, typename TIn, typename TOut>
static inline void sInterleave(unsigned width, unsigned plane_offset, size_t output_stride, const TIn* in, TOut* out,
                               const TOut* out_end)
{
	for (; out < out_end; out += output_stride, in += width)
		for (unsigned col = 0; col < width; col++) // [B]
		{
			for (unsigned ch = 0; ch < channels; ch++)
				out[col * channels + ch] = static_cast<TOut>(in[plane_offset * ch + col]); // [A]
		}
}


template <typename TIn, typename TOut>
static void sFormatToRgb(const Color& color_transformation, unsigned width, unsigned height, unsigned channels,
                         size_t output_stride, TIn* input, TOut* output)
{
	// TODO: this function is 5 ms slower that the old C one :(

	// According to Perf lines [A] and [B] are the origin of 14% of cycles usage, 4% cache misses and
	// 5% of branches misses of the entire decoding (at the time of writing this, there is no other
	// decoding stage than this one), with honorable mentions to Min() and Max() functions.
	// So, SIMD seems to be the only fix here, weird that C optimizes things different (and I too lazy
	// to check the assembly)

	output_stride = output_stride * channels;
	const auto plane_offset = width * height;

	// Color transformation
	if (channels >= 3)
	{
		switch (color_transformation)
		{
		case Color::YCoCg:
			for (unsigned i = 0; i < (width * height); i += 1)
			{
				const auto y = input[plane_offset * 0 + i];
				const auto u = input[plane_offset * 1 + i];
				const auto v = input[plane_offset * 2 + i];

				const auto temp = static_cast<TIn>(y - (v / 2));

				const auto g = static_cast<TIn>(v + temp);
				const auto b = static_cast<TIn>(temp - (u / 2));
				const auto r = static_cast<TIn>(b + u);

				input[plane_offset * 0 + i] = SaturateToLower<TIn>(r);
				input[plane_offset * 1 + i] = SaturateToLower<TIn>(g);
				input[plane_offset * 2 + i] = SaturateToLower<TIn>(b);
			}
			break;

		case Color::SubtractG:
			for (unsigned i = 0; i < (width * height); i += 1)
			{
				const auto y = input[plane_offset * 0 + i];
				const auto u = input[plane_offset * 1 + i];
				const auto v = input[plane_offset * 2 + i];

				const auto r = static_cast<TIn>(u + y);
				const auto g = static_cast<TIn>(y);
				const auto b = static_cast<TIn>(v + y);

				input[plane_offset * 0 + i] = SaturateToLower<TIn>(r);
				input[plane_offset * 1 + i] = SaturateToLower<TIn>(g);
				input[plane_offset * 2 + i] = SaturateToLower<TIn>(b);
			}
			break;

		case Color::None: // We still need to saturate
			for (unsigned i = 0; i < (width * height); i += 1)
			{
				const auto r = input[plane_offset * 0 + i];
				const auto g = input[plane_offset * 1 + i];
				const auto b = input[plane_offset * 2 + i];

				input[plane_offset * 0 + i] = SaturateToLower<TIn>(r);
				input[plane_offset * 1 + i] = SaturateToLower<TIn>(g);
				input[plane_offset * 2 + i] = SaturateToLower<TIn>(b);
			}
		}
	}

	// Saturate
	if (channels != 3)
	{
		const unsigned from_channel = (channels < 3) ? 0 : 3;

		for (unsigned ch = from_channel; ch < channels; ch += 1)
			for (unsigned i = 0; i < (width * height); i += 1)
			{
				input[plane_offset * ch + i] = SaturateToLower<TIn>(input[plane_offset * ch + i]);
			}
	}

	// Interleave channels
	{
		const TOut* out_end = output + output_stride * height;

		if (channels == 3)
			sInterleave<3>(width, plane_offset, output_stride, input, output, out_end);
		else if (channels == 4)
			sInterleave<4>(width, plane_offset, output_stride, input, output, out_end);
		else if (channels == 2)
			sInterleave<2>(width, plane_offset, output_stride, input, output, out_end);
		else if (channels == 1)
			sInterleave<1>(width, plane_offset, output_stride, input, output, out_end);
		else
		{
			auto in = input;
			auto out = output;

			for (unsigned row = 0; row < height; row += 1)
			{
				for (unsigned col = 0; col < width; col += 1)
				{
					for (unsigned ch = 0; ch < channels; ch += 1)
						out[col * channels + ch] = static_cast<TOut>(in[plane_offset * ch + col]);
				}

				in += width;
				out += output_stride;
			}
		}
	}
}


template <>
void FormatToRgb(const Color& color_transformation, unsigned width, unsigned height, unsigned channels,
                 size_t output_stride, int16_t* input, uint8_t* output)
{
	sFormatToRgb(color_transformation, width, height, channels, output_stride, input, output);
}

template <>
void FormatToRgb(const Color& color_transformation, unsigned width, unsigned height, unsigned channels,
                 size_t output_stride, int32_t* input, uint16_t* output)
{
	sFormatToRgb(color_transformation, width, height, channels, output_stride, input, output);
}

} // namespace ako
