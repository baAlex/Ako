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

template <typename T> class DecompressorKagari : public Decompressor<T>
{
  private:
	const T* input;
	const T* input_start;
	const T* input_end;

  public:
	DecompressorKagari(const void* input, size_t input_size)
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

} // namespace ako
