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

class CompressorNone : public Compressor
{
  private:
	void* output_start;
	void* output;

	template <typename T>
	int InternalStep(T (*quantize)(float, T), float quantization, unsigned width, unsigned height, const T* in)
	{
		auto out = reinterpret_cast<T*>(this->output);

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

		this->output = out + (width * height);
		return 0;
	}

  public:
	CompressorNone(void* output)
	{
		this->output_start = output;
		this->output = output;
	}

	int Step(int32_t (*quantize)(float, int32_t), float quantization, unsigned width, unsigned height,
	         const int32_t* in)
	{
		return InternalStep(quantize, quantization, width, height, in);
	}

	int Step(int16_t (*quantize)(float, int16_t), float quantization, unsigned width, unsigned height,
	         const int16_t* in)
	{
		return InternalStep(quantize, quantization, width, height, in);
	}

	size_t Finish()
	{
		return static_cast<size_t>(static_cast<uint8_t*>(this->output) - static_cast<uint8_t*>(this->output_start));
	}
};

} // namespace ako
