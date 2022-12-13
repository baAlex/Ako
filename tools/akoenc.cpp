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

#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "CLI/CLI.hpp"

#include "common/adler32.hpp"
#include "common/callbacks.hpp"
#include "common/developer.hpp"
#include "common/misc.hpp"
#include "common/png.hpp"


const static auto s_color_transformer = // NOLINT(cert-err58-cpp)
    std::map<std::string, ako::Color>({
        {ako::ToString(ako::Color::YCoCg), ako::Color::YCoCg},         //
        {ako::ToString(ako::Color::SubtractG), ako::Color::SubtractG}, //
        {ako::ToString(ako::Color::None), ako::Color::None}            //
    });

const static auto s_wavelet_transformer = // NOLINT(cert-err58-cpp)
    std::map<std::string, ako::Wavelet>({
        {ako::ToString(ako::Wavelet::Dd137), ako::Wavelet::Dd137}, //
        {ako::ToString(ako::Wavelet::Cdf53), ako::Wavelet::Cdf53}, //
        {ako::ToString(ako::Wavelet::Haar), ako::Wavelet::Haar}    //
    });

const static auto s_wrap_transformer = // NOLINT(cert-err58-cpp)
    std::map<std::string, ako::Wrap>({
        {ako::ToString(ako::Wrap::Clamp), ako::Wrap::Clamp},   //
        {ako::ToString(ako::Wrap::Mirror), ako::Wrap::Mirror}, //
        {ako::ToString(ako::Wrap::Repeat), ako::Wrap::Repeat}, //
        {ako::ToString(ako::Wrap::Zero), ako::Wrap::Zero}      //
    });

const static auto s_compression_transformer = // NOLINT(cert-err58-cpp)
    std::map<std::string, ako::Compression>({
        {ako::ToString(ako::Compression::Kagari), ako::Compression::Kagari},         //
        {ako::ToString(ako::Compression::Manbavaran), ako::Compression::Manbavaran}, //
        {ako::ToString(ako::Compression::None), ako::Compression::None},             //
    });


