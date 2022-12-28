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

template <typename T>
static int sDecompress(Decompressor<T>& decompressor, unsigned width, unsigned height, unsigned channels, T* output,
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
		if ((status = decompressor.Step(lp_w, lp_h, out)) != Status::Ok)
			return 1;

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
			if ((status = decompressor.Step(lp_w, hp_h, out)) != Status::Ok)
				return 1;

			out += (lp_w * hp_h);

			// Quadrant B
			if ((status = decompressor.Step(hp_w, lp_h, out)) != Status::Ok)
				return 1;

			out += (hp_w * lp_h);

			// Quadrant D
			if ((status = decompressor.Step(hp_w, hp_h, out)) != Status::Ok)
				return 1;

			out += (hp_w * hp_h);
		}
	}

	return 0;
}


const unsigned BUFFER_SIZE = 256 * 256;

template <>
int Decompress(Compression compression, size_t compressed_size, unsigned width, unsigned height, unsigned channels,
               const void* input, int16_t* output, Status& status)
{
	if (compression == Compression::Kagari)
	{
		auto decompressor = DecompressorKagari(BUFFER_SIZE, compressed_size, input);
		return sDecompress(decompressor, width, height, channels, output, status);
	}

	auto decompressor = DecompressorNone<int16_t>(compressed_size, input);
	return sDecompress(decompressor, width, height, channels, output, status);
}

template <>
int Decompress(Compression compression, size_t compressed_size, unsigned width, unsigned height, unsigned channels,
               const void* input, int32_t* output, Status& status)
{
	(void)compression;
	(void)compressed_size;
	(void)width;
	(void)height;
	(void)channels;
	(void)input;
	(void)output;
	(void)status;
	return 0;
}

} // namespace ako
