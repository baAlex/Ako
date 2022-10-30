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
#include "compression-kagari.hpp"
#include "compression-none.hpp"

namespace ako
{

template <typename T> static inline T sQuantizer(float q, T value)
{
	if (value > 0)
		return static_cast<T>(roundf(fabsf(static_cast<float>(value) / q)) * q);

	return static_cast<T>(-roundf(fabsf(static_cast<float>(value) / q)) * q);
}


template <typename T>
static size_t sCompress(Compressor<T>& compressor, const Settings& settings, unsigned width, unsigned height,
                        unsigned channels, const T* input)
{
	// Code suspiciously similar to Unlift()

	auto in = input;
	unsigned lp_w = width;
	unsigned lp_h = height;
	unsigned hp_w, hp_h;

	const auto lifts_no = LiftsNo(width, height);
	if (lifts_no > 1)
		LiftMeasures((lifts_no - 1), width, height, lp_w, lp_h, hp_w, hp_h);

	// Lowpasses
	for (unsigned ch = 0; ch < channels; ch += 1)
	{
		if (compressor.Step(sQuantizer, 1.0F, lp_w, lp_h, in) == 0)
			return 0;

		in += (lp_w * lp_h); // Quadrant A
	}

	// Highpasses
	for (unsigned lift = (lifts_no - 1); lift < lifts_no; lift -= 1) // Underflows
	{
		LiftMeasures(lift, width, height, lp_w, lp_h, hp_w, hp_h);

		// Quantization step
		const auto x = static_cast<float>(lifts_no) -
		               static_cast<float>(lift + (lifts_no - 8)); // Limit quantization to the last 8 harmonics,
		                                                          // for reference the DCT on JPEG produces 3 harmonics
		                                                          // (ending up in a limit of blocks of 8 pixels)
		                                                          // On J2K this is configurable, 8 harmonics being
		                                                          // the default (I think)...
		                                                          // TODO: revise this again with the compression
		                                                          // stage actually compressing

		const auto q = powf(2.0F, x * (settings.quantization / 50.0F) * powf(x / 16.0F, 1.0F));
		const float q_diagonal = (settings.quantization > 0.0F) ? 2.0F : 1.0F;

		// printf("%f %f\n", q, x);

		// Iterate in Yuv order
		for (unsigned ch = 0; ch < channels; ch += 1)
		{
			// Quadrant C
			if (compressor.Step(sQuantizer, (x > 0.0) ? q : 1.0F, lp_w, hp_h, in) == 0)
				return 0;

			in += (lp_w * hp_h);

			// Quadrant B
			if (compressor.Step(sQuantizer, (x > 0.0) ? q : 1.0F, hp_w, lp_h, in) == 0)
				return 0;

			in += (hp_w * lp_h);

			// Quadrant D
			if (compressor.Step(sQuantizer, (x > 0.0) ? (q * q_diagonal) : 1.0F, hp_w, hp_h, in) == 0)
				return 0;

			in += (hp_w * hp_h);
		}
	}

	return compressor.Finish();
}


template <>
size_t Compress(const Settings& settings, unsigned width, unsigned height, unsigned channels, const int16_t* input,
                void* output)
{
	size_t compressed_size = 0;

	if (settings.compression == Compression::Kagari)
	{
		auto compressor = CompressorKagari<int16_t>(output, sizeof(int16_t) * width * height * channels);
		if ((compressed_size = sCompress(compressor, settings, width, height, channels, input)) == 0)
			goto fallback;
	}
	else // No compression
	{
	fallback:
		auto compressor = CompressorNone<int16_t>(output);
		compressed_size = sCompress(compressor, settings, width, height, channels, input);
	}

	return compressed_size;
}

template <>
size_t Compress(const Settings& settings, unsigned width, unsigned height, unsigned channels, const int32_t* input,
                void* output)
{
	size_t compressed_size = 0;

	if (settings.compression == Compression::Kagari)
	{
		auto compressor = CompressorKagari<int32_t>(output, sizeof(int32_t) * width * height * channels);
		if ((compressed_size = sCompress(compressor, settings, width, height, channels, input)) == 0)
			goto fallback;
	}
	else // No compression
	{
	fallback:
		auto compressor = CompressorNone<int32_t>(output);
		compressed_size = sCompress(compressor, settings, width, height, channels, input);
	}

	return compressed_size;
}

} // namespace ako
