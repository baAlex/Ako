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


class AkoImage
{
  private:
	size_t _width;
	size_t _height;
	size_t _channels;
	void* _data;

	enum akoWrap _wrap;
	enum akoWavelet _wavelet;
	enum akoColorspace _colorspace;
	size_t _tiles_dimension;

  public:
	// clang-format off
	size_t width() const     { return _width; };
	size_t height() const    { return _height; };
	size_t channels() const  { return _channels; };
	const void* data() const { return (const void*)(_data); };

	enum akoWrap wrap() const             { return _wrap; };
	enum akoWavelet wavelet() const       { return _wavelet; };
	enum akoColorspace colorspace() const { return _colorspace; };
	size_t tiles_dimension() const        { return _tiles_dimension; };
	// clang-format on

	AkoImage(const string& filename)
	{
		// Open/read file
		auto blob = make_unique<vector<uint8_t>>();
		{
			auto file = make_unique<fstream>(filename, std::ios::binary | ios_base::in | std::ios::ate);

			if (file->fail() == true)
				throw Modern::Error("Error at opening file '" + filename + "'");

			blob->resize(file->tellg());
			file->seekg(0, std::ios::beg);

			file->read((char*)blob->data(), blob->size());
			if (file->fail() == true)
				throw Modern::Error("Error at reading file '" + filename + "'");
		}

		// Decode
		akoCallbacks callbacks = akoDefaultCallbacks();
		akoStatus status = AKO_ERROR;
		akoSettings settings;

		_data = (void*)akoDecodeExt(&callbacks, blob->size(), blob->data(), &settings, &_channels, &_width, &_height,
		                            &status);

		if (_data == NULL)
			throw Modern::Error("Ako error '" + string(akoStatusString(status)) + "'");

		_wrap = settings.wrap;
		_wavelet = settings.wavelet;
		_colorspace = settings.colorspace;
		_tiles_dimension = settings.tiles_dimension;
	}

	~AkoImage()
	{
		akoDefaultFree(_data);
	}
};


void AkoDec(const vector<string>& args)
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
	const auto ako = make_unique<AkoImage>(filename_input);

	if (verbose == true)
		cout << "Input: '" << filename_input << "', " << ako->channels() << " channel(s), " << ako->width() << "x"
		     << ako->height() << " pixels" << endl;
}


int main(int argc, const char* argv[])
{
	try
	{
		auto args = make_unique<vector<string>>();
		for (int i = 0; i < argc; i++)
			args->push_back(string(argv[i]));

		AkoDec(*args);
	}
	catch (Modern::Error& e)
	{
		cout << e.info << std::endl;
		return 1;
	}

	return 0;
}
