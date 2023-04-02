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

class CompressorKagari final : public Compressor<int16_t>
{
  private:
	static const unsigned RLE_TRIGGER = 2;

	unsigned m_block_width;
	unsigned m_block_height;

	int16_t* m_block_start = nullptr;
	int16_t* m_block_end;
	int16_t* m_block;

	uint16_t* m_code_segment_start = nullptr;
	uint16_t* m_code_segment;
	uint16_t* m_data_segment_start = nullptr;
	uint16_t* m_data_segment;

	AnsEncoder m_code_ans_encoder;
	AnsEncoder m_data_ans_encoder;
	BitWriter m_writer;

	Histogram m_histogram[65536 + 1];
	unsigned m_histogram_last = 0;


	uint16_t ZigZagEncode(int16_t in) const
	{
		return static_cast<uint16_t>((in * 2) ^ (in >> 15));
	}

	int Emit(uint32_t rle_length, uint32_t literal_length, int16_t rle_value, const int16_t* literal_values)
	{
		// Developers, developers, developers
		{
			(void)rle_value;
			// printf("\tE [Rle, l: %u, v: '%c']", rle_length, static_cast<char>(rle_value));
			// printf(" [Lit, l: %u, v: '", literal_length);
			// for (uint32_t i = 0; i < literal_length; i += 1)
			//	printf("%c", static_cast<char>(literal_values[i]));
			// printf("']\n");
		}

		// Code/Instructions
		*m_code_segment++ = static_cast<uint16_t>(rle_length);
		*m_code_segment++ = static_cast<uint16_t>(literal_length - 1); // Notice the '- 1'

		m_histogram[rle_length].c += 1;
		m_histogram[literal_length - 1].c += 1;
		m_histogram_last = Max(m_histogram_last, rle_length);
		m_histogram_last = Max(m_histogram_last, literal_length - 1);

		// Literal data
		for (uint32_t i = 0; i < literal_length; i += 1)
		{
			const auto value = ZigZagEncode(literal_values[i]);
			*m_data_segment++ = value;

			m_histogram[value].d += 1;
			m_histogram_last = Max(m_histogram_last, static_cast<unsigned>(value));
		}

		// Bye!
		return 0;
	}

	int Compress()
	{
		const auto block_length = static_cast<uint32_t>(m_block - m_block_start);
		m_block = m_block_start;

		// Rle compress
		{
			uint32_t rle_length = 0;
			int16_t rle_value = 0;

			// Main loop
			for (uint32_t i = 0; i < block_length; i += 1)
			{
				if (m_block[i] == rle_value && rle_length != 0xFFFF)
					rle_length += 1;
				else
				{
					// Find literal length
					uint32_t literal_length = 0;
					{
						uint32_t repetitions = 0; // Inside our literal

						for (uint32_t u = i + 1;
						     u < block_length && repetitions < RLE_TRIGGER && literal_length < 0xFFFF; u += 1)
						{
							literal_length += 1;
							if (m_block[u] == m_block[u - 1])
								repetitions += 1;
							else
								repetitions = 0;
						}

						if (repetitions == RLE_TRIGGER)
							literal_length -= RLE_TRIGGER;
					}

					// Emit
					if (Emit(rle_length, literal_length + 1, rle_value, m_block + i) != 0)
						return 1;

					// Next step
					rle_value = m_block[i + literal_length];
					rle_length = 0;
					i += literal_length;
				}
			}

			// Remainder
			if (rle_length != 0)
			{
				// Always end on a literal
				if (Emit(rle_length - 1, 1, rle_value, &rle_value) != 0)
					return 1;
			}
		}

		const auto code_length = static_cast<uint32_t>(m_code_segment - m_code_segment_start);
		const auto data_length = static_cast<uint32_t>(m_data_segment - m_data_segment_start);

		// Check if Ans provide us improvements on top of Rle
		uint32_t ans_compress = 0;
		{
			const auto ans_code_size = m_code_ans_encoder.Encode(code_length, m_code_segment_start);
			const auto ans_data_size = m_data_ans_encoder.Encode(data_length, m_data_segment_start);

			// printf("ANS: %u, %u\n", ans_code_size, ans_data_size);

			if (ans_code_size != 0 && ans_data_size != 0)
			{
				if ((ans_code_size + ans_data_size) <
				    (code_length + data_length) * sizeof(uint16_t) * 8) // x8 as AnsEncode() returns size in bits
					ans_compress = 1;
			}
		}

		// Output block metadata
		{
			if (m_writer.Write(ans_compress, 1) == 0)
				return 1;
			if (m_writer.WriteRice(code_length) == 0)
				return 1;
			if (m_writer.WriteRice(data_length) == 0)
				return 1;
		}

		// Output block segments
		if (ans_compress == 0)
		{
			for (auto v = m_code_segment_start; v < m_code_segment; v += 1)
			{
				if (m_writer.Write(*v, 16) == 0)
					return 1;
			}

			for (auto v = m_data_segment_start; v < m_data_segment; v += 1)
			{
				if (m_writer.Write(*v, 16) == 0)
					return 1;
			}
		}
		else
		{
			if (m_code_ans_encoder.Write(&m_writer) == 0)
				return 1;
			if (m_data_ans_encoder.Write(&m_writer) == 0)
				return 1;
		}

		m_code_segment = m_code_segment_start;
		m_data_segment = m_data_segment_start;

		// Bye!
		return 0;
	}

