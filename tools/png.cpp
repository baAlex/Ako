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

#include "shared.hpp"
#include "thirdparty/lodepng.h"


// clang-format off
const static LodePNGCompressSettings ZLIB_PRESET[10] ={
	{0, 1, 256,   6, 32,  0, nullptr, nullptr, nullptr},
	{1, 1, 512,   6, 32,  0, nullptr, nullptr, nullptr},
	{1, 1, 512,   6, 64,  0, nullptr, nullptr, nullptr},
	{1, 1, 1024,  6, 64,  0, nullptr, nullptr, nullptr},
	{2, 1, 1024,  6, 92,  1, nullptr, nullptr, nullptr},
	{2, 1, 2048,  3, 92,  1, nullptr, nullptr, nullptr},
	{2, 1, 2048,  3, 128, 1, nullptr, nullptr, nullptr}, // lodepng_default_compress_settings
	{2, 1, 4096,  3, 128, 1, nullptr, nullptr, nullptr},
	{2, 1, 8192,  3, 256, 1, nullptr, nullptr, nullptr},
	{2, 1, 32768, 3, 256, 1, nullptr, nullptr, nullptr},
};

const static LodePNGFilterStrategy PNG_FILTER_PRESET[10] = {
	LFS_ZERO,
	LFS_MINSUM,
	LFS_MINSUM,
	LFS_MINSUM,
	LFS_MINSUM,
	LFS_MINSUM,
	LFS_MINSUM,
	LFS_MINSUM,
	LFS_MINSUM,
	LFS_BRUTE_FORCE,
};
// clang-format on


int DecodePng(size_t in_size, const void* in, unsigned& out_width, unsigned& out_height, unsigned& out_channels,
              unsigned& out_depth, void** out_image)
{
	LodePNGState state = {};
	lodepng_state_init(&state);

	// Encode
	{
		state.decoder.color_convert = 0; // No color conversion

		const auto status = lodepng_decode(reinterpret_cast<unsigned char**>(out_image), &out_width, &out_height,
		                                   &state, reinterpret_cast<const unsigned char*>(in), in_size);

		if (status != 0)
		{
			std::cout << "LodePNG error code " << status << ": '" << lodepng_error_text(status) << "'\n";
			goto return_failure;
		}
	}

	// Checks and outputs
	if (state.info_png.color.bitdepth == 8)
		out_depth = 8;
	else if (state.info_png.color.bitdepth == 16)
		out_depth = 16;
	else
	{
		std::cout << "Only 8 or 16 bits images supported\n";
		goto return_failure;
	}

	switch (state.info_png.color.colortype)
	{
	case LCT_GREY: out_channels = 1; break;
	case LCT_GREY_ALPHA: out_channels = 2; break;
	case LCT_RGB: out_channels = 3; break;
	case LCT_RGBA: out_channels = 4; break;
	default: std::cout << "Unsupported channels\n"; goto return_failure;
	}

	// Bye!
	lodepng_state_cleanup(&state);
	return 0;

return_failure:
	lodepng_state_cleanup(&state);
	return 1;
}


size_t EncodePng(int effort, unsigned width, unsigned height, unsigned channels, unsigned depth, const void* in,
                 void** out_blob)
{
	size_t blob_size = 0;

	// Settings
	LodePNGState state = {};
	{
		lodepng_state_init(&state);

		if (depth == 8)
			state.info_raw.bitdepth = 8;
		else
			state.info_raw.bitdepth = 16;

		switch (channels)
		{
		case 1: state.info_raw.colortype = LCT_GREY; break;
		case 2: state.info_raw.colortype = LCT_GREY_ALPHA; break;
		case 3: state.info_raw.colortype = LCT_RGB; break;
		case 4: state.info_raw.colortype = LCT_RGBA; break;
		default: std::cout << "Unsupported channels\n"; goto return_failure;
		}

		effort = (effort < 1) ? 1 : effort;
		state.encoder.zlibsettings = ZLIB_PRESET[effort - 1];
		state.encoder.filter_strategy = PNG_FILTER_PRESET[effort - 1];
	}

	// Encode
	{
		const auto status = lodepng_encode(reinterpret_cast<unsigned char**>(out_blob), &blob_size,
		                                   reinterpret_cast<const unsigned char*>(in), width, height, &state);

		if (status != 0)
		{
			std::cout << "LodePNG error code " << status << ": '" << lodepng_error_text(status) << "'\n";
			goto return_failure;
		}
	}

	// Bye!
	lodepng_state_cleanup(&state);
	return blob_size;

return_failure:
	lodepng_state_cleanup(&state);
	return 0;
}
