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
static void sQuantizer(float q, unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                       const T* in, T* out)
{
	if (q >= 1.0F && std::isnan(q) == false && std::isinf(q) == false)
	{
		for (unsigned row = 0; row < height; row += 1)
		{
			q = std::floor(q);

			for (unsigned col = 0; col < width; col += 1)
			{
				// Quantization with integers
				// out[col] = static_cast<T>((in[col] / static_cast<T>(q)) * static_cast<T>(q));

				// Gate, looks horribly pixelated if alone, but here is acting as dead-zone:
				out[col] = (std::abs(static_cast<float>(in[col])) < q / 2.0F) ? 0 : in[col]; // 2.0 - 1.0

				// Quantization with floats
				out[col] = static_cast<T>(std::floor(static_cast<float>(out[col]) / q + 0.5F) * q);

				// Old Ako
				// out[col] = static_cast<T>(std::trunc(static_cast<float>(in[col]) / q) * q);
			}

			in += input_stride;
			out += output_stride;
		}
	}
	else
	{
		for (unsigned row = 0; row < height; row += 1)
		{
			for (unsigned col = 0; col < width; col += 1)
				out[col] = 0;
			out += output_stride;
		}
	}
}


static float sCurve(float power, float x)
{
	const auto a = (1.0F / 16.0F);
	if (x < a)
		return 0.0F; // Quantize only last 16 lifts

	return std::pow(x - a, power + (power * a)); // Emphasis on last lifts
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
		const auto x = (static_cast<float>(lifts_no) - static_cast<float>(lift)) / static_cast<float>(lifts_no);

		// Iterate in Yuv order
		for (unsigned ch = 0; ch < channels; ch += 1)
		{
			// Quantization
			auto q = std::log2f(settings.quantization) * sCurve(settings.quantization_power / 1.0F, x);
			auto q_diagonal = std::log2f(settings.quantization) * sCurve(settings.quantization_power / 2.0F, x);

			if (ch != 0)
			{
				q *= settings.chroma_loss;
				q_diagonal *= settings.chroma_loss;
			}

			q = std::pow(2.0F, q);
			q_diagonal = std::pow(2.0F, q_diagonal);

			if (settings.quantization > 1.0F)
				q_diagonal = Min(q * 2.0F, q_diagonal);

			// Quadrant C
			if (compressor.Step(sQuantizer, q, lp_w, hp_h, in) != 0)
				return 0;
			in += (lp_w * hp_h);

			// Quadrant B
			if (compressor.Step(sQuantizer, q, hp_w, lp_h, in) != 0)
				return 0;
			in += (hp_w * lp_h);

			// Quadrant D
			if (compressor.Step(sQuantizer, q_diagonal, hp_w, hp_h, in) != 0)
				return 0;
			in += (hp_w * hp_h);
		}
	}

	return compressor.Finish();
}


const unsigned TRY = 8;
const float ITERATION_SCALE = 4.0F;


template <typename T>
static size_t sCompress1stPhase(const Callbacks& callbacks, const Settings& settings, unsigned width, unsigned height,
                                unsigned channels, const T* input, void* output, Compression& out_compression)
{
	if (settings.compression != Compression::None)
	{
		// Initializations
		auto s = settings;
		auto compressor = CompressorKagari(BLOCK_WIDTH, BLOCK_HEIGHT, 0, output);
		out_compression = Compression::Kagari;

		size_t target_size = sizeof(T) * width * height * channels;
		float initial_q = Max(settings.quantization, 1.0F);
		if (settings.ratio >= 1.0F)
		{
			target_size = static_cast<size_t>(static_cast<float>(target_size >> 1) / settings.ratio);
			initial_q = 1.0F;
		}

		const size_t error_margin = (target_size * 2) / 100;

		// Find a ceil
		size_t compressed_size;
		float q_floor = initial_q;
		float q_ceil = initial_q;
		while (1)
		{
			s.quantization = q_ceil;
			if (std::isinf(s.quantization) == true)
				goto fallback;

			// User feedback
			if (callbacks.generic_event != nullptr)
				callbacks.generic_event(GenericEvent::RatioIteration, 0, 0, 0,
				                        GenericTypeDesignatedInitialization(s.quantization), callbacks.user_data);

			// Compress
			compressor.Reset(target_size, output);
			compressed_size = sCompress2ndPhase(compressor, s, width, height, channels, input);
			if (compressed_size != 0)
				break;

			q_floor = q_ceil;
			q_ceil = q_ceil * ITERATION_SCALE;
		}

		if (settings.ratio <= 1.0F || q_floor == q_ceil || target_size - compressed_size < error_margin)
			return compressed_size;

		// Divide space in two, between floor and ceil
		for (unsigned i = 0; i < TRY; i += 1)
		{
			const auto q = (q_floor + q_ceil) / 2.0F;
			s.quantization = q;

			if (std::abs(q_floor - q_ceil) < 0.05F) // Epsilon
				break;

			// User feedback
			if (callbacks.generic_event != nullptr)
				callbacks.generic_event(GenericEvent::RatioIteration, 0, 0, 0,
				                        GenericTypeDesignatedInitialization(s.quantization), callbacks.user_data);

			// Compress
			compressor.Reset(target_size, output);
			compressed_size = sCompress2ndPhase(compressor, s, width, height, channels, input);

			if (compressed_size != 0)
			{
				q_ceil = q;
				if (target_size - compressed_size < error_margin)
					break;
			}
			else
				q_floor = q;
		}

		// One last time, as previous iteration end in a failure
		if (compressed_size == 0)
		{
			s.quantization = q_ceil;

			// User feedback
			if (callbacks.generic_event != nullptr)
				callbacks.generic_event(GenericEvent::RatioIteration, 0, 0, 0,
				                        GenericTypeDesignatedInitialization(s.quantization), callbacks.user_data);

			// Compress
			compressor.Reset(target_size, output);
			compressed_size = sCompress2ndPhase(compressor, s, width, height, channels, input);
		}

		// Bye!
		return compressed_size;
	fallback:
		s.quantization = NAN; // God is dead
		compressor.Reset(sizeof(T) * width * height * channels, output);
		return sCompress2ndPhase(compressor, s, width, height, channels, input);
	}
	else
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
