/*

Released into the public domain, or under BSD Zero Clause License:

Copyright (c) 2022 Alexander Brandt

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
USE OR PERFORMANCE OF THIS SOFTWARE.
*/

// BIBLIOGRAPHY:

// - DUDA Jarek (2014). Asymmetric numeral systems: entropy coding combining speed of Huffman coding with compression
//   rate of arithmetic coding. Center for Science of Information, Purdue University. arXiv:1311.2540.
// - PASCO Richard Clark (1976). Source coding algorithms for fast data compression. Standford University.

// ----


// rANS WITH NORMALIZATION (NAIVE APPROACH FOR NOW)
// ================================================

// > clang++ ans2-normalization.cpp -std=c++20 -Wall -Wextra -Wconversion -pedantic -Werror -o ans
// > clang-tidy -checks="-*,cppcoreguidelines-*,bugprone-*,clang-analyzer-*,cert-*,misc-*,performance-*,portability-*"
// ans2-normalization.cpp


#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "ans-cdf.hpp"


typedef uint64_t acc_t;
const acc_t ACC_INITIAL_STATE = 123;
const acc_t ACCUMULATOR_MAX = 4294967295; // 32 bits in a 64 bits accumulator in order to check overflows

const size_t MESSAGE_MAX_PRINT = 40;


template <typename InputT, typename OutputT>
std::vector<OutputT> AnsEncode(const Cdf<InputT>& cdf, const std::vector<InputT>& message,
                               OutputT output_unit_max = ((1 << (sizeof(OutputT) * 8)) - 1))
{
	std::cout << "\nEncode:\n";

	auto output = std::vector<OutputT>();
	acc_t acc = ACC_INITIAL_STATE;

	size_t normalizations = 0;       // For stats
	size_t total_normalizations = 0; // Ditto

	// Main loop
	for (auto i = message.size() - 1; i < message.size(); i--) // Underflows
	{
		const CdfEntry<InputT>& e = cdf.of_symbol(message[i]);

		// Normalize
		normalizations = 0;
		while ((cdf.max_cumulative() * (acc / e.frequency) + (acc % e.frequency) + e.cumulative) > ACCUMULATOR_MAX)
		{
			output.insert(output.begin(), 1, static_cast<OutputT>(acc % output_unit_max));
			acc = acc / output_unit_max;

			normalizations++;
			total_normalizations++;
		}

		// Encode
		acc = cdf.max_cumulative() * (acc / e.frequency) + (acc % e.frequency) + e.cumulative;

		// Developers, developers, developers
		if (i < MESSAGE_MAX_PRINT)
		{
			if (normalizations == 0)
				std::cout << " - '" << e.symbol << "' (f: " << e.frequency << ", c: " << e.cumulative << ")\t->\t"
				          << acc << "\n";
			else
			{
				if (normalizations == 1)
					std::cout << " - '" << e.symbol << "' (f: " << e.frequency << ", c: " << e.cumulative << ")\t->\t"
					          << acc << " (1 normalization, " << output[0] << ")\n";
				else
					std::cout << " - '" << e.symbol << "' (f: " << e.frequency << ", c: " << e.cumulative << ")\t->\t"
					          << acc << " (" << normalizations << " normalizations)\n";
			}
		}
	}

	// Accumulator remainder
	while (acc != 0)
	{
		output.insert(output.begin(), 1, static_cast<OutputT>(acc % output_unit_max));
		acc = acc / output_unit_max;
		total_normalizations++;

		std::cout << " - acc remainder\t->\t" << acc << " (" << output[0] << ")\n";
	}

	// Bye!
	std::cout << "Total normalizations: " << total_normalizations << "\n";
	std::cout << "Bitstream size: " << total_normalizations * sizeof(OutputT) << " bytes\n";
	return output;
}


