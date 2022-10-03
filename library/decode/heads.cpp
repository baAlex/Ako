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

Status DecodeHead(size_t input_size, const void* input, unsigned* out_width, unsigned* out_height,
                  unsigned* out_channels, unsigned* out_depth, Settings* out_settings)
{
	if (input_size < sizeof(ImageHead))
		return Status::TruncatedImageHead;

	auto head = *(reinterpret_cast<const ImageHead*>(input));

	if (SystemEndianness() != Endianness::Little)
	{
		head.magic = EndiannessReverse(head.magic);
		head.a = EndiannessReverse(head.a);
		head.b = EndiannessReverse(head.b);
		head.c = EndiannessReverse(head.c);
	}

	if (head.magic != IMAGE_HEAD_MAGIC)
		return Status::NotAnAkoFile;

	const auto width = static_cast<unsigned>((head.a >> 7) & 0xFFFFFF) + 1;    // Width, 24 in 25 bits
	const auto height = static_cast<unsigned>((head.b >> 7) & 0xFFFFFF) + 1;   // Height, 24 in 25 bits
	const auto depth = static_cast<unsigned>((head.a >> 2) & 0xF) + 1;         // Depth, 4 in 5 bits
	const auto td = static_cast<unsigned>((head.b >> 2) & 0x1F);               // Tiles, 5 in 5 bits
	const auto channels = static_cast<unsigned>((head.c >> 7) & 0xFFFFFF) + 1; // Channels, 24 in 25 bits

	auto settings = DefaultSettings();
	Status status;

	settings.tiles_dimension = 0;
	if (td != 0)
		settings.tiles_dimension = (1 << td);

	settings.color = ToColor(static_cast<uint32_t>(head.a & 0x3), status); // Color, 2 in 2 bits
	if (status != Status::Ok)
		return status;

	settings.wavelet = ToWavelet(static_cast<uint32_t>(head.b & 0x3), status); // Wavelet, 2 in 2 bits
	if (status != Status::Ok)
		return status;

	settings.wrap = ToWrap(static_cast<uint32_t>((head.c >> 5) & 0x3), status); // Wrap, 2 in 2 bits
	if (status != Status::Ok)
		return status;

	settings.compression =
	    ToCompression(static_cast<uint32_t>((head.c >> 3) & 0x3), status); // Compression, 2 in 2 bits
	if (status != Status::Ok)
		return status;

	if ((status = ValidateProperties(width, height, channels, depth)) != Status::Ok ||
	    (status = ValidateSettings(settings)) != Status::Ok)
		return status;

	// All validated, return what we have
	if (out_settings != nullptr)
		*out_settings = settings;
	if (out_width != nullptr)
		*out_width = width;
	if (out_height != nullptr)
		*out_height = height;
	if (out_channels != nullptr)
		*out_channels = channels;
	if (out_depth != nullptr)
		*out_depth = depth;

	return status;
}


Status TileHeadRead(const TileHead& head_raw, size_t& out_compressed_size)
{
	TileHead head = head_raw;

	if (SystemEndianness() != Endianness::Little)
	{
		head.magic = EndiannessReverse(head.magic);
		head.no = EndiannessReverse(head.no);
		head.compressed_size = EndiannessReverse(head.compressed_size);
	}

	if (head.magic != TILE_HEAD_MAGIC || head.compressed_size == 0)
		return Status::InvalidTileHead;

	out_compressed_size = head.compressed_size;
	return Status::Ok;
}

} // namespace ako
