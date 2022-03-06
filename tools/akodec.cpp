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


// clang-format off
const char* help_string = ""
"USAGE\n"
"    akodec [optional options] -i <input filename> -o <output filename>\n"
"    akodec [optional options] -i <input filename>\n"
"\n    Only PNG files supported as output.\n"
"\nINFORMATION OPTIONS\n"
"-v --version\n    Print program version and license terms.\n"
"\n-h, --help\n    Print this help.\n"
"\n-verbose, --verbose\n    Print all available information while encoding.\n"
"\nDECODING OPTIONS\n"
"-f, --fast\n"
"    Uncompressed PNG files.\n"
"\nEXTRA TOOLS\n"
"-ch, --checksum\n"
"-b, --benchmark"
"";
// clang-format on


class AkoImage
{
  private:
	size_t width_;
	size_t height_;
	size_t channels_;
	void* data_;

	enum akoWrap wrap_;
	enum akoWavelet wavelet_;
	enum akoColor color_;
	enum akoCompression compression_;
	size_t tiles_dimension_;
	size_t blob_size_;

  public:
	// clang-format off
	size_t width() const     { return width_; };
	size_t height() const    { return height_; };
	size_t channels() const  { return channels_; };
	const void* data() const { return (const void*)(data_); };

	enum akoWrap wrap() const               { return wrap_; };
	enum akoWavelet wavelet() const         { return wavelet_; };
	enum akoColor color() const             { return color_; };
	enum akoCompression compression() const { return compression_; };
	size_t tiles_dimension() const          { return tiles_dimension_; };
	size_t blob_size() const                { return blob_size_; };
	// clang-format on

	AkoImage(const string& filename, bool benchmark)
	{
		// Open/read file
		auto blob = make_unique<vector<uint8_t>>();
		{
			auto file = make_unique<fstream>(filename, std::ios::binary | ios_base::in | std::ios::ate);

			if (file->fail() == true)
				throw Shared::Error("Error at opening file '" + filename + "'");

			blob_size_ = file->tellg();
			blob->resize(blob_size_);
			file->seekg(0, std::ios::beg);

			file->read((char*)blob->data(), blob->size());
			if (file->fail() == true)
				throw Shared::Error("Error at reading file '" + filename + "'");
		}

		// Decode
		akoSettings settings;
		{
			Shared::Stopwatch total_benchmark;
			akoCallbacks callbacks = akoDefaultCallbacks();
			akoStatus status = AKO_ERROR;

			if (benchmark == true)
			{
				total_benchmark.start(true);

				Shared::EventsData events_data;
				callbacks.events = Shared::EventsCallback;
				callbacks.events_data = &events_data;

				cout << "Benchmark: " << endl;
			}

			data_ = (void*)akoDecodeExt(&callbacks, blob->size(), blob->data(), &settings, &channels_, &width_,
			                            &height_, &status);

			if (benchmark == true)
				total_benchmark.pause_stop(true, " - Total: ");

			if (data_ == NULL)
				throw Shared::Error("Ako decode error: '" + string(akoStatusString(status)) + "'");
		}

		wrap_ = settings.wrap;
		wavelet_ = settings.wavelet;
		color_ = settings.color;
		compression_ = settings.compression;
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
		if (Shared::CheckArgumentSingle("-v", "--version", it) == 0)
		{
			printf("Ako decoding tool v%i.%i.%i\n", TOOLS_VERSION_MAJOR, TOOLS_VERSION_MINOR, TOOLS_VERSION_PATCH);
			printf(" - libako v%i.%i.%i, format %i\n", akoVersionMajor(), akoVersionMinor(), akoVersionPatch(),
			       akoFormatVersion());
			printf(" - lodepng %s\n", LODEPNG_VERSION_STRING);
			printf("\n");
			printf("Copyright (c) 2021-2022 Alexander Brandt. Under MIT License.\n");
			printf("More information at 'https://github.com/baAlex/Ako'\n");
			return;
		}
		else if (Shared::CheckArgumentSingle("-h", "--help", it) == 0)
		{
			cout << help_string << endl;
			return;
		}
		else if (Shared::CheckArgumentSingle("-verbose", "--verbose", it) == 0)
			verbose = true;
		else if (Shared::CheckArgumentSingle("-ch", "--checksum", it) == 0)
			checksum = true;
		else if (Shared::CheckArgumentSingle("-b", "--benchmark", it) == 0)
			benchmark = true;
		else if (Shared::CheckArgumentSingle("-f", "--fast", it) == 0)
			fast_png = true;
		else if (Shared::CheckArgumentPair("-i", "--input", it, it_end) == 0)
			filename_input = *it;
		else if (Shared::CheckArgumentPair("-o", "--output", it, it_end) == 0)
			filename_output = *it;
		else
			throw Shared::Error("Unknown argument '" + *it + "'");
	}

	if (filename_input == "")
		throw Shared::Error("No input filename specified");

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
	uint32_t input_checksum = 0;
	if (checksum == true)
		input_checksum = Shared::Adler32((uint8_t*)ako->data(), ako->width() * ako->height() * ako->channels());

	if (verbose == true)
	{
		cout << "[Compression: " << (int)ako->compression();
		cout << ", Color: " << (int)ako->color();
		cout << ", Wavelet: " << (int)ako->wavelet();
		cout << ", Wrap: " << (int)ako->wrap();
		cout << ", Tiles dimension: " << (int)ako->tiles_dimension() << "]" << endl;
	}

	// Encode
	if (filename_output != "")
	{
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
			default: throw Shared::Error("Unsupported channels number (" + to_string(ako->channels()) + ")");
			}

			if (fast_png != 0)
			{
				state.encoder.zlibsettings.btype = 0;     // No compression
				state.encoder.filter_strategy = LFS_ZERO; // No prediction
			}

			unsigned error = lodepng_encode((unsigned char**)(&blob), &blob_size, (unsigned char*)ako->data(),
			                                (unsigned)ako->width(), (unsigned)ako->height(), &state);

			if (error != 0)
				throw Shared::Error("LodePng error: '" + string(lodepng_error_text(error)) + "'");
		}

		// Write
		if (verbose == true)
			cout << "Writing output: '" << filename_output << "'..." << endl;

		Shared::WriteBlob(filename_output, blob, blob_size);
		free(blob);
	}

	// Bye!
	const auto uncompressed_size = (double)(ako->width() * ako->height() * ako->channels());
	const auto compressed_size = (double)(ako->blob_size());
	const auto bpp =
	    (compressed_size / (double)(ako->width() * ako->height() * ako->channels())) * 8.0F * ako->channels();

	if (checksum == true)
		printf("(%08x) %.2f kB <- %.2f kB, ratio: %.2f:1, %.4f bpp\n", input_checksum, compressed_size / 1000.0F,
		       compressed_size / 1000.0F, uncompressed_size / compressed_size, bpp);
	else
		printf("%.2f kB <- %.2f kB, ratio: %.2f:1, %.4f bpp\n", uncompressed_size / 1000.0F, compressed_size / 1000.0F,
		       uncompressed_size / compressed_size, bpp);
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
	catch (Shared::Error& e)
	{
		cout << e.info << std::endl;
		return 1;
	}

	return 0;
}
