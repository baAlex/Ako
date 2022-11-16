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

class CompressorNone final : public Compressor
{
  private:
	void* output_start;
	void* output;

	size_t buffer_size;
	void* buffer;

	template <typename T>
	int InternalStep(QuantizationCallback<T> quantize, float quantization, unsigned width, unsigned height,
	                 const T* input)
	{
		auto input_length = (width * height);
		auto buffer = reinterpret_cast<T*>(this->buffer);
		auto out = reinterpret_cast<T*>(this->output);

		do
		{
			const auto len = Min(static_cast<unsigned>(this->buffer_size / sizeof(T)), input_length);

			// Quantize input
			quantize(quantization, len, input, buffer);
			input_length -= len;
			input += len;

			// Write output
			if (SystemEndianness() == Endianness::Little)
			{
				for (unsigned i = 0; i < len; i += 1)
					out[i] = buffer[i];
			}
			else
			{
				for (unsigned i = 0; i < len; i += 1)
					out[i] = EndiannessReverse(buffer[i]);
			}

			out += len;

		} while (input_length != 0);

		// Bye!
		this->output = out;
		return 0;
	}

  public:
	CompressorNone(size_t buffer_length, void* output)
	{
		this->output_start = output;
		this->output = output;

		this->buffer_size = buffer_length * sizeof(int32_t); // Int32 being a common case
		this->buffer = malloc(this->buffer_size);
	}

	~CompressorNone()
	{
		free(this->buffer);
	}

	int Step(QuantizationCallback<int32_t> quantize, float quantization, unsigned width, unsigned height,
	         const int32_t* in) override
	{
		return InternalStep(quantize, quantization, width, height, in);
	}

	int Step(QuantizationCallback<int16_t> quantize, float quantization, unsigned width, unsigned height,
	         const int16_t* in) override
	{
		return InternalStep(quantize, quantization, width, height, in);
	}

	size_t Finish() override
	{
		return static_cast<size_t>(static_cast<uint8_t*>(this->output) - static_cast<uint8_t*>(this->output_start));
	}
};

} // namespace ako
