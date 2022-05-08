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


#include "benchmark.hpp"
#include "misc.hpp"
#include "options.hpp"

#include "thirdparty/lodepng.h"

extern "C"
{
#include "ako.h"
}

#define VERSION_MAJOR 0
#define VERSION_MINOR 2
#define VERSION_PATCH 0


class PngImage
{
  private:
	size_t width;
	size_t height;
	size_t channels;
	void* data;

  public:
	// clang-format off
	size_t      get_width() const    { return width; };
	size_t      get_height() const   { return height; };
	size_t      get_channels() const { return channels; };
	const void* get_data() const     { return (const void*)(data); };
	// clang-format on

	PngImage(const std::string& filename)
	{
		unsigned char* blob;
		size_t blob_size;

		LodePNGState state;
		unsigned error;

		unsigned png_width;
		unsigned png_height;
		unsigned char* png_data;

		// Set
		lodepng_state_init(&state);
		state.decoder.color_convert = 0; // Do not convert color

		// Load/decode
		if ((error = lodepng_load_file(&blob, &blob_size, filename.c_str())) != 0)
			throw ErrorStr("LodePng error: '" + std::string(lodepng_error_text(error)) + "'");

		if ((error = lodepng_decode(&png_data, &png_width, &png_height, &state, blob, blob_size)) != 0)
			throw ErrorStr("LodePng error: '" + std::string(lodepng_error_text(error)) + "'");

		// Validate
		switch (state.info_png.color.colortype)
		{
		case LCT_GREY: channels = 1; break;
		case LCT_GREY_ALPHA: channels = 2; break;
		case LCT_RGB: channels = 3; break;
		case LCT_RGBA: channels = 4; break;
		default: throw ErrorStr("Unsupported channels number (" + std::to_string(state.info_png.color.colortype) + ")");
		}

		if (state.info_png.color.bitdepth != 8)
			throw ErrorStr("Unsupported bits per pixel-component (" + std::to_string(state.info_png.color.bitdepth) +
			               ")");

		// Bye!
		data = (void*)png_data;
		width = (size_t)png_width;
		height = (size_t)png_height;

		lodepng_state_cleanup(&state);
		free(blob);
	}

	~PngImage()
	{
		free(data);
	}
};


