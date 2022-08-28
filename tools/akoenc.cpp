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

#include <iostream>

#include "ako.hpp"

#include "thirdparty/CLI11.hpp"
#include "thirdparty/lodepng.h"
#include "thirdparty/picosha2.h"


int main(int argc, const char* argv[])
{
	CLI::App app{"Ako Encoding Tool"};

	std::string input_filename = "";
	std::string output_filename = "";
	bool checksum = false;

	// Parse arguments
	{
		char version_string[256];
		snprintf(version_string, 256,
		         "Ako Encoding Tool\nv%i.%i.%i\n\nCopyright (c) 2021-2022 Alexander Brandt. \nDistribution terms "
		         "specified in LICENSE and THIRDPARTY files.\n",
		         ako::VersionMajor(), ako::VersionMinor(), ako::VersionPatch());

		app.add_option("Input", input_filename, "Input filename")->required();
		app.add_option("-o,--output", input_filename, "Output filename");
		app.add_flag("-c,--checksum", checksum, "Checksum input image");

		app.set_version_flag("-v,--version", version_string);
		CLI11_PARSE(app, argc, argv);
	}

	// Bye!
	return EXIT_SUCCESS;
}