template <typename OutputT, typename InputT>
void AnsDecode(const Cdf<OutputT>& cdf, const std::vector<InputT>& bitcode, size_t message_len,
               InputT input_unit_max = ((1 << (sizeof(InputT) * 8)) - 1))
{
	std::cout << "\nDecode:\n";

	acc_t acc = 0;
	size_t input_i = 0;

	size_t normalizations = 0;       // For stats
	size_t total_normalizations = 0; // Ditto
	size_t i = 0;                    // Ditto

	// Accumulator initialization
	while (acc < input_unit_max && input_i != bitcode.size())
	{
		acc = acc * input_unit_max + bitcode[input_i];
		total_normalizations++;

		std::cout << " - acc initialization\t->\t" << acc << " (" << bitcode[input_i] << ")\n";
		input_i++;
	}

	// Main loop
	while (acc > ACC_INITIAL_STATE)
	{
		if (i == message_len)
		{
			std::cout << "Decoded " << i << " symbols, acc: " << acc << "\n";
			throw std::runtime_error("End never reached.");
			break;
		}

		// Decode
		const uint32_t point = (acc % cdf.max_cumulative());
		const CdfEntry<OutputT>& e = cdf.of_point(point);

		acc = e.frequency * (acc / cdf.max_cumulative()) + point - e.cumulative;

		// Normalize
		normalizations = 0;
		while (acc < input_unit_max && input_i != bitcode.size())
		{
			acc = acc * input_unit_max + bitcode[input_i];
			input_i++;

			normalizations++;
			total_normalizations++;
		}

		// Developers, developers, developers
		if (i++ < MESSAGE_MAX_PRINT)
		{
			if (normalizations == 0)
				std::cout << " - '" << e.symbol << "' (p: " << point << ", f: " << e.frequency
				          << ", c: " << e.cumulative << ")\t->\t" << acc << "\n";
			else
			{
				if (normalizations == 1)
					std::cout << " - '" << e.symbol << "' (p: " << point << ", f: " << e.frequency
					          << ", c: " << e.cumulative << ")\t->\t" << acc << " (1 normalization, "
					          << bitcode[input_i - 1] << ")\n";
				else
					std::cout << " - '" << e.symbol << "' (p: " << point << ", f: " << e.frequency
					          << ", c: " << e.cumulative << ")\t->\t" << acc << " (" << normalizations
					          << " normalizations)\n";
			}
		}
	}

	// Bye!
	std::cout << "Total normalizations: " << total_normalizations << "\n";
	std::cout << "Bitstream size: " << total_normalizations * sizeof(InputT) << " bytes\n";
}


template <typename OutputT> std::vector<OutputT> ReadFile(const std::string& filename)
{
	const size_t READ_BLOCK_SIZE = 512 * 1024;

	auto data = std::vector<OutputT>();
	auto file = std::fstream(filename, std::ios::in | std::ios::binary);

	if (file.is_open() == false)
		throw std::runtime_error("Input error.");

	for (auto blocks = 0;; blocks++)
	{
		const auto append_at = data.size();

		data.resize(data.size() + READ_BLOCK_SIZE / sizeof(OutputT));
		file.read(reinterpret_cast<char*>(data.data()) + append_at * sizeof(OutputT), READ_BLOCK_SIZE);

		if (file.eof() == true)
		{
			data.resize(data.size() - (READ_BLOCK_SIZE - static_cast<size_t>(file.gcount())) / sizeof(OutputT));
			std::cout << "Input: '" << filename << "', " << data.size() << " symbols, read in " << blocks + 1
			          << " blocks\n";
			break;
		}
	}

	file.close();
	return data;
}


template <typename InputT> void WriteFile(const std::vector<InputT>& bitstream, const std::string& filename)
{
	auto file = std::fstream(filename, std::ios::out | std::ios::binary);
	file.write(reinterpret_cast<const char*>(bitstream.data()), static_cast<long>(bitstream.size() * sizeof(InputT)));
	file.close();
}


int main(int argc, const char* argv[])
{
	std::cout << "rANS Test 2\n\n";

	if (argc < 2)
	{
		std::cout << "No enough arguments\n";
		return 0;
	}

	try
	{
		const auto data = ReadFile<uint8_t>(argv[1]);
		const auto cdf = Cdf<uint8_t>(data.data(), data.size(), ((1 << 16) - 1));
		const auto bitstream = AnsEncode<uint8_t, uint16_t>(cdf, data, ((1 << 16) - 1));

		if (argc > 3)
			WriteFile(bitstream, argv[2]);

		AnsDecode<uint8_t, uint16_t>(cdf, bitstream, data.size(), ((1 << 16) - 1));
	}
	catch (const std::exception& e)
	{
		std::cout << e.what() << "\n";
		return 1;
	}

	return 0;
}
