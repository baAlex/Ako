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

template <typename T> class DummyDecompressor : public Decompressor<T>
{
  private:
	const T* input;
	const T* input_start;
	const T* input_end;

  public:
	DummyDecompressor(const void* input, size_t input_size)
	{
		this->input = reinterpret_cast<const T*>(input);
		this->input_start = reinterpret_cast<const T*>(input);
		this->input_end = reinterpret_cast<const T*>(input) + input_size / sizeof(T);
	}

	size_t Step(unsigned width, unsigned height, T* out, Status& status)
	{
		if (input + (width * height) > this->input_end)
		{
			status = Status::TruncatedTileData;
			return 0;
		}

		if (SystemEndianness() == Endianness::Little)
		{
			for (unsigned i = 0; i < (width * height); i += 1)
				out[i] = input[i];
		}
		else
		{
			for (unsigned i = 0; i < (width * height); i += 1)
				out[i] = EndiannessReverse<T>(input[i]);
		}

		input += (width * height);
		return (width * height) * sizeof(T);
	}

	size_t Finish(Status& status) const
	{
		status = Status::Ok;
		return static_cast<size_t>(input - input_start) * sizeof(T);
	}
};


template <typename T>
static size_t sDecompress(Decompressor<T>& decompressor, unsigned width, unsigned height, unsigned channels, T* output,
                          Status& status)
{
	// Code suspiciously similar to Unlift()

	auto out = output;
	unsigned lp_w = width;
	unsigned lp_h = height;
	unsigned hp_w, hp_h;

	const auto lifts_no = LiftsNo(width, height);
	if (lifts_no > 1)
		LiftMeasures((lifts_no - 1), width, height, lp_w, lp_h, hp_w, hp_h);

	// Lowpasses
	for (unsigned ch = 0; ch < channels; ch += 1)
	{
		if (decompressor.Step(lp_w, lp_h, out, status) == 0)
			return 0;

		out += (lp_w * lp_h); // Quadrant A
	}

	// Highpasses
	for (unsigned lift = (lifts_no - 1); lift < lifts_no; lift -= 1) // Underflows
	{
		LiftMeasures(lift, width, height, lp_w, lp_h, hp_w, hp_h);

		// Iterate in Yuv order
		for (unsigned ch = 0; ch < channels; ch += 1)
		{
			// Quadrant C
			if (decompressor.Step(lp_w, hp_h, out, status) == 0)
				return 0;

			out += (lp_w * hp_h);

			// Quadrant B
			if (decompressor.Step(hp_w, lp_h, out, status) == 0)
				return 0;

			out += (hp_w * lp_h);

			// Quadrant D
			if (decompressor.Step(hp_w, hp_h, out, status) == 0)
				return 0;

			out += (hp_w * hp_h);
		}
	}

	return decompressor.Finish(status);
}


template <>
size_t Decompress(const Settings& settings, size_t compressed_size, unsigned width, unsigned height, unsigned channels,
                  const void* input, int16_t* output, Status& status)
{
	size_t decompressed_size = 0;

	if (settings.compression == Compression::Kagari)
	{
		// TODO
	}

	// No compression
	{
		auto decompressor = DummyDecompressor<int16_t>(input, compressed_size);
		decompressed_size = sDecompress(decompressor, width, height, channels, output, status);
	}

	return decompressed_size;
}

template <>
size_t Decompress(const Settings& settings, size_t compressed_size, unsigned width, unsigned height, unsigned channels,
                  const void* input, int32_t* output, Status& status)
{
	size_t decompressed_size = 0;

	if (settings.compression == Compression::Kagari)
	{
		// TODO
	}

	// No compression
	{
		auto decompressor = DummyDecompressor<int32_t>(input, compressed_size);
		decompressed_size = sDecompress(decompressor, width, height, channels, output, status);
	}

	return decompressed_size;
}

} // namespace ako
