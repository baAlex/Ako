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


// ----------------------------

#define ALGO 3

template <typename T> static void sQuantize(float step, unsigned w, unsigned h, const T* in, T* out)
{
	// TODO, endianness
	if (step > 1.0F && w > 4 && h > 4) // HARDCODED
	{
		for (unsigned i = 0; i < (w * h); i += 1)
		{

			if (in[i] > 0)
				out[i] = static_cast<T>(__builtin_roundf(__builtin_fabsf(static_cast<float>(in[i]) / step)) * step);
			else
				out[i] = static_cast<T>(__builtin_roundf(__builtin_fabsf(static_cast<float>(in[i]) / step)) * step *
				                        (-1.0F));
		}
	}
	else
	{
		for (unsigned i = 0; i < (w * h); i += 1)
			out[i] = in[i];
	}
}

template <typename T>
static size_t sKagari(unsigned quantization, unsigned chroma_loss, unsigned width, unsigned height, unsigned channels,
                      const T* input, void* output)
{
	T* out = reinterpret_cast<T*>(output);

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
		for (unsigned i = 0; i < lp_w * lp_h; i += 1)
			out[i] = in[i];

		in += (lp_w * lp_h); // Quadrant A
		out += (lp_w * lp_h);
	}

	// Highpasses
	for (unsigned lift = (lifts_no - 1); lift < lifts_no; lift -= 1) // Underflows
	{
		LiftMeasures(lift, width, height, lp_w, lp_h, hp_w, hp_h);

		// Quantization step
		const auto x = static_cast<float>(lifts_no - lift - 1);

#if (ALGO == 0)
		const auto q_step = (x * x) * (static_cast<float>(quantization) / 512.0F) + 1.0F;
#elif (ALGO == 1)
		const auto q_step = __builtin_powf(2.0F, x * (static_cast<float>(quantization) / 256.0F));
#elif (ALGO == 2) // AkoV2'ish
		const auto q_step =
		    __builtin_powf(2.0F, x * (static_cast<float>(quantization) / 256.0F) * __builtin_powf(x / 8.0F, 1.0F));
#elif (ALGO == 3) // Ditto, different curve
		const auto q_step =
		    __builtin_powf(2.0F, x * (static_cast<float>(quantization) / 300.0F) * __builtin_powf(x / 8.0F, 1.8F));
#endif

		// Iterate in Yuv order
		for (unsigned ch = 0; ch < channels; ch += 1)
		{
			const float chroma =
			    (channels >= 3 && chroma_loss > 1 && (ch == 1 || ch == 2)) ? static_cast<float>(chroma_loss) : 1.0F;
			const float diagonal = (quantization != 0) ? 2.0F : 1.0F;

			// Quadrant C
			sQuantize(q_step * chroma, lp_w, hp_h, in, out);
			in += (lp_w * hp_h);
			out += (lp_w * hp_h);

			// Quadrant B
			sQuantize(q_step * chroma, hp_w, lp_h, in, out);
			in += (hp_w * lp_h);
			out += (hp_w * lp_h);

			// Quadrant D
			sQuantize(q_step * chroma * diagonal, hp_w, hp_h, in, out);
			in += (hp_w * hp_h);
			out += (hp_w * hp_h);
		}
	}

	return TileDataSize<T>(width, height, channels);
}

// ----------------------------


template <typename T>
static size_t sCompress(const Settings& settings, unsigned width, unsigned height, unsigned channels, const T* input,
                        void* output)
{
	if (settings.compression == Compression::Kagari)
		return sKagari(settings.quantization, settings.chroma_loss, width, height, channels, input, output);

	// No compression
	{
		const auto compressed_size = TileDataSize<T>(width, height, channels);

		if (SystemEndianness() == Endianness::Little)
			Memcpy(output, input, compressed_size);
		else
			MemcpyReversingEndianness(compressed_size, input, output);

		return compressed_size;
	}
}


template <>
size_t Compress(const Settings& settings, unsigned width, unsigned height, unsigned channels, const int16_t* input,
                void* output)
{
	return sCompress(settings, width, height, channels, input, output);
}

template <>
size_t Compress(const Settings& settings, unsigned width, unsigned height, unsigned channels, const int32_t* input,
                void* output)
{
	return sCompress(settings, width, height, channels, input, output);
}

} // namespace ako
