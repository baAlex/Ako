/*

MIT License

Copyright (c) 2021 Alexander Brandt

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


#ifndef MISC_HPP
#define MISC_HPP

#include <fstream>
#include <string>


class ErrorStr
{
  public:
	std::string info;

	ErrorStr(const std::string& info = "Error with no information provided")
	{
		this->info = info;
	}
};


inline void WriteBlob(const std::string& filename, const void* blob, size_t size)
{
	auto fp = std::fstream(filename, std::ios::binary | std::ios_base::out);

	fp.write((char*)blob, size);

	if (fp.fail() == true)
		throw ErrorStr("Write error");

	fp.close();
}


inline uint32_t Adler32(const uint8_t* data, size_t len)
{
	// Borrowed from LodePng
	uint32_t s1 = 1u & 0xffffu;
	uint32_t s2 = (1u >> 16u) & 0xffffu;

	while (len != 0u)
	{
		uint32_t i;

		// (Lode) At least 5552 sums can be done before the sums overflow, saving a lot of module divisions
		uint32_t amount = len > 5552u ? 5552u : len;
		len -= amount;

		for (i = 0; i != amount; ++i)
		{
			s1 += (*data++);
			s2 += s1;
		}

		s1 %= 65521u;
		s2 %= 65521u;
	}

	return (s2 << 16u) | s1;
}

#endif
