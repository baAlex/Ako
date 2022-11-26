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

#include <cfloat>

namespace ako
{

template <typename T> static inline void sQuantizer(float q, unsigned length, const T* in, T* out)
{
	if (std::isinf(q) == false)
	{
		if (q > 1.0F)
		{
			// Round is slow, really slow (floor seems to be SIMD optimized)

			// for (unsigned i = 0; i < length; i += 1)
			//	out[i] = static_cast<T>(std::roundf(static_cast<float>(in[i]) / q) * q);

			for (unsigned i = 0; i < length; i += 1)
				out[i] = static_cast<T>(floorf(static_cast<float>(in[i]) / q + 0.5F) * q);
		}
		else
		{
			for (unsigned i = 0; i < length; i += 1)
				out[i] = in[i];
		}
	}
	else
	{
		for (unsigned i = 0; i < length; i += 1)
			out[i] = 0;
	}
}


template <typename T>
static size_t sCompress2ndPhase(Compressor<T>& compressor, const Settings& settings, unsigned width, unsigned height,
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
		if (compressor.Step(sQuantizer, 1.0F, lp_w, lp_h, in) != 0)
			return 0;

		in += (lp_w * lp_h); // Quadrant A
	}

	// Highpasses
	for (unsigned lift = (lifts_no - 1); lift < lifts_no; lift -= 1) // Underflows
	{
		LiftMeasures(lift, width, height, lp_w, lp_h, hp_w, hp_h);

		// Quantization step
		const auto x = (static_cast<float>(lifts_no) - static_cast<float>(lift + 1)) / static_cast<float>(lifts_no - 1);
		const auto q = powf(2.0F, x * settings.quantization * powf(x / 16.0F, 1.0F));
		const float q_diagonal = (settings.quantization > 0.0F) ? 2.0F : 1.0F;

		// printf("x: %f, q: %f\n", x, (x > 0.0) ? q : 1.0F);

		// Iterate in Yuv order
		for (unsigned ch = 0; ch < channels; ch += 1)
		{
			// Quadrant C
			if (compressor.Step(sQuantizer, (x > 0.0) ? q : 1.0F, lp_w, hp_h, in) != 0)
				return 0;

			in += (lp_w * hp_h);

			// Quadrant B
			if (compressor.Step(sQuantizer, (x > 0.0) ? q : 1.0F, hp_w, lp_h, in) != 0)
				return 0;

			in += (hp_w * lp_h);

			// Quadrant D
			if (compressor.Step(sQuantizer, (x > 0.0) ? (q * q_diagonal) : 1.0F, hp_w, hp_h, in) != 0)
				return 0;

			in += (hp_w * hp_h);
		}
	}

	return compressor.Finish();
}


const unsigned BUFFER_SIZE = 512 * 512;

template <typename T>
static size_t sCompress1stPhase(const Settings& settings, unsigned width, unsigned height, unsigned channels,
                                const T* input, void* output, Compression& out_compression)
{
	size_t compressed_size = 0;

	if (settings.compression == Compression::None)
		goto fallback;

	// Compress following quantization (including no quantization at all)
	if (settings.quantization > 0.0F || settings.ratio < 1.0F)
	{
		auto compressor = CompressorKagari<T>(BUFFER_SIZE, sizeof(T) * width * height * channels, output);
		out_compression = Compression::Kagari;

		if (sCompress2ndPhase(compressor, settings, width, height, channels, input) == 0 ||
		    (compressed_size = compressor.Finish()) == 0)
		{
			printf("!!! Compression fallback !!!\n"); // TODO, use callbacks
			goto fallback;
		}
	}

	// Compress iterating until meet a certain ratio
	else
	{
		const auto target_size = static_cast<float>((sizeof(T) >> 1) * width * height * channels) / settings.ratio;
		out_compression = Compression::Kagari;

		auto s = settings;
		auto q_floor = 1.0F;
		auto q_ceil = 1.0F;
		auto q = 1.0F;

		// Find ceil
		while (1)
		{
			q_floor = q_ceil;
			q_ceil *= 2.0F;
			s.quantization = q_ceil;
			// printf("%.4f\n", q_ceil);

			auto compressor = CompressorKagari<T>(BUFFER_SIZE, static_cast<size_t>(target_size), output);
			if ((compressed_size = sCompress2ndPhase(compressor, s, width, height, channels, input)) != 0)
				break;

			// TODO, bad hardcoded limit, but technically quantization doesn't have a maximum right now
			if (q_ceil > 4096.0F)
			{
				printf("!!! Compression fallback !!!\n"); // TODO, use callbacks
				goto fallback;
			}
		}

		// Divide space by two
		for (int i = 0; i < 8; i += 1)
		{
			q = (q_floor + q_ceil) / 2.0F;
			s.quantization = q;
			// printf("%.4f, %.4f -> %.4f\n", q_floor, q_ceil, q);

			auto compressor = CompressorKagari<T>(BUFFER_SIZE, static_cast<size_t>(target_size), output);
			compressed_size = sCompress2ndPhase(compressor, s, width, height, channels, input);

			if (target_size > static_cast<float>(compressed_size) && compressed_size != 0)
				q_ceil = q;
			else
				q_floor = q;
		}

		// One more time!
		{
			s.quantization = q_ceil;
			printf("%.4f\n", q_ceil);

			auto compressor = CompressorKagari<T>(BUFFER_SIZE, static_cast<size_t>(target_size), output);
			compressed_size = sCompress2ndPhase(compressor, s, width, height, channels, input);
		}
	}

	return compressed_size;

fallback:
	auto compressor = CompressorNone<T>(BUFFER_SIZE, output);
	out_compression = Compression::None;

	sCompress2ndPhase(compressor, settings, width, height, channels, input);
	compressed_size = compressor.Finish();

	return compressed_size;
}


template <>
size_t Compress(const Settings& settings, unsigned width, unsigned height, unsigned channels, const int16_t* input,
                void* output, Compression& out_compression)
{
	return sCompress1stPhase(settings, width, height, channels, input, output, out_compression);
}

template <>
size_t Compress(const Settings& settings, unsigned width, unsigned height, unsigned channels, const int32_t* input,
                void* output, Compression& out_compression)
{
	return sCompress1stPhase(settings, width, height, channels, input, output, out_compression);
}

} // namespace ako
