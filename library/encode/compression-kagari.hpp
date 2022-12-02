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

template <typename T> class CompressorKagari final : public Compressor<T>
{
  private:
	static const unsigned RLE_TRIGGER = 4;

	uint8_t* output_start;
	uint8_t* output_end;
	uint8_t* output;

	T* buffer_start = nullptr;
	T* buffer_end;
	T* buffer;

	uint32_t ZigZagEncode(int32_t in) const
	{
		return static_cast<uint32_t>((in << 1) ^ (in >> 31));
	}

	uint16_t ZigZagEncode(int16_t in) const
	{
		return static_cast<uint16_t>((in << 1) ^ (in >> 15));
	}

	uint16_t* FlipSignCast(int16_t* ptr)
	{
		return reinterpret_cast<uint16_t*>(ptr);
	}

	uint32_t* FlipSignCast(int32_t* ptr)
	{
		return reinterpret_cast<uint32_t*>(ptr);
	}

	int Emit(unsigned rle_length, unsigned literal_length, T rle_value, const T* literal_values)
	{
		// Developers, developers, developers
		{
			(void)rle_value;
			// printf("\tE [Rle, l: %u, v: '%c']", rle_length, static_cast<char>(rle_value));
			// printf(" [Lit, l: %u, v: '", literal_length);
			// for (unsigned i = 0; i < literal_length; i += 1)
			//	printf("%c", static_cast<char>(literal_values[i]));
			// printf("']\n");
		}

		// Check space in output
		if (this->output + (sizeof(uint32_t) * 2) + (sizeof(T) * literal_length) > this->output_end)
			return 1;

		// Write Rle and literal lengths
		auto out_u32 = reinterpret_cast<uint32_t*>(this->output);
		*out_u32++ = rle_length;
		*out_u32++ = literal_length;

		// Write literal values
		auto out_uT = FlipSignCast(reinterpret_cast<T*>(out_u32));
		for (unsigned i = 0; i < literal_length; i += 1)
			*out_uT++ = ZigZagEncode(*literal_values++);

		// Update pointer
		this->output = reinterpret_cast<uint8_t*>(out_uT);

		// Bye!
		return 0;
	}

	int Compress()
	{
		const auto buffer_len = static_cast<unsigned>(this->buffer - this->buffer_start);

		this->buffer = this->buffer_start;

		unsigned rle_len = 0;
		T rle_value = 0;

		// Main loop
		for (unsigned i = 0; i < buffer_len; i += 1)
		{
			if (this->buffer[i] == rle_value)
				rle_len += 1;
			else
			{
				// Find literal length
				unsigned literal_len = 0;
				{
					unsigned repetitions = 0; // Inside our literal

					for (unsigned u = i + 1; u < buffer_len && repetitions < RLE_TRIGGER; u += 1)
					{
						literal_len += 1;
						if (this->buffer[u] == this->buffer[u - 1])
							repetitions += 1;
						else
							repetitions = 0;
					}

					if (repetitions == RLE_TRIGGER)
						literal_len -= RLE_TRIGGER;
				}

				// Emit
				if (Emit(rle_len, literal_len + 1, rle_value, this->buffer + i) != 0)
					return 1;

				// Next step
				rle_value = this->buffer[i + literal_len];
				rle_len = 0;
				i += literal_len;
			}
		}

		// Remainder
		if (rle_len != 0)
		{
			if (Emit(rle_len, 0, rle_value, this->buffer) != 0)
				return 1;
		}

		// Bye!
		return 0;
	}

  public:
	CompressorKagari(unsigned buffer_length, size_t output_size, void* output)
	{
		Reset(buffer_length, output_size, output);
	}

	~CompressorKagari()
	{
		free(this->buffer_start);
	}

	void Reset(unsigned buffer_length, size_t output_size, void* output)
	{
		this->output_start = reinterpret_cast<uint8_t*>(output);
		this->output_end = this->output_start + output_size;
		this->output = this->output_start;

		if (this->buffer_start == nullptr || this->buffer_end - this->buffer_start < buffer_length)
		{
			if (this->buffer_start != nullptr) // TODO
				free(this->buffer_start);
			this->buffer_start = reinterpret_cast<T*>(malloc(buffer_length * sizeof(T)));
		}

		this->buffer_end = this->buffer_start + buffer_length;
		this->buffer = this->buffer_start;
	}

	int Step(QuantizationCallback<T> quantize, float quantization, unsigned width, unsigned height,
	         const T* input) override
	{
		auto input_length = (width * height);
		do
		{
			// No space in buffer, compress what we have so far
			if (this->buffer == this->buffer_end)
			{
				if (Compress() != 0)
					return 1;
			}

			// Quantize input
			{
				const auto length = Min(static_cast<unsigned>(this->buffer_end - this->buffer), input_length);

				quantize(quantization, length, input, this->buffer);
				this->buffer += length;
				input_length -= length;
				input += length;
			}

		} while (input_length != 0);

		// Bye!
		return 0;
	}

	size_t Finish() override
	{
		if (this->buffer - this->buffer_start > 0)
		{
			if (Compress() != 0)
				return 0;
		}

		return static_cast<size_t>(this->output - this->output_start);
	}
};

} // namespace ako
