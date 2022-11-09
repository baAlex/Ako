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

template <typename TIn> // TODO, not optimal as the encoder converts everything to uint32
class CompressorKagari : public Compressor<TIn>
{
  private:
	static const size_t BLOCK_LEN = 25;
	static const unsigned RLE_TRIGGER = 4;

	uint32_t* output_start;
	uint32_t* output_end;
	uint32_t* output;

	uint32_t* block_start;
	uint32_t* block_end;
	uint32_t* block;

	uint32_t ZigZagEncode(int32_t in) const
	{
		return static_cast<uint32_t>((in << 1) ^ (in >> 31));
	}

	int32_t ZigZagDecode(uint32_t in) const
	{
		return static_cast<int32_t>((in >> 1) ^ (~(in & 1) + 1));
	}

	int EmitLiteral(unsigned length, const uint32_t* value)
	{
		// Developers, developers, developers
		printf("\tE [Lit, len: %u, v: '", length);
		for (unsigned i = 0; i < length; i += 1)
			printf("%c", static_cast<char>(ZigZagDecode(value[i])));
		printf("'] (%li/%li)\n", output - output_start, output_end - output_start);

		// Write to output
		if (output + 1 + length > output_end)
			return 1;

		*output++ = length;
		for (unsigned i = 0; i < length; i += 1)
			*output++ = *value++;

		return 0;
	}

	int EmitRle(unsigned length, uint32_t value)
	{
		// Developers, developers, developers
		printf("\tE [Rle, len: %u, v: '%c'] (%li/%li)\n", length, static_cast<char>(ZigZagDecode(value)),
		       output - output_start, output_end - output_start);

		// Write to output
		if (output + 1 > output_end)
			return 1;

		*output++ = length;
		return 0;
	}

	int Compress()
	{
		const auto block_len = static_cast<unsigned>(this->block - this->block_start);

		this->block = this->block_start;
		unsigned literal_len = 1;
		unsigned rle_len = 0;

		// Main loop
		for (unsigned i = 1; i < block_len; i += 1)
		{
			if (this->block[i] != this->block[i - 1])
			{
				literal_len += 1;
				literal_len += rle_len; // No enough Rle values, make them part of literals
				rle_len = 0;
			}
			else
			{
				rle_len += 1;

				// Enough Rle values, do something
				if (rle_len == RLE_TRIGGER)
				{
					// Emit literals so far
					if (EmitLiteral(literal_len, &this->block[i - RLE_TRIGGER - literal_len + 1]) != 0)
						return 1;

					literal_len = 0;

					// Find Rle end
					for (unsigned u = i + 1; u < block_len && this->block[i] == this->block[u]; u += 1)
						rle_len += 1;

					// Emit Rle
					if (EmitRle(rle_len, this->block[i]) != 0)
						return 1;

					i += rle_len - RLE_TRIGGER;
					rle_len = 0;
				}
			}
		}

		// Remainder
		if (literal_len != 0 || rle_len != 0)
		{
			literal_len += rle_len;
			if (EmitLiteral(literal_len, &this->block[block_len - literal_len]) != 0)
				return 1;
		}

		// Bye!
		return 0;
	}

  public:
	CompressorKagari(void* output, size_t max_output_size)
	{
		this->output_start = reinterpret_cast<uint32_t*>(output);
		this->output_end = reinterpret_cast<uint32_t*>(output) + max_output_size / sizeof(uint32_t);
		this->output = reinterpret_cast<uint32_t*>(output);

		this->block_start = reinterpret_cast<uint32_t*>(malloc(sizeof(uint32_t) * BLOCK_LEN));
		this->block_end = this->block_start + BLOCK_LEN;
		this->block = this->block_start;
	}

	~CompressorKagari()
	{
		free(this->block_start);
	}

	int Step(TIn (*quantize)(float, TIn), float quantization, unsigned width, unsigned height, const TIn* in)
	{
		// Fill block
		auto len = (width * height);
		do
		{
			// Quantizing and converting values to unsigned
			for (; len != 0 && this->block < this->block_end; len -= 1)
			{
				const auto v = static_cast<int32_t>(quantize(quantization, *in++));
				*this->block++ = ZigZagEncode(v);
			}

			// No space, compress what we have so far
			if (this->block == this->block_end)
			{
				if (Compress() != 0)
					return 1;
				printf("\tE -- Block --\n"); // Developers, developers, developers
			}

		} while (len != 0);

		// Bye!
		return 0;
	}

	size_t Finish()
	{
		if (this->block - this->block_start > 0 && Compress() != 0)
			return 0;

		printf("\tE [Final (%li/%li)]\n", output - output_start,
		       output_end - output_start); // Developers, developers, developers
		return static_cast<size_t>(this->output - this->output_start) * sizeof(uint32_t);
	}
};

} // namespace ako
