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


#include "shared.hpp"
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
		akoSettings settings;
		{
			Modern::Stopwatch total_benchmark;
			akoCallbacks callbacks = akoDefaultCallbacks();
			akoStatus status = AKO_ERROR;

			if (benchmark == true)
			{
				total_benchmark.start(true);

				Modern::EventsData events_data;
				callbacks.events = Modern::EventsCallback;
				callbacks.events_data = &events_data;

				cout << "Benchmark: " << endl;
			}

			data_ = (void*)akoDecodeExt(&callbacks, blob->size(), blob->data(), &settings, &channels_, &width_,
			                            &height_, &status);

			if (benchmark == true)
				total_benchmark.pause_stop(true, " - Total: ");

			if (data_ == NULL)
				throw Modern::Error("Ako decode error: '" + string(akoStatusString(status)) + "'");
		}

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
	bool checksum = false;
	bool benchmark = false;
	bool fast_png = false;

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
		else if (Modern::CheckArgumentSingle("-ch", "--checksum", it) == 0)
			checksum = true;
		else if (Modern::CheckArgumentSingle("-b", "--benchmark", it) == 0)
			benchmark = true;
		else if (Modern::CheckArgumentSingle("-f", "--fast", it) == 0)
			fast_png = true;
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
	if (verbose == true)
	{
		cout << "# AkoDec v" << TOOLS_VERSION_MAJOR << "." << TOOLS_VERSION_MINOR << "." << TOOLS_VERSION_PATCH;
		cout << " (format " << akoFormatVersion() << ", library v" << akoVersionMajor() << "." << akoVersionMinor()
		     << "." << akoVersionPatch() << ")" << endl;

		cout << "Opening input: '" << filename_input << "'..." << endl;
	}

	const auto ako = make_unique<AkoImage>(filename_input, benchmark);

	if (verbose == true)
		cout << "Input data: " << ako->channels() << " channels, " << ako->width() << "x" << ako->height() << " px"
		     << endl;

	// Checksum data
	if (checksum == true)
	{
		auto value = Modern::Adler32((uint8_t*)ako->data(), ako->width() * ako->height() * ako->channels());

		if (verbose == true)
			cout << "Input data checksum: " << std::hex << value << std::dec << std::endl;
		else
			cout << "Checksum of input '" << filename_input << "' data: " << std::hex << value << std::dec << std::endl;
	}

	if (verbose == true)
	{
		cout << "[Colorspace: " << (int)ako->colorspace();
		cout << ", Wrap: " << (int)ako->wrap();
		cout << ", Wavelet: " << (int)ako->wavelet();
		cout << ", Tiles dimension: " << (int)ako->tiles_dimension() << "]" << endl;
	}

	// Encode
	if (filename_output == "")
		return;

	void* blob = NULL;
	size_t blob_size = 0;
	{
		if (verbose == true)
			cout << "Encoding output: '" << filename_output << "'..." << endl;

		LodePNGState state;

		lodepng_state_init(&state);
		state.info_raw.bitdepth = 8;

		switch (ako->channels())
		{
		case 1: state.info_raw.colortype = LCT_GREY; break;
		case 2: state.info_raw.colortype = LCT_GREY_ALPHA; break;
		case 3: state.info_raw.colortype = LCT_RGB; break;
		case 4: state.info_raw.colortype = LCT_RGBA; break;
		default: throw Modern::Error("Unsupported channels number (" + to_string(ako->channels()) + ")");
		}

		if (fast_png != 0)
		{
			state.encoder.zlibsettings.btype = 0;     // No compression
			state.encoder.filter_strategy = LFS_ZERO; // No prediction
		}

		unsigned error = lodepng_encode((unsigned char**)(&blob), &blob_size, (unsigned char*)ako->data(),
		                                (unsigned)ako->width(), (unsigned)ako->height(), &state);

		if (error != 0)
			throw Modern::Error("LodePng error: '" + string(lodepng_error_text(error)) + "'");
	}

	// Write
	if (verbose == true)
		cout << "Writing output: '" << filename_output << "'..." << endl;

	Modern::WriteBlob(filename_output, blob, blob_size);

	// Bye!
	free(blob);
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