size_t EncodePass(bool verbose, int ratio, const akoCallbacks* callbacks, const akoSettings* settings, size_t channels,
                  size_t width, size_t height, const void* in, void** out, akoStatus* out_status)
{
	// One pass, no ratio specified
	if (ratio == 0 || settings->wavelet == AKO_WAVELET_NONE || settings->compression == AKO_COMPRESSION_NONE)
	{
		return akoEncodeExt(callbacks, settings, channels, width, height, in, out, out_status);
	}

	// Lossless, if ratio == 1
	else if (ratio == 1)
	{
		auto new_settings = *settings;
		new_settings.quantization = 0;
		new_settings.gate = 0;
		return akoEncodeExt(callbacks, &new_settings, channels, width, height, in, out, out_status);
	}

	// Multiple passes to find a quantization value that
	// place us close to the desired compression ratio
	{
		const size_t target_size = (width * height * channels) / ratio;
		const size_t error_margin = (target_size * 4) / 100;

		if (verbose == true)
			std::printf("Target: %.2f kB, error: %.2f kB...\n", (double)target_size / 1000.0F,
			            (double)error_margin / 1000.0F);

		// Ceil zero
		auto new_settings = *settings;
		new_settings.quantization = 0;

		size_t ceil_size = akoEncodeExt(callbacks, &new_settings, channels, width, height, in, out, out_status);

		// Exponentially find a floor
		new_settings.quantization = 1;

		size_t floor_size = ceil_size;
		int floor_q = 0;
		int ceil_q = 0;
		do
		{
			new_settings.quantization *= 4;

			ceil_size = floor_size;
			ceil_q = floor_q;

			floor_size = akoEncodeExt(callbacks, &new_settings, channels, width, height, in, out, out_status);
			floor_q = new_settings.quantization;

			if (verbose == true)
				std::printf(" - Q: %i|%i, %.1f|%.1f kB\n", ceil_q, floor_q, (double)ceil_size / 1000.0F,
				            (double)floor_size / 1000.0F);

		} while (floor_size > target_size);

		// Divide range in two
		size_t last_size = floor_size;
		while (std::max(floor_size, ceil_size) - std::min(floor_size, ceil_size) > error_margin &&
		       std::abs(floor_q - ceil_q) > 1)
		{
			new_settings.quantization = (ceil_q + floor_q) / 2;
			last_size = akoEncodeExt(callbacks, &new_settings, channels, width, height, in, out, out_status);

			if (last_size > target_size)
			{
				ceil_size = last_size;
				ceil_q = new_settings.quantization;
			}
			else
			{
				floor_size = last_size;
				floor_q = new_settings.quantization;
			}

			if (verbose == true)
				std::printf(" - Q: %i|%i, %.1f|%.1f kB\n", ceil_q, floor_q, (double)ceil_size / 1000.0F,
				            (double)floor_size / 1000.0F);
		}

		// Bye!
		if (std::max(floor_size, target_size) - std::min(floor_size, target_size) <
		    std::max(ceil_size, target_size) - std::min(ceil_size, target_size))
		{
			if (verbose == true)
				std::printf(" - Q: %i\n", floor_q);

			new_settings.quantization = floor_q;
			if (last_size == floor_size)
				return last_size;
		}
		else
		{
			if (verbose == true)
				std::printf(" - Q: %i\n", ceil_q);

			new_settings.quantization = ceil_q;
			if (last_size == ceil_size)
				return last_size;
		}

		return akoEncodeExt(callbacks, &new_settings, channels, width, height, in, out, out_status);
	}

	return 0;
}


