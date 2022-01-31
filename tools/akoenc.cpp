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


const char* version_string = "version_string"; // TODO: Add thanks to Lode Vandevenne (LodePng)
const char* help_string = "help_string";

#define TOOL_VERSION_MAJOR 0
#define TOOL_VERSION_MINOR 2
#define TOOL_VERSION_PATCH 0


class PngImage
{
  private:
	size_t width_;
	size_t height_;
	size_t channels_;
	void* data_;

  public:
	// clang-format off
	size_t width() const     { return width_; };
	size_t height() const    { return height_; };
	size_t channels() const  { return channels_; };
	const void* data() const { return (const void*)(data_); };
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
			throw Shared::Error("LodePng error: '" + string(lodepng_error_text(error)) + "'");

		if ((error = lodepng_decode(&data, &width, &height, &state, blob, blob_size)) != 0)
			throw Shared::Error("LodePng error: '" + string(lodepng_error_text(error)) + "'");

		// Validate
		switch (state.info_png.color.colortype)
		{
		case LCT_GREY: channels_ = 1; break;
		case LCT_GREY_ALPHA: channels_ = 2; break;
		case LCT_RGB: channels_ = 3; break;
		case LCT_RGBA: channels_ = 4; break;
		default: throw Shared::Error("Unsupported channels number (" + to_string(state.info_png.color.colortype) + ")");
		}

		if (state.info_png.color.bitdepth != 8)
			throw Shared::Error("Unsupported bits per pixel-component (" + to_string(state.info_png.color.bitdepth) +
			                    ")");

		data_ = (void*)data;
		width_ = (size_t)width;
		height_ = (size_t)height;

		// Bye!
		lodepng_state_cleanup(&state);
		free(blob);
	}

	~PngImage()
	{
		free(data_);
	}
};


