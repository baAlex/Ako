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

void ImageHeadWrite(const Settings& settings, unsigned image_w, unsigned image_h, unsigned channels, unsigned depth,
                    ImageHead& out)
{
	int td = 0;
	while ((1 << td) < static_cast<int>(settings.tiles_dimension))
		td += 1;

	out.magic = IMAGE_HEAD_MAGIC;

	out.a = static_cast<uint32_t>((image_w - 1) & 0xFFFFFF) << 7;   // Width, 24 in 25 bits
	out.a |= static_cast<uint32_t>((depth - 1) & 0xF) << 2;         // Depth, 4 in 5 bits
	out.a |= static_cast<uint32_t>(ToNumber(settings.color) & 0x3); // Color, 2 in 2 bits

	out.b = static_cast<uint32_t>((image_h - 1) & 0xFFFFFF) << 7;     // Height, 24 in 25 bits
	out.b |= static_cast<uint32_t>(td & 0x1F) << 2;                   // Tiles, 5 in 5 bits
	out.b |= static_cast<uint32_t>(ToNumber(settings.wavelet) & 0x3); // Wavelet, 2 in 2 bits

	out.c = static_cast<uint32_t>((channels - 1) & 0xFFFFFF) << 7;      // Channels, 24 in 25 bits
	out.c |= static_cast<uint32_t>(ToNumber(settings.wrap) & 0x3) << 5; // Wrap, 2 in 2 bits
	out.c |= static_cast<uint32_t>(0);                                  // Unused 5 bits

	if (SystemEndianness() != Endianness::Little)
	{
		out.magic = EndiannessReverse(out.magic);
		out.a = EndiannessReverse(out.a);
		out.b = EndiannessReverse(out.b);
		out.c = EndiannessReverse(out.c);
	}
}


void TileHeadWrite(unsigned no, Compression compression, size_t compressed_size, TileHead& out)
{
	out.magic = TILE_HEAD_MAGIC;

	out.no = static_cast<uint32_t>(no);
	out.compressed_size = static_cast<uint32_t>(compressed_size); // TODO, check

	out.tags = 0;
	out.tags |= static_cast<uint32_t>(ToNumber(compression) & 0x3) << 30; // Compression, 2 bits

	if (SystemEndianness() != Endianness::Little)
	{
		out.magic = EndiannessReverse(out.magic);
		out.no = EndiannessReverse(out.no);
		out.compressed_size = EndiannessReverse(out.compressed_size);
	}
}

} // namespace ako
