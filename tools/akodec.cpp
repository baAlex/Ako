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
#include <string>
#include <vector>

#include "ako.hpp"
#include "shared.hpp"

#include "thirdparty/CLI11.hpp"
#include "thirdparty/lodepng.h"


int main(int argc, const char* argv[])
{
	std::string input_filename = "";
	std::string output_filename = "";
	int effort = 7;
	bool checksum = false;
	bool verbose = false;
	bool quiet = false;
	bool benchmark = false;

	// Parse arguments
	{
		using namespace std;
		using namespace ako;
		using namespace CLI;

		CLI::App app{"Ako Decoding Tool"};

		char version_string[256];
		snprintf(version_string, 256,
		         "Ako Decoding Tool\nv%i.%i.%i\n\nCopyright (c) 2021-2022 Alexander Brandt."
		         "\nDistribution terms specified in LICENSE and THIRDPARTY files.\n",
		         VersionMajor(), VersionMinor(), VersionPatch());

		// clang-format off
		app.add_option("Input", input_filename, "Input filename")
			->option_text("FILENAME")->required()->take_first();

		app.set_version_flag("-v,--version", version_string);

		app.add_flag("--quiet", quiet,                 "Do not print anything");
		app.add_flag("--verbose", verbose,             "Print all available information while running");
		app.add_option("-o,--output", output_filename, "Output filename")->option_text("FILENAME");
		app.add_flag("-k,--checksum", checksum,        "Checksum output image");
		app.add_flag("-b,--benchmark", benchmark,      "Benchmark");

		app.add_option("-e,--effort", effort, "Encoding effort. From 1 to 10.\n[Default is " + to_string(effort) + "]");
		// clang-format on

		CLI11_PARSE(app, argc, argv);
	}

	if (quiet == false && verbose == true)
		std::cout << "\n";

	// Read input file
	auto input_blob = std::vector<uint8_t>();
	size_t input_blob_size = 0;
	{
		if (quiet == false && verbose == true)
			std::cout << "Input file name: '" << input_filename << "'\n";

		if (ReadFile(input_filename, input_blob) != 0)
			return EXIT_FAILURE;

		if (quiet == false && verbose == true)
			std::cout << "Input file size: " << input_blob.size() << " byte(s)\n";

		input_blob_size = input_blob.size();
	}

	// Decode
	void* input_image = nullptr;
	unsigned width = 0;
	unsigned height = 0;
	unsigned channels = 0;
	unsigned depth = 0;
	{
		if (quiet == false && verbose == true)
			std::cout << "Decoding Ako...\n";

		// Configure callbacks
		auto callbacks = ako::DefaultCallbacks();
		CallbacksData callbacks_data = {};

		if (quiet == false)
		{
			callbacks.user_data = &callbacks_data;
			callbacks_data.side = "decoder-side";

			if (verbose == true)
			{
				callbacks.generic_event = CallbackGenericEvent;
				callbacks.format_event = CallbackFormatEvent;
				callbacks.compression_event = CallbackCompressionEvent;
			}
			else if (benchmark == true)
				callbacks.format_event = CallbackBenchmarkFormatEvent;
		}

		// Decode
		ako::Settings s = {};
		auto status = ako::Status::Error;

		input_image =
		    ako::DecodeEx(callbacks, input_blob.size(), input_blob.data(), width, height, channels, depth, &s, &status);

		if (status != ako::Status::Ok)
		{
			std::cout << "Ako error: '" << ako::ToString(status) << "'\n";
			return EXIT_FAILURE;
		}

		if (quiet == false && verbose == true)
			PrintSettings(s, "decoder-side");

		input_blob.resize(1);

		// Benchmark
		if (quiet == false && benchmark == true)
		{
			std::cout << "Benchmark:\n";
			std::cout << " - Format: " << static_cast<double>(callbacks_data.format_duration.count()) / 1000.0
			          << " ms\n";
		}
	}

	// Checksum image
	uint32_t hash = 0;
	if (checksum == true)
	{
		hash = ako::Adler32(input_image, (width * height * channels * (depth / 8)));

		if (quiet == false && verbose == true)
			std::cout << "Input image hash: " << std::hex << hash << std::dec << " (Adler32)\n";
	}

	// Encode
	void* encoded_blob = nullptr;
	size_t encoded_blob_size = 0;
	if (output_filename != "")
	{
		if (quiet == false && verbose == true)
			std::cout << "Encoding PNG...\n";

		if ((encoded_blob_size = EncodePng(effort, width, height, channels, depth, input_image, &encoded_blob)) == 0)
			return EXIT_FAILURE;

		if (quiet == false && verbose == true)
			std::cout << "Encoded blob size: " << encoded_blob_size << " byte(s)\n";

		ako::DefaultCallbacks().free(input_image);
	}

	// Write output file
	if (output_filename != "")
	{
		if (quiet == false && verbose == true)
			std::cout << "Output file name: '" << output_filename << "'\n";

		if (WriteFile(output_filename, encoded_blob_size, encoded_blob) != 0)
			return EXIT_FAILURE;

		free(encoded_blob);
	}

	// Bye!
	if (quiet == false)
	{
		const auto uncompressed_size = static_cast<float>(width * height * channels) / 1000.0F;
		const auto compressed_size = static_cast<float>(input_blob_size) / 1000.0F;
		const auto bpp = (static_cast<float>(input_blob_size) / static_cast<float>(width * height * channels)) * 8.0F;

		if (checksum == true)
			printf("(%x) %.2f kB <- %.2f kB, ratio: %.2f:1, %.4f bpp\n", hash, uncompressed_size, compressed_size,
			       uncompressed_size / compressed_size, bpp);
		else
			printf("%.2f kB <- %.2f kB, ratio: %.2f:1, %.4f bpp\n", uncompressed_size, compressed_size,
			       uncompressed_size / compressed_size, bpp);
	}

	return EXIT_SUCCESS;
}
