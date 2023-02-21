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

template <typename T> class DecompressorNone final : public Decompressor<T>
{
  private:
	const T* m_input_end;
	const T* m_input;

  public:
	DecompressorNone(size_t input_size, const void* input)
	{
		m_input_end = reinterpret_cast<const T*>(input) + input_size / sizeof(T);
		m_input = reinterpret_cast<const T*>(input);
	}

	Status Step(bool vertical, unsigned width, unsigned height, T* out) override
	{
		(void)vertical;

		if (m_input + (width * height) > m_input_end)
			return Status::TruncatedTileData;

		if (SystemEndianness() == Endianness::Little)
		{
			for (unsigned i = 0; i < (width * height); i += 1)
				out[i] = m_input[i];
		}
		else
		{
			for (unsigned i = 0; i < (width * height); i += 1)
				out[i] = EndiannessReverse<T>(m_input[i]);
		}

		m_input += (width * height);
		return Status::Ok;
	}
};

} // namespace ako
