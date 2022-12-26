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

class KagariBitReader
{
	static const uint32_t ACC_LEN = 32;

	const uint32_t* input_end;
	const uint32_t* input;

	uint32_t accumulator;
	uint32_t accumulator_usage;

  public:
	KagariBitReader(size_t input_length, const uint32_t* input)
	{
		this->input_end = input + input_length;
		this->input = input;

		this->accumulator = 0;
		this->accumulator_usage = 0;
	}

	int ReadBits(uint32_t len, uint32_t& v)
	{
		// Accumulator contains our value, ideal fast path
		if (len <= this->accumulator_usage)
		{
			const auto mask = static_cast<uint32_t>(1 << len) - 1;

			this->accumulator_usage -= len;
			v = (this->accumulator >> this->accumulator_usage) & mask;
		}

		// Accumulator doesn't have it, at least entirely, ultra super duper slow path
		else
		{
			if (this->input + 1 > this->input_end || len >= ACC_LEN)
				return 1;

			// Keep what accumulator has
			const auto min = this->accumulator_usage;
			const auto mask = static_cast<uint32_t>(1 << min) - 1;

			v = this->accumulator & mask;

			// Read, make value with remainder part
			this->accumulator = *this->input++;
			this->accumulator_usage = ACC_LEN - (len - min);

			v |= (this->accumulator >> this->accumulator_usage) << min;

			// Developers, developers, developers
			// printf("D |\tRead!, d: 0x%X, min: %u\n", *(this->input - 1), min);
		}

		// Developers, developers, developers
		// printf("D | v: %u,\tl: %u, u: %u (%u)\n", v, len, this->accumulator_usage, ACC_LEN -
		// this->accumulator_usage);

		// Bye!
		return 0;
	}
};

template <typename T> class DecompressorKagari final : public Decompressor<T>
{
  private:
	const uint8_t* input_end;
	const uint8_t* input;

	T* buffer_start;
	T* buffer_end;

	unsigned buffer_usage;
	T* buffer;

	int32_t ZigZagDecode(uint32_t length, const uint32_t* in, int32_t* out) const
	{
		// if (length == 0) // A linter will expect this
		// 	return 0;

		for (uint32_t i = 0; i < length; i += 1)
			out[i] = static_cast<int32_t>((in[i] >> 1) ^ (~(in[i] & 1) + 1));
		return out[length - 1]; // NOLINT
	}

	int16_t ZigZagDecode(uint32_t length, const uint16_t* in, int16_t* out) const
	{
		// if (length == 0) // A linter will expect this
		// 	return 0;

		for (uint32_t i = 0; i < length; i += 1)
			out[i] = static_cast<int16_t>((in[i] >> 1) ^ (~(in[i] & 1) + 1));
		return out[length - 1]; // NOLINT
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
			const uint32_t rle_length = *in_u32++;
			const uint32_t literal_length = (*in_u32++) + 1; // Notice the +1

			// Checks
			if (out + rle_length + literal_length > out_end ||
			    this->input + (sizeof(uint32_t) * 2) + (sizeof(T) * literal_length) > this->input_end)
				return 1;

			// Write Rle values
			for (uint32_t i = 0; i < rle_length; i += 1)
				out[i] = rle_value;

			// Write literal values
			rle_value =
			    ZigZagDecode(literal_length, FlipSignCast(reinterpret_cast<const T*>(in_u32)), out + rle_length);

			// Developers, developers, developers
			{
				// printf("\tD [Rle, l: %u, v: '%c']", rle_length, static_cast<char>(rle_value));
				// printf(" [Lit, l: %u, v: '", literal_length);
				// for (unsigned i = 0; i < literal_length; i += 1)
				//	printf("%c", static_cast<char>(out[rle_length + i]));
				// printf("']\n");
			}

			// Update pointers
			this->input += (sizeof(uint32_t) * 2) + (sizeof(T) * literal_length);
			out += rle_length + literal_length;
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
