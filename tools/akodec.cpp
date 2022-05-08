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

#define TOOLS_VERSION_MAJOR 0
#define TOOLS_VERSION_MINOR 2
#define TOOLS_VERSION_PATCH 0


// clang-format off
const LodePNGCompressSettings ZLIB_PRESET[10] ={
	{2, 1, 32768, 3, 256, 1, 0, 0, 0},
	{2, 1, 8192, 3, 256, 1, 0, 0, 0},
	{2, 1, 4096, 3, 128, 1, 0, 0, 0},
	{2, 1, 2048, 3, 128, 1, 0, 0, 0}, // lodepng_default_compress_settings
	{2, 1, 2048, 3, 92, 1, 0, 0, 0},
	{2, 1, 1024, 6, 92, 1, 0, 0, 0},
	{1, 1, 1024, 6, 64, 0, 0, 0, 0},
	{1, 1, 512, 6, 64, 0, 0, 0, 0},
	{1, 1, 512, 6, 32, 0, 0, 0, 0},
	{0, 1, 256, 6, 32, 0, 0, 0, 0}
};

const LodePNGFilterStrategy PNG_FILTER_PRESET[10] = {
	LFS_BRUTE_FORCE,
	LFS_MINSUM,
	LFS_MINSUM,
	LFS_MINSUM,
	LFS_MINSUM,
	LFS_MINSUM,
	LFS_MINSUM,
	LFS_MINSUM,
	LFS_MINSUM,
	LFS_ZERO
};
// clang-format on


class AkoImage
{
  private:
	size_t width;
	size_t height;
	size_t channels;
	void* data;

	akoWavelet wavelet;
	akoColor color;
	akoWrap wrap;
	akoCompression compression;
	size_t blob_size;

  public:
	// clang-format off
	size_t      get_width() const    { return width; };
	size_t      get_height() const   { return height; };
	size_t      get_channels() const { return channels; };
	const void* get_data() const     { return (const void*)(data); };

	akoWavelet     get_wavelet() const     { return wavelet; };
	akoColor       get_color() const       { return color; };
	akoWrap        get_wrap() const        { return wrap; };
	akoCompression get_compression() const { return compression; };
	size_t         get_blob_size() const   { return blob_size; };
	// clang-format on

	AkoImage(const std::string& filename, bool quiet, bool benchmark)
	{
		// Read file
		auto blob = std::vector<uint8_t>();
		{
			auto file = std::fstream(filename, std::ios::binary | std::ios_base::in | std::ios::ate);

			if (file.fail() == true)
				throw ErrorStr("Error at opening file '" + filename + "'");

			blob_size = file.tellg();
			blob.resize(blob_size);
			file.seekg(0, std::ios::beg);

			file.read((char*)blob.data(), blob.size());
			if (file.fail() == true)
				throw ErrorStr("Error at reading file '" + filename + "'");

			file.close();
		}

		// Decode
		akoSettings settings;
		{
			Stopwatch total_benchmark;
			akoCallbacks callbacks = akoDefaultCallbacks();
			akoStatus status = AKO_ERROR;

			if (benchmark == true && quiet == false)
			{
				total_benchmark.start(true);

				EventsData events_data;
				callbacks.events = EventsCallback;
				callbacks.events_data = &events_data;

				std::printf("Benchmark: \n");
			}

			data = (void*)akoDecodeExt(&callbacks, blob.size(), blob.data(), &settings, &channels, &width, &height,
			                           &status);

			if (benchmark == true && quiet == false)
				total_benchmark.pause_stop(true, " - Total: ");

			if (data == NULL)
				throw ErrorStr("Ako error: '" + std::string(akoStatusString(status)) + "'");
		}

		// Bye!
		wavelet = settings.wavelet;
		color = settings.color;
		wrap = settings.wrap;
		compression = settings.compression;
	}

	~AkoImage()
	{
		akoDefaultFree(data);
	}
};


