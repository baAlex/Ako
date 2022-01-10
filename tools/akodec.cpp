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
using namespace std;


const char* version_string = "version_string";
const char* help_string = "help_string";


class AkoImage
{
  private:
	size_t width_;
	size_t height_;
	size_t channels_;
	void* data_;

	enum akoWrap wrap_;
	enum akoWavelet wavelet_;
	enum akoColorspace colorspace_;
	size_t tiles_dimension_;

  public:
	// clang-format off
	size_t width() const     { return width_; };
	size_t height() const    { return height_; };
	size_t channels() const  { return channels_; };
	const void* data() const { return (const void*)(data_); };

	enum akoWrap wrap() const             { return wrap_; };
	enum akoWavelet wavelet() const       { return wavelet_; };
	enum akoColorspace colorspace() const { return colorspace_; };
	size_t tiles_dimension() const        { return tiles_dimension_; };
	// clang-format on

	AkoImage(const string& filename, bool benchmark)
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

		if (benchmark == true)
		{
			Modern::EventsData events_data;
			callbacks.events = Modern::EventsCallback;
			callbacks.events_data = &events_data;
		}

		data_ = (void*)akoDecodeExt(&callbacks, blob->size(), blob->data(), &settings, &channels_, &width_, &height_,
		                            &status);

		if (data_ == NULL)
			throw Modern::Error("Ako decode error: '" + string(akoStatusString(status)) + "'");

		wrap_ = settings.wrap;
		wavelet_ = settings.wavelet;
		colorspace_ = settings.colorspace;
		tiles_dimension_ = settings.tiles_dimension;
	}

	~AkoImage()
	{
		akoDefaultFree(data_);
	}
};


void AkoDec(const vector<string>& args)
{
	string filename_input = "";
	string filename_output = "";
	bool verbose = false;
	bool benchmark = false;

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
		else if (Modern::CheckArgumentSingle("-b", "--benchmark", it) == 0)
			benchmark = true;
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
	const auto ako = make_unique<AkoImage>(filename_input, benchmark);

	if (verbose == true)
	{
		if (benchmark == true)
			cout << endl;

		cout << "AkoDec: '" << filename_input << "', " << ako->channels() << " channel(s), " << ako->width() << "x"
		     << ako->height() << " pixels" << endl;

		cout << " - tiles dimension: " << ako->tiles_dimension() << " px, wrap mode: " << (int)ako->wrap()
		     << ", wavelet: " << (int)ako->wavelet() << ", colorspace: " << ako->colorspace() << endl;
	}

	// Encode
	void* blob = NULL;
	size_t blob_size = 0;
	{
		LodePNGColorType color;
		switch (ako->channels())
		{
		case 1: color = LCT_GREY; break;
		case 2: color = LCT_GREY_ALPHA; break;
		case 3: color = LCT_RGB; break;
		case 4: color = LCT_RGBA; break;
		default: throw Modern::Error("Unsupported channels number (" + to_string(ako->channels()) + ")");
		}

		unsigned error = lodepng_encode_memory((unsigned char**)(&blob), &blob_size, (unsigned char*)ako->data(),
		                                       (unsigned)ako->width(), (unsigned)ako->height(), color, 8);

		if (error != 0)
			throw Modern::Error("LodePng error: '" + string(lodepng_error_text(error)) + "'");
	}

	// Bye!
	Modern::WriteBlob(filename_output, blob, blob_size);
	akoDefaultFree(blob);
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
