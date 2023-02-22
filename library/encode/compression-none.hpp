/*

MIT License

Copyright (c) 2021-2023 Alexander Brandt

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

template <typename T> class CompressorNone final : public Compressor<T>
{
  private:
	T* m_output_start;
	T* m_output;

	unsigned m_buffer_length;
	T* m_buffer;

  public:
	CompressorNone(unsigned buffer_length, void* output)
	{
		m_output_start = reinterpret_cast<T*>(output);
		m_output = m_output_start;

		m_buffer_length = buffer_length;
		m_buffer = reinterpret_cast<T*>(malloc(buffer_length * sizeof(T)));
	}

	~CompressorNone()
	{
		free(m_buffer);
	}

	int Step(QuantizationCallback<T> quantize, float quantization, unsigned width, unsigned height,
	         const T* input) override
	{
		auto input_length = (width * height);
		do
		{
			const auto length = Min(m_buffer_length, input_length);

			// Quantize input
			quantize(quantization, length, 1, length, length, input, m_buffer);
			input_length -= length;
			input += length;

			// Write output
			if (SystemEndianness() == Endianness::Little)
			{
				for (unsigned i = 0; i < length; i += 1)
					*m_output++ = m_buffer[i];
			}
			else
			{
				for (unsigned i = 0; i < length; i += 1)
					*m_output++ = EndiannessReverse(m_buffer[i]);
			}

		} while (input_length != 0);

		// Bye!
		return 0;
	}

	size_t Finish() override
	{
		return static_cast<size_t>(m_output - m_output_start) * sizeof(T);
	}
};

} // namespace ako