void AkoDec(const std::string& filename_input, const std::string& filename_output, int effort, bool verbose = false,
            bool quiet = false, bool benchmark = false, bool checksum = false)
{
	if (filename_input == "")
		throw ErrorStr("No input filename specified");

	// Open input
	if (verbose == true)
	{
		std::printf("Ako decoding tool v%i.%i.%i\n", TOOLS_VERSION_MAJOR, TOOLS_VERSION_MINOR, TOOLS_VERSION_PATCH);
		std::printf(" - libako v%i.%i.%i, format %i\n", akoVersionMajor(), akoVersionMinor(), akoVersionPatch(),
		            akoFormatVersion());
		std::printf(" - lodepng %s\n", LODEPNG_VERSION_STRING);

		std::printf("Opening input: '%s'...\n", filename_input.c_str());
	}

	const auto ako = AkoImage(filename_input, quiet, benchmark);

	if (verbose == true)
		std::printf("Input data: %zu channels, %zux%zu px, wavelet: %i, color: %i, wrap: %i, compression: %i\n",
		            ako.get_channels(), ako.get_width(), ako.get_height(), (int)ako.get_wavelet(), (int)ako.get_color(),
		            (int)ako.get_wrap(), (int)ako.get_compression());

	// Checksum data
	uint32_t input_checksum = 0;
	if (checksum == true)
		input_checksum = Adler32((uint8_t*)ako.get_data(), ako.get_width() * ako.get_height() * ako.get_channels());

	// Encode
	if (verbose == true)
		std::printf("Encoding...\n");

	void* blob = NULL;
	size_t get_blob_size = 0;
	{
		LodePNGState state;
		lodepng_state_init(&state);

		state.info_raw.bitdepth = 8;

		switch (ako.get_channels())
		{
		case 1: state.info_raw.colortype = LCT_GREY; break;
		case 2: state.info_raw.colortype = LCT_GREY_ALPHA; break;
		case 3: state.info_raw.colortype = LCT_RGB; break;
		case 4: state.info_raw.colortype = LCT_RGBA; break;
		default: throw ErrorStr("Unsupported channels number (" + std::to_string(ako.get_channels()) + ")");
		}

		state.encoder.zlibsettings = ZLIB_PRESET[10 - effort];
		state.encoder.filter_strategy = PNG_FILTER_PRESET[10 - effort];

		const unsigned error = lodepng_encode((unsigned char**)(&blob), &get_blob_size, (unsigned char*)ako.get_data(),
		                                      (unsigned)ako.get_width(), (unsigned)ako.get_height(), &state);

		if (error != 0)
			throw ErrorStr("LodePng error: '" + std::string(lodepng_error_text(error)) + "'");
	}

	// Write output
	if (filename_output != "")
	{
		if (verbose == true)
			std::printf("Writing output: '%s'...\n", filename_output.c_str());

		WriteBlob(filename_output, blob, get_blob_size);
	}

	// Bye!
	const auto uncompressed_size = (double)(ako.get_width() * ako.get_height() * ako.get_channels());
	const auto compressed_size = (double)ako.get_blob_size();
	const auto bpp = (compressed_size / (double)(ako.get_width() * ako.get_height() * ako.get_channels())) * 8.0F *
	                 ako.get_channels();

	if (quiet == false)
	{
		if (checksum == true)
			std::printf("(%08x) %.2f kB <- %.2f kB, ratio: %.2f:1, %.4f bpp\n", input_checksum,
			            uncompressed_size / 1000.0F, compressed_size / 1000.0F, uncompressed_size / compressed_size,
			            bpp);
		else
			std::printf("%.2f kB <- %.2f kB, ratio: %.2f:1, %.4f bpp\n", uncompressed_size / 1000.0F,
			            compressed_size / 1000.0F, uncompressed_size / compressed_size, bpp);
	}

	std::free(blob);
}


int main(int argc, const char* argv[])
{
	std::string input_filename;
	std::string output_filename;
	int effort = 7;
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
		opts.add_integer("-e", "--effort", "Computational effort to encode output, from 1 to 10.", 7, 1, 10,
		                 encoding_category);

		const auto extra_category = opts.add_category("EXTRA TOOLS");
		opts.add_bool("-b", "--benchmark", "", extra_category);
		opts.add_bool("-ch", "--checksum", "", extra_category);

		if (opts.parse_arguments(argc, argv) != 0)
			return 1;

		// Help message
		if (opts.get_bool("--help") == true)
		{
			std::printf("USAGE\n");
			std::printf("    akodec [optional options] -i <input filename> -o <output filename>\n");
			std::printf("    akodec [optional options] -i <input filename>\n");
			std::printf("\n    Only PNG files supported as output.\n");
			std::printf("\n");

			opts.print_help();

			return 0;
		}

		// Version message
		if (opts.get_bool("--version") == true)
		{
			std::printf("Ako decoding tool v%i.%i.%i\n", TOOLS_VERSION_MAJOR, TOOLS_VERSION_MINOR, TOOLS_VERSION_PATCH);
			std::printf(" - libako v%i.%i.%i, format %i\n", akoVersionMajor(), akoVersionMinor(), akoVersionPatch(),
			            akoFormatVersion());
			std::printf(" - lodepng %s\n", LODEPNG_VERSION_STRING);
			std::printf("\n");
			std::printf("Copyright (c) 2021-2022 Alexander Brandt. Under MIT License.\n");
			std::printf("\n");
			std::printf("More information at 'https://github.com/baAlex/Ako'\n");
			return 0;
		}

		// Set decoding settings
		input_filename = opts.get_string("--input");
		output_filename = opts.get_string("--output");

		effort = opts.get_integer("--effort");
		verbose = opts.get_bool("--verbose");
		quiet = opts.get_bool("--quiet");
		benchmark = opts.get_bool("--benchmark");
		checksum = opts.get_bool("--checksum");
	}

	// Decode!
	try
	{
		AkoDec(input_filename, output_filename, effort, verbose, quiet, benchmark, checksum);
		return 0;
	}
	catch (ErrorStr& e)
	{
		std::cout << e.info << "\n";
		return 1;
	}

	return 0;
}
