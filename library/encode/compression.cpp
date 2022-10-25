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

template <typename T> class Compressor
{
  private:
	uint8_t* output;
	size_t cursor; // Or 'compressed size'

  public:
	Compressor(void* output)
	{
		this->output = reinterpret_cast<uint8_t*>(output);
		this->cursor = 0;
	}

	int Step(T (*quantize)(float, T), float quantization, unsigned width, unsigned height, const T* in)
	{
		auto out = reinterpret_cast<T*>(output + cursor);

		if (SystemEndianness() == Endianness::Little)
		{
			for (unsigned i = 0; i < (width * height); i += 1)
				out[i] = quantize(quantization, in[i]);
		}
		else
		{
			for (unsigned i = 0; i < (width * height); i += 1)
				out[i] = EndiannessReverse<T>(quantize(quantization, in[i]));
		}

		cursor += sizeof(T) * width * height;
		return 0;
	}

	size_t Finish() const
	{
		return cursor;
	}
};


template <typename T> static inline T sDummyQuantizer(float q, T value)
{
	if (value > 0)
		return static_cast<T>(roundf(fabsf(static_cast<float>(value) / q)) * q);

	return static_cast<T>(roundf(fabsf(static_cast<float>(value) / q)) * (-q));
}


template <typename T>
static size_t sCompress(const Settings& settings, unsigned width, unsigned height, unsigned channels,
                        Compressor<T>& compressor, const T* input)
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
		if (compressor.Step(sDummyQuantizer, 1.0F, lp_w, lp_h, in) != 0)
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
		                                                          // the default for quantization (I think)...
		                                                          // TODO: revise this again with the compression
		                                                          // stage working

		const auto q = powf(2.0F, x * (settings.quantization / 100.0F) * powf(x / 8.0F, 1.8F));
		const float q_diagonal = (settings.quantization > 0.0F) ? 2.0F : 1.0F;

		// printf("%f %f\n", q, x);

		// Iterate in Yuv order
		for (unsigned ch = 0; ch < channels; ch += 1)
		{
			// Quadrant C
			if (compressor.Step(sDummyQuantizer, (x > 0.0) ? q : 1.0F, lp_w, hp_h, in) != 0)
				return 0;

			in += (lp_w * hp_h);

			// Quadrant B
			if (compressor.Step(sDummyQuantizer, (x > 0.0) ? q : 1.0F, hp_w, lp_h, in) != 0)
				return 0;

			in += (hp_w * lp_h);

			// Quadrant D
			if (compressor.Step(sDummyQuantizer, (x > 0.0) ? (q * q_diagonal) : 1.0F, hp_w, hp_h, in) != 0)
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
	auto compressor = Compressor<int16_t>(output);
	return sCompress(settings, width, height, channels, compressor, input);
}

template <>
size_t Compress(const Settings& settings, unsigned width, unsigned height, unsigned channels, const int32_t* input,
                void* output)
{
	auto compressor = Compressor<int32_t>(output);
	return sCompress(settings, width, height, channels, compressor, input);
}

} // namespace ako
