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

class DecompressorNone final : public Decompressor
{
  private:
	const void* input_start;
	const void* input_end;
	const void* input;

	template <typename T> Status InternalStep(unsigned width, unsigned height, T* out)
	{
		const auto in = reinterpret_cast<const T*>(this->input);

		if (in + (width * height) > this->input_end)
			return Status::TruncatedTileData;

		if (SystemEndianness() == Endianness::Little)
		{
			for (unsigned i = 0; i < (width * height); i += 1)
				out[i] = in[i];
		}
		else
		{
			for (unsigned i = 0; i < (width * height); i += 1)
				out[i] = EndiannessReverse<T>(in[i]);
		}

		this->input = in + (width * height);
		return Status::Ok;
	}

  public:
	DecompressorNone(const void* input, size_t input_size)
	{
		this->input_start = input;
		this->input_end = reinterpret_cast<const uint8_t*>(input) + input_size;
		this->input = input;
	}

	Status Step(unsigned width, unsigned height, int16_t* out) override
	{
		return InternalStep(width, height, out);
	}

	Status Step(unsigned width, unsigned height, int32_t* out) override
	{
		return InternalStep(width, height, out);
	}
};

} // namespace ako