void AkoEnc(const vector<string>& args)
{
	string filename_input = "";
	string filename_output = "";
	bool verbose = false;
	bool checksum = false;
	bool benchmark = false;

	akoSettings settings = akoDefaultSettings();

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
			cout << version_string << endl;
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
		else if (Shared::CheckArgumentPair("-i", "--input", it, it_end) == 0)
			filename_input = *it;
		else if (Shared::CheckArgumentPair("-o", "--output", it, it_end) == 0)
			filename_output = *it;
		else if (Shared::CheckArgumentSingle("-dev-n", "--dev-no-compression", it) == 0)
			settings.compression = AKO_COMPRESSION_NONE;
		else if (Shared::CheckArgumentPair("-wr", "--wrap", it, it_end) == 0)
		{
			if (*it == "clamp" || *it == "0")
				settings.wrap = AKO_WRAP_CLAMP;
			else if (*it == "repeat" || *it == "1")
				settings.wrap = AKO_WRAP_REPEAT;
			else if (*it == "zero" || *it == "2")
				settings.wrap = AKO_WRAP_ZERO;
			else
				throw Shared::Error("Unknown wrap mode '" + *it + "'");
		}
		else if (Shared::CheckArgumentPair("-w", "--wavelet", it, it_end) == 0)
		{
			if (*it == "dd137" || *it == "0")
				settings.wavelet = AKO_WAVELET_DD137;
			else if (*it == "cdf53" || *it == "1")
				settings.wavelet = AKO_WAVELET_CDF53;
			else if (*it == "haar" || *it == "2")
				settings.wavelet = AKO_WAVELET_HAAR;
			else if (*it == "none" || *it == "3")
				settings.wavelet = AKO_WAVELET_NONE;
			else
				throw Shared::Error("Unknown wavelet transformation '" + *it + "'");
		}
		else if (Shared::CheckArgumentPair("-c", "--color", it, it_end) == 0)
		{
			if (*it == "ycocg" || *it == "0")
				settings.color = AKO_COLOR_YCOCG;
			else if (*it == "ycocg-r" || *it == "1")
				settings.color = AKO_COLOR_YCOCG_R;
			else if (*it == "subtract-g" || *it == "2")
				settings.color = AKO_COLOR_SUBTRACT_G;
			else if (*it == "rgb" || *it == "3")
				settings.color = AKO_COLOR_RGB;
			else
				throw Shared::Error("Unknown color transformation'" + *it + "'");
		}
		else if (Shared::CheckArgumentPair("-dev-t", "--dev-tiles", it, it_end) == 0)
			settings.tiles_dimension = stol(*it);
		else if (Shared::CheckArgumentPair("-q", "--quantization", it, it_end) == 0)
			settings.quantization = stoi(*it);
		else if (Shared::CheckArgumentPair("-g", "--gate", it, it_end) == 0)
			settings.gate = stoi(*it);
		else if (Shared::CheckArgumentPair("-d", "--discard-transparent-pixels", it, it_end) == 0) // :)
		{
			if (*it == "1" || *it == "true" || *it == "yes" || *it == "yup" || *it == "si")
				settings.discard_transparent_pixels = 1;
			else if (*it == "0" || *it == "false" || *it == "no" || *it == "nop")
				settings.discard_transparent_pixels = 0;
			else
				throw Shared::Error("Unknown boolean value '" + *it + "' for discard-transparent-pixels argument");
		}
		else
			throw Shared::Error("Unknown argument '" + *it + "'");
	}

	if (filename_input == "")
		throw Shared::Error("No input filename specified");

	// Open input
	if (verbose == true)
	{
		cout << "# AkoEnc v" << TOOLS_VERSION_MAJOR << "." << TOOLS_VERSION_MINOR << "." << TOOLS_VERSION_PATCH;
		cout << " (format " << akoFormatVersion() << ", library v" << akoVersionMajor() << "." << akoVersionMinor()
		     << "." << akoVersionPatch() << ")" << endl;

		cout << "Opening input: '" << filename_input << "'..." << endl;
	}

	const auto png = make_unique<PngImage>(filename_input);

	if (verbose == true)
		cout << "Input data: " << png->channels() << " channels, " << png->width() << "x" << png->height() << " px"
		     << endl;

	// Checksum data
	if (checksum == true)
	{
		auto value = Shared::Adler32((uint8_t*)png->data(), png->width() * png->height() * png->channels());

		if (verbose == true)
			cout << "Input data checksum: " << std::hex << value << std::dec << std::endl;
		else
			cout << "Checksum of input '" << filename_input << "' data: " << std::hex << value << std::dec << std::endl;
	}

	if (verbose == true)
	{
		cout << "[Compression: " << (int)settings.compression;
		cout << ", Color: " << (int)settings.color;
		cout << ", Wavelet: " << (int)settings.wavelet;
		cout << ", Wrap: " << (int)settings.wrap;
		cout << ", Tiles dimension: " << (int)settings.tiles_dimension;
		cout << ", Discard transparent pixels: " << (bool)settings.discard_transparent_pixels << "]" << endl;
	}

	// Encode
	void* blob = NULL;
	size_t blob_size = 0;
	{
		if (verbose == true)
			cout << "Encoding output: '" << filename_output << "'..." << endl;

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

		blob_size = akoEncodeExt(&callbacks, &settings, png->channels(), png->width(), png->height(), png->data(),
		                         &blob, &status);

		if (benchmark == true)
			total_benchmark.pause_stop(true, " - Total: ");

		if (blob_size == 0)
			throw Shared::Error("Ako encode error: '" + string(akoStatusString(status)) + "'");
	}

	// Write
	if (filename_output != "")
	{
		if (verbose == true)
			cout << "Writing output: '" << filename_output << "'..." << endl;

		Shared::WriteBlob(filename_output, blob, blob_size);
	}

	// Bye!
	const auto uncompressed_size = (double)(png->width() * png->height() * png->channels());
	const auto compressed_size = (double)blob_size;
	const auto bpp =
	    (compressed_size / (double)(png->width() * png->height() * png->channels())) * 8.0F * png->channels();

	printf("%.2f kB -> %.2f kB, ratio: %.2f:1, %.4f bpp\n", uncompressed_size / 1000.0F, compressed_size / 1000.0F,
	       uncompressed_size / compressed_size, bpp);

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
	catch (Shared::Error& e)
	{
		cout << e.info << std::endl;
		return 1;
	}

	return 0;
}
