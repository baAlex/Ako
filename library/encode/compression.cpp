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

#include "ako-private.hpp"
#include "compression-kagari.hpp"
#include "compression-none.hpp"

namespace ako
{

template <typename T>
static void sQuantizer(bool vertical, float q, unsigned width, unsigned height, unsigned input_stride,
                       unsigned output_stride, const T* in, T* out)
{
	(void)vertical;

	if (std::isnan(q) == false && q > 1.0F && width > 32 && height > 32)
	{
		if (std::isinf(q) == true)
		{
			for (unsigned row = 0; row < height; row += 1)
			{
				for (unsigned col = 0; col < width; col += 1)
					out[col] = 0;
				out += output_stride;
			}
			return;
		}

		for (unsigned row = 0; row < height; row += 1)
		{
			for (unsigned col = 0; col < width; col += 1)
			{
				// Quantization with integers
				out[col] = static_cast<T>((in[col] / static_cast<T>(q)) * static_cast<T>(q));

				// Quantization with floats
				// out[col] = static_cast<T>(std::floor(static_cast<float>(in[col]) / q + 0.5F) * q);

				// Gate (looks horribly pixelated than previous two):
				// out[col] = (std::abs(static_cast<float>(in[col])) < q) ? 0 : in[col];
			}

			in += input_stride;
			out += output_stride;
		}

		return;
	}

	for (unsigned row = 0; row < height; row += 1)
	{
		for (unsigned col = 0; col < width; col += 1)
			out[col] = in[col];
		in += input_stride;
		out += output_stride;
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
		if (compressor.Step(sQuantizer, 1.0F, true, lp_w, lp_h, in) != 0)
			return 0;

		in += (lp_w * lp_h); // Quadrant A
	}

	// Highpasses
	for (unsigned lift = (lifts_no - 1); lift < lifts_no; lift -= 1) // Underflows
	{
		LiftMeasures(lift, width, height, lp_w, lp_h, hp_w, hp_h);

		// Quantization step
		const auto x = (static_cast<float>(lifts_no) - static_cast<float>(lift)) / static_cast<float>(lifts_no);
		const auto q = std::pow(2.0F, std::pow(x, 3.0F) * (std::log2f(settings.quantization)));
		const float q_diagonal = (settings.quantization > 0.0F) ? 2.0F : 1.0F;

		// printf("x: %f, q: %f\n", x, q);

		// Iterate in Yuv order
		for (unsigned ch = 0; ch < channels; ch += 1)
		{
			const float q_sub = (ch == 0 || settings.quantization == 0.0F) ? 1.0F : 2.0F;

			// Quadrant C
			if (compressor.Step(sQuantizer, (q * q_sub), true, lp_w, hp_h, in) != 0)
				return 0;

			in += (lp_w * hp_h);

			// Quadrant B
			if (compressor.Step(sQuantizer, (q * q_sub), true, hp_w, lp_h, in) != 0)
				return 0;

			in += (hp_w * lp_h);

			// Quadrant D
			if (compressor.Step(sQuantizer, (q * q_sub * q_diagonal), true, hp_w, hp_h, in) != 0)
				return 0;

			in += (hp_w * hp_h);
		}
	}

	return compressor.Finish();
}


const unsigned TRY = 8;
const float ITERATION_SCALE = 2.0F;


template <typename T>
static size_t sCompress1stPhase(const Callbacks& callbacks, const Settings& settings, unsigned width, unsigned height,
                                unsigned channels, const T* input, void* output, Compression& out_compression)
{
	size_t compressed_size = 0;

	if (settings.compression != Compression::None)
	{
		// Initialization
		auto target_size = sizeof(T) * width * height * channels;
		auto compressor = CompressorKagari(BLOCK_WIDTH, BLOCK_HEIGHT, target_size, output);
		out_compression = Compression::Kagari;

		auto s = settings;
		auto q_floor = settings.quantization;
		auto q_ceil = settings.quantization;
		s.quantization = settings.quantization;

		if (settings.quantization == 0.0F && settings.ratio >= 1.0F) // TODO, repeated check
		{
			q_floor = 0.0;
			q_ceil = 0.0;
			s.quantization = 0.0F;
			target_size =
			    static_cast<size_t>(static_cast<float>((sizeof(T) >> 1) * width * height * channels) / settings.ratio);
		}

		const auto error_margin = (target_size * 4) / 100;

		// First floor (quantization as specified, may be zero)
		{
			compressor.Reset(target_size, output);
			if (callbacks.generic_event != nullptr)
				callbacks.generic_event(GenericEvent::RatioIteration, 0, 0, 0,
				                        GenericTypeDesignatedInitialization(s.quantization), callbacks.user_data);

			compressor.Reset(target_size, output);
			if ((compressed_size = sCompress2ndPhase(compressor, s, width, height, channels, input)) == 0)
			{
				if (settings.ratio < 1.0F)
					goto fallback;
			}
		}

		// Iterate?
		if (settings.quantization == 0.0F && settings.ratio >= 1.0F && //
		    ((compressed_size != 0 && compressed_size > target_size) || compressed_size == 0))
		{
			// Find ceil
			while (1)
			{
				q_floor = q_ceil;
				q_ceil = (q_ceil != 0.0) ? (q_ceil * ITERATION_SCALE) : ITERATION_SCALE;
				s.quantization = q_ceil;

				if (callbacks.generic_event != nullptr)
					callbacks.generic_event(GenericEvent::RatioIteration, 0, 0, 0,
					                        GenericTypeDesignatedInitialization(s.quantization), callbacks.user_data);

				compressor.Reset(target_size, output);
				if ((compressed_size = sCompress2ndPhase(compressor, s, width, height, channels, input)) != 0)
					break;

				if (s.quantization > 4096.0F) // TODO, horribly hardcoded!
					goto fallback;
			}

			// Divide space in two, between floor and ceil
			for (unsigned i = 0; i < TRY; i += 1)
			{
				const auto q = (q_floor + q_ceil) / 2.0F;
				s.quantization = q;

				if (callbacks.generic_event != nullptr)
					callbacks.generic_event(GenericEvent::RatioIteration, 0, 0, 0,
					                        GenericTypeDesignatedInitialization(s.quantization), callbacks.user_data);

				compressor.Reset(target_size, output);
				compressed_size = sCompress2ndPhase(compressor, s, width, height, channels, input);

				if (compressed_size < target_size && compressed_size != 0)
				{
					q_ceil = q;
					if (target_size - compressed_size < error_margin)
						break;
				}
				else
					q_floor = q;
			}

			// Last iteration being too close to floor, failed
			if (s.quantization != q_ceil)
			{
				s.quantization = q_ceil;

				if (callbacks.generic_event != nullptr)
					callbacks.generic_event(GenericEvent::RatioIteration, 0, 0, 0,
					                        GenericTypeDesignatedInitialization(s.quantization), callbacks.user_data);

				compressor.Reset(target_size, output);
				compressed_size = sCompress2ndPhase(compressor, s, width, height, channels, input);
			}
		}

		// Bye!
		if (callbacks.histogram_event != nullptr)
			callbacks.histogram_event(compressor.GetHistogram(), compressor.GetHistogramLength(), callbacks.user_data);

		return compressed_size;
	}

fallback:
{
	auto compressor = CompressorNone<T>(BLOCK_LENGTH, output);
	out_compression = Compression::None;

	return sCompress2ndPhase(compressor, settings, width, height, channels, input);
}
}


template <>
size_t Compress(const Callbacks& callbacks, const Settings& settings, unsigned width, unsigned height,
                unsigned channels, const int16_t* input, void* output, Compression& out_compression)
{
	return sCompress1stPhase(callbacks, settings, width, height, channels, input, output, out_compression);
}

template <>
size_t Compress(const Callbacks& callbacks, const Settings& settings, unsigned width, unsigned height,
                unsigned channels, const int32_t* input, void* output, Compression& out_compression)
{
	(void)callbacks;
	(void)settings;
	(void)width;
	(void)height;
	(void)channels;
	(void)input;
	(void)output;
	(void)out_compression;
	return 0;
}

} // namespace ako