void AkoEnc(const akoSettings& settings, const std::string& filename_input, const std::string& filename_output,
            int ratio = 0, bool verbose = false, bool quiet = false, bool benchmark = false, bool checksum = false)
{
	if (filename_input == "")
		throw ErrorStr("No input filename specified");

	// Open input
	if (verbose == true)
	{
		std::printf("Ako encoding tool v%i.%i.%i\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
		std::printf(" - libako v%i.%i.%i, format %i\n", akoVersionMajor(), akoVersionMinor(), akoVersionPatch(),
		            akoFormatVersion());
		std::printf(" - lodepng %s\n", LODEPNG_VERSION_STRING);

		std::printf("Opening input: '%s'...\n", filename_input.c_str());
	}

	const auto png = PngImage(filename_input);

	if (verbose == true)
		std::printf("Input data: %zu channels, %zux%zu px\n", png.get_channels(), png.get_width(), png.get_height());

	// Checksum data
	uint32_t input_checksum = 0;
	if (checksum == true)
		input_checksum = Adler32((uint8_t*)png.get_data(), png.get_width() * png.get_height() * png.get_channels());

	// Encode
	if (verbose == true)
	{
		std::printf("Encoding...\n");

		std::printf("[Wavelet: %i", (int)settings.wavelet);
		std::printf(", color: %i", (int)settings.color);
		std::printf(", wrap: %i", (int)settings.wrap);
		std::printf(", compression %i", (int)settings.compression);
		std::printf(", chroma loss: %i", (int)settings.chroma_loss);
		std::printf(", discard non-visible: %i]\n", (int)settings.discard_non_visible);
	}

	void* blob = NULL;
	size_t blob_size = 0;
	{
		Stopwatch total_benchmark;
		akoCallbacks callbacks = akoDefaultCallbacks();
		akoStatus status = AKO_ERROR;

		if (benchmark == true && quiet == false)
		{
			total_benchmark.start(true);

			if (ratio == 0)
			{
				EventsData events_data;
				callbacks.events = EventsCallback;
				callbacks.events_data = &events_data;
				std::printf("Benchmark: \n");
			}
		}

		blob_size = EncodePass(verbose, ratio, &callbacks, &settings, png.get_channels(), png.get_width(),
		                       png.get_height(), png.get_data(), &blob, &status);

		if (benchmark == true && quiet == false)
		{
			if (ratio != 0)
				std::printf("Benchmark: \n");

			total_benchmark.pause_stop(true, " - Total: ");
		}

		if (blob_size == 0)
			throw ErrorStr("Ako error: '" + std::string(akoStatusString(status)) + "'");
	}

	// Write output
	if (filename_output != "")
	{
		if (verbose == true)
			std::printf("Writing output: '%s'...\n", filename_output.c_str());

		WriteBlob(filename_output, blob, blob_size);
	}

	// Bye!
	const auto uncompressed_size = (double)(png.get_width() * png.get_height() * png.get_channels());
	const auto compressed_size = (double)blob_size;
	const auto bpp = (compressed_size / (double)(png.get_width() * png.get_height() * png.get_channels())) * 8.0F *
	                 png.get_channels();

	if (quiet == false)
	{
		if (checksum == true)
			std::printf("(%08x) %.2f kB -> %.2f kB, ratio: %.2f:1, %.4f bpp\n", input_checksum,
			            uncompressed_size / 1000.0F, compressed_size / 1000.0F, uncompressed_size / compressed_size,
			            bpp);
		else
			std::printf("%.2f kB -> %.2f kB, ratio: %.2f:1, %.4f bpp\n", uncompressed_size / 1000.0F,
			            compressed_size / 1000.0F, uncompressed_size / compressed_size, bpp);
	}

	akoDefaultFree(blob);
}


int main(int argc, const char* argv[])
{
	akoSettings settings = akoDefaultSettings();
	std::string input_filename;
	std::string output_filename;
	int ratio = 0;
	bool verbose = false;
	bool quiet = false;
	bool benchmark = false;
	bool checksum = false;

	// Options
	{
		auto opts = OptionsManager();

		const auto print_category = opts.add_category("PRINT OPTIONS");
		opts.add_bool("-v", "--version", "Print program version and license terms.", print_category);
		opts.add_bool("-h", "--help", "Print this help.", print_category);
		opts.add_bool("-verbose", "--verbose", "Print all available information while encoding.", print_category);
		opts.add_bool("-quiet", "--quiet", "Don't print anything.", print_category);

		const auto io_category = opts.add_category("INPUT/OUTPUT OPTIONS");
		opts.add_string("-i", "--input", "Input filename.", "", "", io_category);
		opts.add_string(
		    "-o", "--output",
		    "Output filename. If not specified, all operations will take place then the result will be discarded.", "",
		    "", io_category);

		const auto encoding_category = opts.add_category("ENCODING OPTIONS");
		opts.add_integer("-q", "--quantization",
		                 "Controls loss in the final image and in turn, compression gains. Achieves it by reducing "
		                 "wavelet coefficients accuracy accordingly  to the provided value. Default and recommended "
		                 "loss method. Set it to zero for lossless compression.",
		                 16, 0, 8192, encoding_category);
		opts.add_integer("-g", "--noise-gate",
		                 "Loss method similar to '--quantization', however it removes wavelet coefficients under a "
		                 "threshold set by the provided value. Set it to zero for lossless compression.",
		                 0, 0, 8192, encoding_category);
		opts.add_string("-w", "--wavelet",
		                "Wavelet transformation to apply. Options are: DD137, CDF53, HAAR and NONE. For lossy "
		                "compression DD137 provides better results, for lossless CDF53.",
		                "DD137", "DD137 CDF53 HAAR NONE", encoding_category);
		opts.add_string("-c", "--color",
		                "Color transformation to apply. Options are: YCOCG, SUBTRACT-G and NONE. All options serve "
		                "both lossy and lossless compression.",
		                "YCOCG", "YCOCG SUBTRACT-G NONE", encoding_category);
		opts.add_string("-wr", "--wrap",
		                "Controls how loss wrap around image borders. Options are: CLAMP, MIRROR, REPEAT and ZERO; "
		                "all them analog to texture-wrapping options in OpenGL. Two common choices are CLAMP that "
		                "perform no wrap, and REPEAT for images intended to serve as tiled textures.",
		                "CLAMP", "CLAMP MIRROR REPEAT ZERO", encoding_category);
		opts.add_integer("-chroma-loss", "--chroma-loss",
		                 "Subsampling thingie. Zero to disable it. Only applies if either '--quantization' or "
		                 "'--noise-gate' is set.",
		                 1, 0, 8192, encoding_category);
		opts.add_bool("-d", "--discard-non-visible",
		              "Discard pixels that do not contribute to the final image (those in transparent areas). For "
		              "lossless compression do not set this option.",
		              encoding_category);

		const auto extra_category = opts.add_category("EXTRA TOOLS");
		opts.add_bool("-b", "--benchmark", "", extra_category);
		opts.add_bool("-ch", "--checksum", "", extra_category);

		const auto experimental_category = opts.add_category("EXPERIMENTAL");
		opts.add_integer("-dev-r", "--dev-ratio", "", 0, 0, 4096, experimental_category);
		opts.add_string("-dev-compression", "--dev-compression",
		                "Compression method, options are: KAGARI, MANBAVARAN and NONE. Be caution of the last two as "
		                "they can crash your computer, corrupt files, produce invalid data, misbrew your colombian "
		                "coffee and everything in between.",
		                "KAGARI", "KAGARI MANBAVARAN NONE", experimental_category);

		if (opts.parse_arguments(argc, argv) != 0)
			return 1;

		// Help message
		if (opts.get_bool("--help") == true)
		{
			std::printf("USAGE\n");
			std::printf("    akoenc [optional options] -i <input filename> -o <output filename>\n");
			std::printf("    akoenc [optional options] -i <input filename>\n");
			std::printf("\n    Only PNG files supported as input.\n");
			std::printf("\n");

			opts.print_help();

			return 0;
		}

		// Version message
		if (opts.get_bool("--version") == true)
		{
			std::printf("Ako encoding tool v%i.%i.%i\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
			std::printf(" - libako v%i.%i.%i, format %i\n", akoVersionMajor(), akoVersionMinor(), akoVersionPatch(),
			            akoFormatVersion());
			std::printf(" - lodepng %s\n", LODEPNG_VERSION_STRING);
			std::printf("\n");
			std::printf("Copyright (c) 2021-2022 Alexander Brandt. Under MIT License.\n");
			std::printf("\n");
			std::printf("More information at 'https://github.com/baAlex/Ako'\n");
			return 0;
		}

		// Set encoding settings
		input_filename = opts.get_string("--input");
		output_filename = opts.get_string("--output");

		verbose = opts.get_bool("--verbose");
		quiet = opts.get_bool("--quiet");
		benchmark = opts.get_bool("--benchmark");
		checksum = opts.get_bool("--checksum");

		settings.quantization = opts.get_integer("--quantization");
		settings.gate = opts.get_integer("--noise-gate");
		settings.discard_non_visible = opts.get_bool("--discard-non-visible");
		settings.wavelet = (akoWavelet)opts.get_string_index("--wavelet");
		settings.color = (akoColor)opts.get_string_index("--color");
		settings.wrap = (akoWrap)opts.get_string_index("--wrap");
		settings.chroma_loss = opts.get_integer("--chroma-loss");

		ratio = opts.get_integer("--dev-ratio");
		settings.compression = (akoCompression)opts.get_string_index("--dev-compression");
	}

	// Encode!
	try
	{
		AkoEnc(settings, input_filename, output_filename, ratio, verbose, quiet, benchmark, checksum);
		return 0;
	}
	catch (ErrorStr& e)
	{
		std::cout << e.info << "\n";
		return 1;
	}

	return 0;
}