  public:
	CompressorKagari(unsigned block_width, unsigned block_height, size_t output_size, void* output)
	{
		const auto block_length = block_width * block_height;
		m_block_width = block_width;
		m_block_height = block_height;

		m_block_start = reinterpret_cast<int16_t*>(std::malloc(block_length * sizeof(int16_t)));
		m_block_end = m_block_start + block_length;

		m_code_segment_start = reinterpret_cast<uint16_t*>(std::malloc((block_length + 2) * sizeof(uint16_t)));
		m_data_segment_start = reinterpret_cast<uint16_t*>(std::malloc((block_length + 2) * sizeof(uint16_t)));

		Reset(output_size, output);
	}

	~CompressorKagari()
	{
		std::free(m_block_start);
		std::free(m_code_segment_start);
		std::free(m_data_segment_start);
	}

	void Reset(size_t output_size, void* output)
	{
		m_writer.Reset(static_cast<uint32_t>(output_size / sizeof(uint32_t)), reinterpret_cast<uint32_t*>(output));
		m_block = m_block_start;
		m_code_segment = m_code_segment_start;
		m_data_segment = m_data_segment_start;

		Memset(m_histogram, 0, sizeof(Histogram) * (65536 + 1));
		m_histogram_last = 0;
	}

	int Step(QuantizationCallback<int16_t> quantize, float quantization, unsigned width, unsigned height,
	         const int16_t* input) override
	{
		auto input_length = (width * height);
		unsigned x = 0;
		unsigned y = 0;

		do
		{
			const auto block_w = Min(m_block_width, width - x);
			const auto block_h = Min(m_block_height, height - y);
			const auto block_length = block_w * block_h;

			// No space in block, compress what we have so far
			if (m_block == m_block_end || m_block + block_length > m_block_end)
			{
				if (Compress() != 0)
					return 1;
			}

			// Quantize input
			{
				if (width >= CDF53_MINIMUM_LENGTH && height >= CDF53_MINIMUM_LENGTH)
					quantize(quantization, block_w, block_h, width, block_w, input + x, m_block);
				else
					quantize(1.0F, block_w, block_h, width, block_w, input + x, m_block);

				m_block += block_length;
				input_length -= block_length;
				x += m_block_width;

				if (x >= width)
				{
					x = 0;
					y += m_block_height;
					input += width * m_block_height;
				}
			}

		} while (input_length != 0);

		// Bye!
		return 0;
	}

	size_t Finish() override
	{
		// Remainder
		if (m_block - m_block_start > 0)
		{
			if (Compress() != 0)
				return 0;
		}

		// Bye!
		return m_writer.Finish() * sizeof(uint32_t);
	}

	unsigned GetHistogramLength() const
	{
		return m_histogram_last + 1;
	}

	const Histogram* GetHistogram() const
	{
		return m_histogram;
	}
};

} // namespace ako
