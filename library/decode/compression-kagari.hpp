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

	unsigned m_block_width;
	unsigned m_block_height;

	int16_t* m_block_start;
	int16_t* m_block_end;

	uint32_t m_block_usage;
	int16_t* m_block_cursor;

	uint16_t* m_code_segment_start = nullptr;
	uint16_t* m_data_segment_start = nullptr;


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

		auto code = m_code_segment_start;
		auto data = m_data_segment_start;

		// Input
		uint32_t code_length;
		{
			uint32_t ans_compressed;
			uint32_t data_length;

			// Input block metadata
			if (m_reader.Read(1, ans_compressed) != 0)
				return 1;
			if (m_reader.ReadRice(code_length) != 0)
				return 1;
			if (m_reader.ReadRice(data_length) != 0)
				return 1;

			// Input block segments
			if (ans_compressed == 0)
			{
				uint32_t v;
				for (uint32_t i = 0; i < code_length; i += 1)
				{
					if (m_reader.Read(16, v) != 0) // TODO
						return 1;

					code[i] = static_cast<uint16_t>(v);
				}

				for (uint32_t i = 0; i < data_length; i += 1)
				{
					if (m_reader.Read(16, v) != 0)
						return 1;

					data[i] = static_cast<uint16_t>(v);
				}
			}
			else
			{
				if (AnsDecode(m_reader, g_cdf_c, code_length, code) == 0)
					return 1;
				if (AnsDecode(m_reader, g_cdf_d, data_length, data) == 0)
					return 1;
			}
		}

		// Rle decompress
		while (out < out_end)
		{
			if (code - m_code_segment_start >= code_length)
				break;

			// Read Rle and literal lengths
			uint32_t rle_length = *code++;
			uint32_t literal_length = *code++;

			literal_length += 1;

			// Checks
			if (out + rle_length + literal_length > out_end)
				return 1;

			// Write Rle values
			for (uint32_t i = 0; i < rle_length; i += 1)
				out[i] = rle_value;

			// Write literal values
			for (uint32_t i = 0; i < literal_length; i += 1)
			{
				const uint32_t v = *data++;
				out[rle_length + i] = static_cast<int16_t>(ZigZagDecode(static_cast<uint16_t>(v)));
			}

			rle_value = out[rle_length + literal_length - 1];

			// Developers, developers, developers
			{
				// printf("\tD [Rle, l: %u, v: '%c']", rle_length, static_cast<char>(rle_value));
				// printf(" [Lit, l: %u, v: '", literal_length);
				// for (uint32_t i = 0; i < literal_length; i += 1)
				//	printf("%c", static_cast<char>(out[rle_length + i]));
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
	DecompressorKagari(unsigned block_width, unsigned block_height, size_t input_size, const void* input)
	{
		const auto block_length = block_width * block_height;
		m_block_width = block_width;
		m_block_height = block_height;

		m_reader.Reset(static_cast<uint32_t>(input_size / sizeof(uint32_t)), reinterpret_cast<const uint32_t*>(input));

		m_block_start = reinterpret_cast<int16_t*>(std::malloc(block_length * sizeof(int16_t)));
		m_block_end = m_block_start + block_length;

		m_code_segment_start = reinterpret_cast<uint16_t*>(std::malloc((block_length + 2) * sizeof(uint16_t)));
		m_data_segment_start = reinterpret_cast<uint16_t*>(std::malloc((block_length + 2) * sizeof(uint16_t)));

		m_block_usage = 0;
		m_block_cursor = m_block_start;
	}

	~DecompressorKagari()
	{
		std::free(m_block_start);
		std::free(m_code_segment_start);
		std::free(m_data_segment_start);
	}

	Status Step(unsigned width, unsigned height, int16_t* output) override
	{
		auto output_length = (width * height);
		unsigned x = 0;
		unsigned y = 0;

		while (output_length != 0)
		{
			const auto block_w = Min(m_block_width, width - x);
			const auto block_h = Min(m_block_height, height - y);
			const auto block_length = block_w * block_h;

			// If needed, decompress an entire or more blocks
			if (m_block_usage == 0)
			{
				if (Decompress() != 0)
					return Status::Error;
				if (m_block_usage == 0)
					return Status::Error;
			}

			// Move values that this block provides
			{
				Memcpy2d(block_w, block_h, block_w, width, m_block_cursor, output + x);

				m_block_usage -= block_length;
				m_block_cursor += block_length;
				output_length -= block_length;
				x += m_block_width;

				if (x >= width)
				{
					x = 0;
					y += m_block_height;
					output += width * m_block_height;
				}
			}
		}

		// Bye!
		return Status::Ok;
	}
};

} // namespace ako
