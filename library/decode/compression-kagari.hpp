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
	size_t block_length;

	const uint32_t* input_start;
	const uint32_t* input_end;
	const uint32_t* input;

	int32_t ZigZagDecode(uint32_t in) const
	{
		return static_cast<int32_t>((in >> 1) ^ (~(in & 1) + 1));
	}

  public:
	DecompressorKagari(size_t block_length, const void* input, size_t input_size)
	{
		this->block_length = block_length;
		this->input_start = reinterpret_cast<const uint32_t*>(input);
		this->input_end = reinterpret_cast<const uint32_t*>(input) + input_size / sizeof(uint32_t);
		this->input = reinterpret_cast<const uint32_t*>(input);
	}

	Status Step(unsigned width, unsigned height, T* out)
	{
		const T* out_end = out + width * height;

		while (out < out_end)
		{
			const T* out_block_end = Min<const T*>(out + this->block_length, out_end);

			// Decompress block
			while (out < out_block_end)
			{
				T last_value = 0;

				// Literal
				{
					if (this->input == this->input_end)
						return Status::Error;

					const uint32_t length = *this->input++;

					if (out + length > out_block_end || this->input + length > this->input_end)
						return Status::Error;

					for (uint32_t i = 0; i < length; i += 1)
						*out++ = static_cast<T>(ZigZagDecode(*this->input++));

					last_value = *(out - 1);

					// Developers, developers, developers
					// printf("\tD [Lit, len: %u, v: '", length);
					// for (unsigned i = 0; i < length; i += 1)
					// 	printf("%c", static_cast<char>(ZigZagDecode((input - length)[i])));
					// printf("']\n");

					// All done?
					if (out == out_block_end)
						break;
				}

				// Rle
				{
					if (this->input == this->input_end)
						return Status::Error;

					const uint32_t length = *this->input++;

					if (out + length > out_block_end)
						return Status::Error;

					for (uint32_t i = 0; i < length; i += 1)
						*out++ = last_value;

					// Developers, developers, developers
					// printf("\tD [Rle, len: %u, v: '%c']\n", length, last_value);
				}
			}
		}

		return Status::Ok;
	}
};

} // namespace ako
