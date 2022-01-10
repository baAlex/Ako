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
			throw Modern::Error("LodePng error: '" + string(lodepng_error_text(error)) + "'");

		if ((error = lodepng_decode(&data, &width, &height, &state, blob, blob_size)) != 0)
			throw Modern::Error("LodePng error: '" + string(lodepng_error_text(error)) + "'");

		// Validate
		switch (state.info_png.color.colortype)
		{
		case LCT_GREY: channels_ = 1; break;
		case LCT_GREY_ALPHA: channels_ = 2; break;
		case LCT_RGB: channels_ = 3; break;
		case LCT_RGBA: channels_ = 4; break;
		default: throw Modern::Error("Unsupported channels number (" + to_string(state.info_png.color.colortype) + ")");
		}

		if (state.info_png.color.bitdepth != 8)
			throw Modern::Error("Unsupported bits per pixel-component (" + to_string(state.info_png.color.bitdepth) +
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
		else if (Modern::CheckArgumentPair("-r", "--wrap", it, it_end) == 0)
		{
			if (*it == "clamp" || *it == "0")
				settings.wrap = AKO_WRAP_CLAMP;
			else if (*it == "repeat" || *it == "1")
				settings.wrap = AKO_WRAP_REPEAT;
			else if (*it == "zero" || *it == "2")
				settings.wrap = AKO_WRAP_ZERO;
			else
				throw Modern::Error("Unknown wrap mode '" + *it + "'");
		}
		else if (Modern::CheckArgumentPair("-w", "--wavelet", it, it_end) == 0)
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
				throw Modern::Error("Unknown wavelet '" + *it + "'");
		}
		else if (Modern::CheckArgumentPair("-c", "--colorspace", it, it_end) == 0)
		{
			if (*it == "ycocg" || *it == "0")
				settings.colorspace = AKO_COLORSPACE_YCOCG;
			else if (*it == "ycocg-r" || *it == "1")
				settings.colorspace = AKO_COLORSPACE_YCOCG_R;
			else if (*it == "rgb" || *it == "2")
				settings.colorspace = AKO_COLORSPACE_RGB;
			else
				throw Modern::Error("Unknown colorspace '" + *it + "'");
		}
		else if (Modern::CheckArgumentPair("-t", "--tiles", it, it_end) == 0)
			settings.tiles_dimension = stol(*it);
		else
			throw Modern::Error("Unknown argument '" + *it + "'");
	}

	if (filename_input == "")
		throw Modern::Error("No input filename specified");

	// Open input
	const auto png = make_unique<PngImage>(filename_input);

	if (verbose == true)
	{
		cout << "AkoEnc: '" << filename_input << "', " << png->channels() << " channel(s), " << png->width() << "x"
		     << png->height() << " pixels" << endl;

		cout << " - tiles dimension: " << settings.tiles_dimension << " px, wrap mode: " << (int)settings.wrap
		     << ", wavelet: " << (int)settings.wavelet << ", colorspace: " << settings.colorspace << endl;

		if (benchmark == true)
			cout << endl;
	}

	// Encode
	void* blob = NULL;
	size_t blob_size = 0;
	{
		akoCallbacks callbacks = akoDefaultCallbacks();
		akoStatus status = AKO_ERROR;

		if (benchmark == true)
		{
			Modern::EventsData events_data;
			callbacks.events = Modern::EventsCallback;
			callbacks.events_data = &events_data;
		}

		blob_size = akoEncodeExt(&callbacks, &settings, png->channels(), png->width(), png->height(), png->data(),
		                         &blob, &status);

		if (blob_size == 0)
			throw Modern::Error("Ako encode error: '" + string(akoStatusString(status)) + "'");
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

		AkoEnc(*args);
	}
	catch (Modern::Error& e)
	{
		cout << e.info << std::endl;
		return 1;
	}

	return 0;
}
