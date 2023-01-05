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
	static const uint32_t ACCUMULATOR_LENGTH = 32;

	const uint32_t* input_end;
	const uint32_t* input;

	uint32_t accumulator;
	uint32_t accumulator_usage;

  public:
	KagariBitReader(size_t input_length = 0, const uint32_t* input = nullptr)
	{
		Reset(input_length, input);
	}

	void Reset(size_t input_length, const uint32_t* input)
	{
		this->input_end = input + input_length;
		this->input = input;

		this->accumulator = 0;
		this->accumulator_usage = 0;
	}

	int Read(uint32_t length, uint32_t& value)
	{
		const auto mask = ~(0xFFFFFFFF << length);

		// Accumulator contains our value, ideal fast path
		if (length <= this->accumulator_usage)
		{
			value = this->accumulator & mask;
			this->accumulator >>= length;
			this->accumulator_usage -= length;
		}

		// Accumulator doesn't have it, at least entirely, ultra super duper slow path
		else
		{
			if (this->input + 1 > this->input_end)
				return 1;

			// Keep what accumulator has
			const auto min = this->accumulator_usage;
			value = this->accumulator;

			// Read, make value with remainder part
			this->accumulator = *this->input++;
			this->accumulator_usage = ACCUMULATOR_LENGTH - (length - min);

			value = (value | (this->accumulator << min)) & mask;
			this->accumulator >>= (length - min);

			// Developers, developers, developers
			// printf("\tD | \tRead accumulator [d: 0x%x, min: %u]\n", *(this->input - 1), min);
		}

		// Developers, developers, developers
		// printf("\tD | v: %u,\tl: %u, u: %u (%u)\n", value, length, this->accumulator_usage,
		//       ACCUMULATOR_LENGTH - this->accumulator_usage);

		// Bye!
		return 0;
	}

	size_t Finish(const uint32_t* input_start) const
	{
		const auto input_length = Max(static_cast<size_t>(1), static_cast<size_t>(this->input - input_start));
		printf("\tDecoded length: %zu\n\n", input_length); // Developers, developers, developers
		return input_length;
	}
};


size_t KagariAnsDecode(uint32_t length, const uint32_t* input, uint16_t* output)
{
	KagariBitReader reader(length + 1, input); // FIXME
	uint32_t state = 0;

	for (uint32_t i = 0; i < length; i += 1)
	{
		// Normalize state (fill it)
		while (state < ANS_L /*&& state != ANS_INITIAL_STATE*/)
		{
			uint32_t word;
			if (reader.Read(ANS_B_LEN, word) != 0)
				return 0;
			// if (word == 0) // Shouldn't be needed if 'INITIAL_STATE > ANS_L'
			//	break;

			state = (state << ANS_B_LEN) | word;

			printf("\tD | %u\n", word); // Developers, developers, developers
		}

		// Find root Cdf entry
		const uint32_t modulo = state & ANS_M_MASK;
		CdfEntry e = g_cdf1[G_CDF1_LENGTH - 1];

		for (uint32_t i = 1; i < G_CDF1_LENGTH; i += 1)
		{
			if (g_cdf1[i].cumulative > modulo)
			{
				e = g_cdf1[i - 1];
				break;
			}
		}

		// Output value
		{
			// Suffix raw from bitstream
			uint32_t suffix = 0;
			reader.Read(e.suffix_length, suffix); // Do not check, let it fail

			// Value is 'root + suffix'
			const uint16_t value = e.root + static_cast<uint16_t>(suffix);
			*output++ = value; // TODO, check?

			// Developers, developers, developers
			printf("\tD | 0x%x\t-> Value: %u (r: %u, sl: %u, f: %u, c: %u)\n", state, value, e.root, e.suffix_length,
			       e.frequency, e.cumulative);
		}

		// Update state
		state = e.frequency * (state >> ANS_M_LEN) + modulo - e.cumulative;
	}

	// Normalize state
	// We want the bit reader to be in sync, and happens that the
	// encoder can normalize before processing the first value
	while (state < ANS_L)
	{
		uint32_t word;
		if (reader.Read(ANS_B_LEN, word) != 0)
			return 0;

		state = (state << ANS_B_LEN) | word;

		printf("\tD | %u\n", word); // Developers, developers, developers
	}

	// Bye!
	return reader.Finish(input);
}


class DecompressorKagari final : public Decompressor<int16_t>
{
  private:
	KagariBitReader reader;

	int16_t* block_start;
	int16_t* block_end;

	unsigned block_usage;
	int16_t* block_cursor;

	int16_t ZigZagDecode(uint16_t value) const
	{
		return static_cast<int16_t>((value >> 1) ^ (~(value & 1) + 1));
	}

	int Decompress()
	{
		this->block_cursor = this->block_start; // We always decompress an entire block (as the encoder do)

		auto out_end = this->block_end;
		auto out = this->block_start;
		int16_t rle_value = 0;

		while (out < out_end)
		{
			// Read Rle and literal lengths
			uint32_t rle_length;
			uint32_t literal_length;

			if (this->reader.Read(16, rle_length) != 0)
				break; // End reached, not an error, if is an error Step() will catch it
			if (this->reader.Read(16, literal_length) != 0)
				break; // Ditto

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
				uint32_t value;
				if (this->reader.Read(16, value) != 0) // TODO
					return 1;

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
		this->block_usage = static_cast<unsigned>(out - this->block_start);
		return 0;
	}

  public:
	DecompressorKagari(unsigned block_length, size_t input_size, const void* input)
	{
		this->reader.Reset(input_size / sizeof(uint32_t), reinterpret_cast<const uint32_t*>(input));

		this->block_start = reinterpret_cast<int16_t*>(malloc(block_length * sizeof(int16_t)));
		this->block_end = this->block_start + block_length;

		this->block_usage = 0;
		this->block_cursor = this->block_start;
	}

	~DecompressorKagari()
	{
		free(this->block_start);
	}

	Status Step(unsigned width, unsigned height, int16_t* out) override
	{
		auto output_length = (width * height);

		while (output_length != 0)
		{
			// If needed, decompress an entire block
			if (this->block_usage == 0)
			{
				if (Decompress() != 0)
					return Status::Error;
				if (this->block_usage == 0)
					return Status::Error;
			}

			// Move values that this block can provide
			{
				const auto length = Min(this->block_usage, output_length);

				for (unsigned i = 0; i < length; i += 1)
					out[i] = this->block_cursor[i]; // NOLINT

				// False positive, as procedure [A] (which makes [B] be valid)
				// is confusing Clang Tidy. With encoder cooperation by
				// disabling '- 1' operations there, our linter then will not
				// report this here section but [B] (which is correct, and can
				// be fix with an extra comparision)... so yeah, closer but not
				// really an error (I think)

				this->block_usage -= length;
				this->block_cursor += length;
				output_length -= length;
				out += length;
			}
		}

		// Bye!
		return Status::Ok;
	}
};

} // namespace ako
