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

class DecompressorKagari final : public Decompressor<int16_t>
{
  private:
	BitReader m_reader;

	int16_t* m_block_start;
	int16_t* m_block_end;

	unsigned m_block_usage;
	int16_t* m_block_cursor;

	uint16_t* m_mini_buffer_start = nullptr;

	int16_t ZigZagDecode(uint16_t value) const
	{
		return static_cast<int16_t>((value >> 1) ^ (~(value & 1) + 1));
	}

	int Decompress()
	{
		m_block_cursor = m_block_start; // We always decompress an entire block (as the encoder do)

		auto out_end = m_block_end;
		auto out = m_block_start;
		int16_t rle_value = 0;

		auto mini_buffer = m_mini_buffer_start;

		// Input
		uint32_t block_head;
		{
			// Input block head
			if (m_reader.Read((BLOCK_LENGTH_BIT_LEN + 1 + 1), block_head) != 0)
				return 1;

			// Input block data
			if ((block_head & 0x01) == 0)
			{
				for (uint32_t i = 0; i < (block_head >> 1); i += 1)
				{
					uint32_t v;
					if (m_reader.Read(16, v) != 0) // TODO
						return 1;
					mini_buffer[i] = static_cast<uint16_t>(v);
				}
			}
			else
			{
				if (AnsDecode(m_reader, block_head >> 1, mini_buffer) == 0)
					return 1;
			}
		}

		// Rle decompress
		while (out < out_end)
		{
			if (mini_buffer - m_mini_buffer_start >= (block_head >> 1)) // TODO
				break;

			// Read Rle and literal lengths
			uint32_t rle_length = *mini_buffer++;
			uint32_t literal_length = *mini_buffer++;

			literal_length += 1; // [A]

			// Checks
			if (out + rle_length + literal_length > out_end)
				return 1;

			// Write Rle values
			for (uint32_t i = 0; i < rle_length; i += 1)
				out[i] = rle_value;

			// Write literal values
			for (uint32_t i = 0; i < literal_length; i += 1)
			{
				const uint32_t value = *mini_buffer++;
				// if (m_reader.Read(16, value) != 0) // TODO
				//	return 1;

				out[rle_length + i] = static_cast<int16_t>(ZigZagDecode(static_cast<uint16_t>(value)));
			}

			rle_value = out[rle_length + literal_length - 1]; // [B]

			// Developers, developers, developers
			{
				// printf("\tD [Rle, l: %u, v: '%c']", rle_length, static_cast<char>(rle_value));
				// printf(" [Lit, l: %u, v: '", literal_length);
				// for (uint32_t i = 0; i < literal_length; i += 1)
				// 	printf("%c", static_cast<char>(out[rle_length + i]));
				// printf("']\n");
			}

			// Update pointers
			out += rle_length + literal_length;
		}

		// Bye!
		m_block_usage = static_cast<unsigned>(out - m_block_start);
		return 0;
	}

  public:
	DecompressorKagari(unsigned block_length, size_t input_size, const void* input)
	{
		m_reader.Reset(static_cast<uint32_t>(input_size / sizeof(uint32_t)), reinterpret_cast<const uint32_t*>(input));

		m_block_start = reinterpret_cast<int16_t*>(std::malloc(block_length * sizeof(int16_t)));
		m_block_end = m_block_start + block_length;

		m_mini_buffer_start = reinterpret_cast<uint16_t*>(std::malloc((block_length + 2) * sizeof(uint16_t)));

		m_block_usage = 0;
		m_block_cursor = m_block_start;
	}

	~DecompressorKagari()
	{
		std::free(m_block_start);
		std::free(m_mini_buffer_start);
	}

	Status Step(unsigned width, unsigned height, int16_t* out) override
	{
		auto output_length = (width * height);

		while (output_length != 0)
		{
			// If needed, decompress an entire block
			if (m_block_usage == 0)
			{
				if (Decompress() != 0)
					return Status::Error;
				if (m_block_usage == 0)
					return Status::Error;
			}

			// Move values that this block can provide
			{
				const auto length = Min(m_block_usage, output_length);

				for (unsigned i = 0; i < length; i += 1)
					out[i] = m_block_cursor[i]; // NOLINT

				// False positive, as procedure [A] (which makes [B] be valid)
				// is confusing Clang Tidy. With encoder cooperation by
				// disabling '- 1' operations there, our linter then will not
				// report this here section but [B] (which is correct, and can
				// be fix with an extra comparision)... so yeah, closer but not
				// really an error (I think)

				m_block_usage -= length;
				m_block_cursor += length;
				output_length -= length;
				out += length;
			}
		}

		// Bye!
		return Status::Ok;
	}
};

} // namespace ako
