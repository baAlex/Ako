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
	static const unsigned RLE_TRIGGER = 4;
	static const unsigned HISTOGRAM_LENGTH = 0xFFFF;

	BitWriter m_writer;
	Histogram m_histogram[HISTOGRAM_LENGTH + 1];

	int16_t* m_block_start = nullptr;
	int16_t* m_block_end;
	int16_t* m_block;

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
			// 	printf("%c", static_cast<char>(literal_values[i]));
			// printf("']\n");
		}

		m_histogram[literal_length - 1].i += 1; // Notice the '- 1'
		m_histogram[rle_length].i += 1;

		if (rle_length > 0xFFFF || literal_length > 0xFFFF) // Block size don't allow this to happen
			return 1;

		// Instructions
		if (m_writer.Write(rle_length, 16) != 0)
			return 1;
		if (m_writer.Write(literal_length - 1, 16) != 0) // Notice the '- 1'
			return 1;

		// Literal data
		for (uint32_t i = 0; i < literal_length; i += 1)
		{
			const auto value = ZigZagEncode(literal_values[i]);
			if (m_writer.Write(value, 16) != 0)
				return 1;

			m_histogram[value].d += 1;
		}

		// Bye!
		return 0;
	}

	int Compress()
	{
		const auto block_length = static_cast<uint32_t>(m_block - m_block_start);

		m_block = m_block_start;

		uint32_t rle_length = 0;
		int16_t rle_value = 0;

		// Main loop
		for (uint32_t i = 0; i < block_length; i += 1)
		{
			if (m_block[i] == rle_value)
				rle_length += 1;
			else
			{
				// Find literal length
				uint32_t literal_length = 0;
				{
					uint32_t repetitions = 0; // Inside our literal

					for (uint32_t u = i + 1; u < block_length && repetitions < RLE_TRIGGER; u += 1)
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

		// Bye!
		return 0;
	}

  public:
	CompressorKagari(unsigned block_length, size_t output_size, void* output)
	{
		Reset(block_length, output_size, output);
	}

	~CompressorKagari()
	{
		free(m_block_start);
	}

	void Reset(unsigned block_length, size_t output_size, void* output)
	{
		// assert(output_size / sizeof(uint32_t) <= 0xFFFFFFFF); // TODO

		m_writer.Reset(static_cast<uint32_t>(output_size / sizeof(uint32_t)), reinterpret_cast<uint32_t*>(output));

		if (m_block_start == nullptr || static_cast<unsigned>(m_block_end - m_block_start) < block_length)
		{
			if (m_block_start != nullptr) // TODO
				free(m_block_start);
			m_block_start = reinterpret_cast<int16_t*>(malloc(block_length * sizeof(int16_t)));
		}

		m_block_end = m_block_start + block_length;
		m_block = m_block_start;

		Memset(m_histogram, 0, sizeof(Histogram) * (HISTOGRAM_LENGTH + 1));
	}

	int Step(QuantizationCallback<int16_t> quantize, float quantization, unsigned width, unsigned height,
	         const int16_t* input) override
	{
		auto input_length = (width * height);
		do
		{
			// No space in block, compress what we have so far
			if (m_block == m_block_end)
			{
				if (Compress() != 0)
					return 1;
			}

			// Quantize input
			{
				const auto length = Min(static_cast<unsigned>(m_block_end - m_block), input_length);

				quantize(quantization, length, input, m_block);
				m_block += length;
				input_length -= length;
				input += length;
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
		return HISTOGRAM_LENGTH + 1;
	}

	const Histogram* GetHistogram() const
	{
		return m_histogram;
	}
};

} // namespace ako
