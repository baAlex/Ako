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

template <typename T> class DecompressorKagari final : public Decompressor<T>
{
  private:
	const uint8_t* input_end;
	const uint8_t* input;

	T* buffer_start;
	T* buffer_end;

	unsigned buffer_usage;
	T* buffer;

	int32_t ZigZagDecode(uint32_t in) const
	{
		return static_cast<int32_t>((in >> 1) ^ (~(in & 1) + 1));
	}

	int16_t ZigZagDecode(uint16_t in) const
	{
		return static_cast<int16_t>((in >> 1) ^ (~(in & 1) + 1));
	}

	const uint16_t* FlipSignCast(const int16_t* ptr)
	{
		return reinterpret_cast<const uint16_t*>(ptr);
	}

	const uint32_t* FlipSignCast(const int32_t* ptr)
	{
		return reinterpret_cast<const uint32_t*>(ptr);
	}

	int Decompress()
	{
		this->buffer = this->buffer_start; // We always decompress an entire buffer (as the encoder do)

		auto out_end = this->buffer_end;
		auto out = this->buffer_start;
		T rle_value = 0;

		while (out < out_end)
		{
			// Read Rle and literal lengths
			if (this->input + (sizeof(uint32_t) * 2) > this->input_end) // End reached, not an error
				break;

			auto in_u32 = reinterpret_cast<const uint32_t*>(this->input);
			uint32_t rle_length = *in_u32++;
			uint32_t literal_length = *in_u32++;

			// Checks
			if (out + rle_length + literal_length > out_end ||
			    this->input + (sizeof(uint32_t) * 2) + (sizeof(T) * literal_length) > this->input_end)
				return 1;

			// Update pointer
			this->input += (sizeof(uint32_t) * 2) + (sizeof(T) * literal_length);

			// Write Rle values
			// printf("\tD [Rle, l: %u, v: '%c']", rle_length, static_cast<char>(rle_value));
			if (rle_length > 0)
			{
				do
					*out++ = rle_value;
				while (--rle_length != 0);
			}

			// Write literal values
			// printf(" [Lit, l: %u, v: ?]\n", literal_length);
			if (literal_length > 0)
			{
				auto in_uT = FlipSignCast(reinterpret_cast<const T*>(in_u32));
				do
					*out++ = ZigZagDecode(*in_uT++);
				while (--literal_length != 0);

				rle_value = *(out - 1);
			}
		}

		// Bye!
		this->buffer_usage = static_cast<unsigned>(out - this->buffer_start);
		return 0;
	}

  public:
	DecompressorKagari(unsigned buffer_length, size_t input_size, const void* input)
	{
		this->input_end = reinterpret_cast<const uint8_t*>(input) + input_size;
		this->input = reinterpret_cast<const uint8_t*>(input);

		this->buffer_start = reinterpret_cast<T*>(malloc(buffer_length * sizeof(T)));
		this->buffer_end = this->buffer_start + buffer_length;

		this->buffer_usage = 0;
		this->buffer = this->buffer_start;
	}

	~DecompressorKagari()
	{
		free(this->buffer_start);
	}

	Status Step(unsigned width, unsigned height, T* out) override
	{
		auto output_length = (width * height);

		while (output_length != 0)
		{
			// If needed, decompress an entire buffer
			if (this->buffer_usage == 0)
			{
				if (Decompress() != 0)
					return Status::Error;
				if (this->buffer_usage == 0)
					return Status::Error;
			}

			// Move values that this buffer can provide
			{
				const auto length = Min(this->buffer_usage, output_length);

				for (unsigned i = 0; i < length; i += 1)
					out[i] = this->buffer[i];

				this->buffer_usage -= length;
				this->buffer += length;
				output_length -= length;
				out += length;
			}
		}

		// Bye!
		return Status::Ok;
	}
};

} // namespace ako
