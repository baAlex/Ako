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


#include "modern-app.hpp"
#include "thirdparty/lodepng.h"
using namespace std;

extern "C"
{
#include "ako.h"
}

const char* version_string = "version_string";
const char* help_string = "help_string";


class PngImage
{
  private:
	size_t _width;
	size_t _height;
	size_t _channels;
	void* _data;

  public:
	// clang-format off
	size_t width() const     { return _width; };
	size_t height() const    { return _height; };
	size_t channels() const  { return _channels; };
	const void* data() const { return (const void*)(_data); };
	// clang-format on

	PngImage(const string& filename)
	{
		unsigned char* blob;
		size_t blob_size;

		LodePNGState state;
		unsigned error;

		unsigned width;
		unsigned height;
		unsigned char* data;

		// Set
		lodepng_state_init(&state);
		state.decoder.color_convert = 0; // Do not convert

		// Load/decode
		if ((error = lodepng_load_file(&blob, &blob_size, filename.c_str())) != 0)
			throw Modern::Error("LodePng error '" + string(lodepng_error_text(error)) + "'");

		if ((error = lodepng_decode(&data, &width, &height, &state, blob, blob_size)) != 0)
			throw Modern::Error("LodePng error '" + string(lodepng_error_text(error)) + "'");

		// Validate
		switch (state.info_png.color.colortype)
		{
		case LCT_GREY: _channels = 1; break;
		case LCT_GREY_ALPHA: _channels = 2; break;
		case LCT_RGB: _channels = 3; break;
		case LCT_RGBA: _channels = 4; break;
		default: throw Modern::Error("Unsupported channels number");
		}

		if (state.info_png.color.bitdepth != 8)
			throw Modern::Error("Unsupported bits per pixel-component");

		_data = (void*)data;
		_width = (size_t)width;
		_height = (size_t)height;

		// Bye!
		lodepng_state_cleanup(&state);
		free(blob);
	}

	~PngImage()
	{
		free(_data);
	}
};


void AkoEnc(const vector<string>& args)
{
	string filename_input = "";
	string filename_output = "";
	bool verbose = false;

	// Check arguments
	if (args.size() == 1)
	{
		cout << help_string << endl;
		return;
	}

	for (auto it = begin(args) + 1, it_end = end(args); it != it_end; it++)
	{
		if (Modern::CheckArgumentSingle("-v", "--version", it) == 0)
		{
			cout << version_string << endl;
			return;
		}
		else if (Modern::CheckArgumentSingle("-h", "--help", it) == 0)
		{
			cout << help_string << endl;
			return;
		}
		else if (Modern::CheckArgumentSingle("-verbose", "--verbose", it) == 0)
			verbose = true;
		else if (Modern::CheckArgumentPair("-i", "--input", it, it_end) == 0)
			filename_input = *it;
		else if (Modern::CheckArgumentPair("-o", "--output", it, it_end) == 0)
			filename_output = *it;
		else
			throw Modern::Error("Unknown argument '" + *it + "'");
	}

	if (filename_input == "")
		throw Modern::Error("No input filename specified");

	// Open input
	const auto png = make_unique<PngImage>(filename_input);

	if (verbose == true)
		cout << "Input: '" << filename_input << "', " << png->channels() << " channel(s), " << png->width() << "x"
		     << png->height() << " pixels" << endl;

	// Encode
	void* blob = NULL;
	size_t blob_size = 0;
	{
		akoSettings settings = akoDefaultSettings();
		akoCallbacks callbacks = akoDefaultCallbacks();
		akoStatus status = AKO_ERROR;

		blob_size = akoEncodeExt(&callbacks, &settings, png->channels(), png->width(), png->height(), png->data(),
		                         &blob, &status);

		if (blob_size == 0)
			throw Modern::Error("Ako error '" + string(akoStatusString(status)) + "'");
	}

	// Write
	if (filename_output != "")
	{
		auto fp = make_unique<fstream>(filename_output, std::ios::binary | ios_base::out);

		fp->write((char*)blob, blob_size);

		if (fp->fail() == true)
			throw Modern::Error("Write error");
	}

	// Bye!
	akoDefaultFree(blob);
}


int main(int argc, const char* argv[])
{
	try
	{
		auto args = make_unique<vector<string>>();
		for (int i = 0; i < argc; i++)
			args->push_back(string(argv[i]));

		AkoEnc(*args);
	}
	catch (Modern::Error& e)
	{
		cout << e.info << std::endl;
		return 1;
	}

	return 0;
}