int main(int argc, const char* argv[])
{
	std::string input_filename = "";
	std::string output_filename = "";
	std::string histogram_output_filename = "";
	bool checksum = false;
	bool verbose = false;
	bool quiet = false;
	bool benchmark = false;

	auto s = ako::DefaultSettings();

	// Parse arguments
	{
		using namespace std;
		using namespace ako;
		using namespace CLI;

		CLI::App app{"Ako Encoding Tool"};

		char version_string[256];
		snprintf(version_string, 256,
		         "Ako Encoding Tool\nv%i.%i.%i\n\nCopyright (c) 2021-2022 Alexander Brandt."
		         "\nDistribution terms specified in LICENSE and THIRDPARTY files.\n",
		         VersionMajor(), VersionMinor(), VersionPatch());

		// clang-format off
		app.add_option("Input", input_filename, "Input filename")
			->option_text("FILENAME")->required()->take_first();

		app.set_version_flag("-v,--version", version_string);

		app.add_flag("--quiet", quiet,                 "Do not print anything");
		app.add_flag("--verbose", verbose,             "Print all available information while running");
		app.add_option("-o,--output", output_filename, "Output filename")->option_text("FILENAME");
		app.add_flag("-k,--checksum", checksum,        "Checksum input image");
		app.add_flag("-b,--benchmark", benchmark,      "Benchmark");

		app.add_option("--histogram", histogram_output_filename, "Histogram output filename")->option_text("FILENAME");

		app.add_option("-c,--color", s.color, "Color transformation: YCOCG, SUBTRACTG or NONE.\n"
		                                      "[Default is " + string(ToString(s.color)) + "]")
		                                      ->option_text("OPTION")
		                                      ->transform(CheckedTransformer(s_color_transformer, ignore_case));

		app.add_option("-w,--wavelet", s.wavelet, "Wavelet transformation: DD137, CDF53 or HAAR.\n"
		                                          "[Default is " + string(ToString(s.wavelet)) + "]")
		                                          ->option_text("OPTION")
		                                          ->transform(CheckedTransformer(s_wavelet_transformer, ignore_case));

		app.add_option("-p,--wrap", s.wrap, "Wrap mode: CLAMP, MIRROR, REPEAT or ZERO.\n"
		                                    "[Default is " + string(ToString(s.wrap)) + "]")
		                                    ->option_text("OPTION")
		                                    ->transform(CheckedTransformer(s_wrap_transformer, ignore_case));

		app.add_option("-z,--compression", s.compression, "Compression method: KAGARI, MANBAVARAN or NONE.\n"
		                                                  "[Default is " + string(ToString(s.compression)) + "]")
		                                                  ->option_text("OPTION")
		                                                  ->transform(CheckedTransformer(s_compression_transformer, ignore_case));

		app.add_option("-q,--quantization", s.quantization, "Quantization, controls loss. Zero for lossless.\n"
		                                                    "[Default is " + to_string(s.quantization) +"]")
		                                                    ->option_text("NUMBER");

		/*app.add_option("-g,--noise-gate", s.gate, "Noise gate threshold, controls loss. Zero for lossless.\n"
		                                          "[Default is " + to_string(s.gate) +"]")
		                                          ->option_text("NUMBER");

		app.add_option("-l,--chroma-loss", s.chroma_loss, "Multiplies quantization and noise-gate threshold on\n"
		                                                  "chroma channels. Zero or 1 for same loss in both luma\n"
		                                                  "and chroma channels.\n"
		                                                  "[Default is " + to_string(s.chroma_loss) +"]")->option_text("NUMBER");*/

		app.add_flag("-d,--discard", s.discard, "Discards pixels under fully transparent areas. If set\n"
		                                        "output will be lossy.\n"
		                                        "[" + ((s.discard) ? string("Set") : string("No set")) + " by default]");

		app.add_option("-r,--ratio", s.ratio, "Ratio. Zero for lossless.\n"
		                                      "[Default is " + to_string(s.ratio) +"]")
		                                      ->option_text("NUMBER");

		app.add_option("-t,--tiles-size", s.tiles_dimension, "Tiles size, must be a power of two. Zero traits image\nas a whole.\n"
		                                                     "[Default is " + to_string(s.tiles_dimension) +"]")
		                                                     ->option_text("NUMBER");
		// clang-format on

		CLI11_PARSE(app, argc, argv);

		s.quantization = (s.quantization > 0.0F) ? s.quantization : 0.0F;
		// s.gate = (s.gate > 0.0F) ? s.gate : 0.0F;
		// s.chroma_loss = (s.chroma_loss > 0.0F) ? s.chroma_loss : 0.0F;
		s.ratio = (s.ratio > 0.0F) ? s.ratio : 0.0F;
	}

	if (quiet == false && verbose == true)
		std::cout << "\n";

	// Read input file
	auto input_blob = std::vector<uint8_t>();
	{
		if (quiet == false && verbose == true)
			std::cout << "Input file name: '" << input_filename << "'\n";

		if (ReadFile(input_filename, input_blob) != 0)
			return EXIT_FAILURE;

		if (quiet == false && verbose == true)
			std::cout << "Input file size: " << input_blob.size() << " byte(s)\n";
	}

	// Decode
	void* input_image = nullptr;
	unsigned width = 0;
	unsigned height = 0;
	unsigned channels = 0;
	unsigned depth = 0;
	{
		if (quiet == false && verbose == true)
			std::cout << "Decoding PNG...\n";

		if (DecodePng(input_blob.size(), input_blob.data(), width, height, channels, depth, &input_image) != 0)
			return EXIT_FAILURE;

		input_blob.resize(1);
	}

	// Checksum image
	uint32_t hash = 0;
	if (checksum == true)
	{
		hash = Adler32(input_image, (width * height * channels * (depth / 8)));

		if (quiet == false && verbose == true)
			std::cout << "Input image hash: " << std::hex << hash << std::dec << " (Adler32)\n";
	}

	// Encode
	void* encoded_blob = nullptr;
	size_t encoded_blob_size = 0;
	{
		if (quiet == false && verbose == true)
		{
			PrintSettings(s, "encoder-side");
			std::cout << "Encoding Ako...\n";
		}

		// Configure callbacks
		auto callbacks = ako::DefaultCallbacks();
		CallbacksData callbacks_data = {};
		callbacks_data.histogram_output = histogram_output_filename;

		if (quiet == false)
		{
			callbacks.user_data = &callbacks_data;

			callbacks_data.print = verbose;
			callbacks_data.prefix = "E |";

			callbacks.generic_event = CallbackGenericEvent;
			callbacks.format_event = CallbackFormatEvent;
			callbacks.lifting_event = CallbackLiftingEvent;
			callbacks.compression_event = CallbackCompressionEvent;
			callbacks.histogram_event = CallbackHistogramEvent;
		}

		// Encode
		auto status = ako::Status::Error;
		encoded_blob_size =
		    ako::EncodeEx(callbacks, s, width, height, channels, 8, input_image, &encoded_blob, &status);

		if (status != ako::Status::Ok)
		{
			std::cout << "Ako error: '" << ako::ToString(status) << "'\n";
			return EXIT_FAILURE;
		}

		if (quiet == false && verbose == true)
			std::cout << "Encoded blob size: " << encoded_blob_size << " byte(s)\n";

		free(input_image);

		// Benchmark
		if (quiet == false && benchmark == true)
		{
			// clang-format off
			std::cout << "Benchmark:\n";
			std::cout << " - Format:      " << static_cast<double>(callbacks_data.format_duration.count()) / 1000.0 << " ms\n";
			std::cout << " - Lift:        " << static_cast<double>(callbacks_data.lifting_duration.count()) / 1000.0 << " ms\n";
			std::cout << " - Compression: " << static_cast<double>(callbacks_data.compression_duration.count()) / 1000.0 << " ms\n";
			std::cout << " - Total:       " << static_cast<double>(callbacks_data.total_duration.count()) / 1000.0 << " ms\n";
			// clang-format on
		}
	}

	// Write output file
	if (output_filename != "")
	{
		if (quiet == false && verbose == true)
			std::cout << "Output file name: '" << output_filename << "'\n";

		if (WriteFile(output_filename, encoded_blob_size, encoded_blob) != 0)
			return EXIT_FAILURE;
	}

	ako::DefaultCallbacks().free(encoded_blob);

	// Bye!
	if (quiet == false)
	{
		const auto uncompressed_size = static_cast<float>(width * height * channels) / 1000.0F;
		const auto compressed_size = static_cast<float>(encoded_blob_size) / 1000.0F;
		const auto bpp = (static_cast<float>(encoded_blob_size) / static_cast<float>(width * height * channels)) *
		                 8.0F * static_cast<float>(channels);

		if (checksum == true)
			printf("(%x) %.2f kB -> %.2f kB, ratio: %.2f:1, %.4f bpp\n", hash, uncompressed_size, compressed_size,
			       uncompressed_size / compressed_size, bpp);
		else
			printf("%.2f kB -> %.2f kB, ratio: %.2f:1, %.4f bpp\n", uncompressed_size, compressed_size,
			       uncompressed_size / compressed_size, bpp);
	}

	return EXIT_SUCCESS;
}
