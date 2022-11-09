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

template <typename T> class DecompressorNone : public Decompressor<T>
{
  private:
	const T* input_start;
	const T* input_end;
	const T* input;

  public:
	DecompressorNone(const void* input, size_t input_size)
	{
		this->input_start = reinterpret_cast<const T*>(input);
		this->input_end = reinterpret_cast<const T*>(input) + input_size / sizeof(T);
		this->input = reinterpret_cast<const T*>(input);
	}

	Status Step(unsigned width, unsigned height, T* out)
	{
		if (this->input + (width * height) > this->input_end)
			return Status::TruncatedTileData;

		if (SystemEndianness() == Endianness::Little)
		{
			for (unsigned i = 0; i < (width * height); i += 1)
				out[i] = this->input[i];
		}
		else
		{
			for (unsigned i = 0; i < (width * height); i += 1)
				out[i] = EndiannessReverse<T>(this->input[i]);
		}

		this->input += (width * height);
		return Status::Ok;
	}
};

} // namespace ako
