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

template <typename T> class CompressorNone : public Compressor<T>
{
  private:
	T* output_start;
	T* output;

  public:
	CompressorNone(void* output)
	{
		this->output_start = reinterpret_cast<T*>(output);
		this->output = reinterpret_cast<T*>(output);
	}

	int Step(T (*quantize)(float, T), float quantization, unsigned width, unsigned height, const T* in)
	{
		if (SystemEndianness() == Endianness::Little)
		{
			for (unsigned i = 0; i < (width * height); i += 1)
				this->output[i] = quantize(quantization, in[i]);
		}
		else
		{
			for (unsigned i = 0; i < (width * height); i += 1)
				this->output[i] = EndiannessReverse<T>(quantize(quantization, in[i]));
		}

		this->output += (width * height);
		return 0;
	}

	size_t Finish()
	{
		return static_cast<size_t>(this->output - this->output_start) * sizeof(T);
	}
};

} // namespace ako
