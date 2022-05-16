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


// rANS WITH NORMALIZATION
// =======================

// > clang++ ans2-normalization.cpp -std=c++20 -Wall -Wextra -Wconversion -pedantic -Werror -o ans
// > clang-tidy -checks="-*,cppcoreguidelines-*,bugprone-*,clang-analyzer-*,cert-*,misc-*,performance-*,portability-*"
// ans2-normalization.cpp


#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#include "ans-cdf.hpp"


typedef uint64_t acc_encode_t; // To avoid overflow at ((2 ^ 32) - 1)
typedef uint32_t acc_decode_t;

const size_t ACC_INITIAL_STATE = 123; // Arbitrary number
const size_t MESSAGE_MAX_PRINT = 40;  // Dev purposes

const size_t B = 1 << 16; // Output/input base, Duda's paper uses base 2 as an example (to extract individual bits),
                          // also suggests (1 << 8), and another alternative is to extract the exact number of bits for
                          // each symbol (through requires some math compromises... TODO(?)). This choice here is to
                          // reduce normalizations by output/input(-ing) 16 bits.

const size_t L =
    1 << 16; // Must be divisible by 'm' and for now 'm' is hardcoded to (1 << 16) [1], also we should aim to the fewest
             // normalizations ('L' works in conjunction with 'B'), and finally be a number that accumulators, after all
             // maths involved, can hold... kinda, this value here fits but we are unable to do a comparision [2] (we
             // can fix it with an intrinsic or assembly, for now a 64 bits accumulator at encoding do the trick).


acc_encode_t C(acc_encode_t accumulator, uint32_t frequency, uint32_t cumulative, uint32_t m)
{
	return m * (accumulator / frequency) + (accumulator % frequency) + cumulative;
}

acc_decode_t D(acc_decode_t accumulator, uint32_t frequency, uint32_t cumulative, uint32_t m, uint32_t modulo_point)
{
	return frequency * (accumulator / m) + modulo_point - cumulative;
}


template <typename InputT, typename OutputT>
std::unique_ptr<std::vector<OutputT>> AnsEncode(const Cdf<InputT>& cdf, const std::vector<InputT>& message)
{
	std::cout << "\nEncode:\n";

	auto output = std::make_unique<std::vector<OutputT>>();
	output->reserve(message.size());

	acc_encode_t acc = ACC_INITIAL_STATE;

	size_t normalizations = 0;       // For stats
	size_t total_normalizations = 0; // Ditto

	// Main loop
	for (auto i = message.size() - 1; i < message.size(); i--) // Underflows, ANS operates in reverse
	{
		const CdfEntry<InputT>& e = cdf.of_symbol(message[i]);

		// Normalize (output)
		normalizations = 0;
		while (C(acc, e.frequency, e.cumulative, cdf.m()) > (L * B) - 1) // [2] see above
		{
			output->insert(output->begin(), 1, static_cast<OutputT>(acc % static_cast<acc_encode_t>(B)));
			acc = acc / static_cast<acc_encode_t>(B);

			normalizations++;
			total_normalizations++;
		}

		// Encode
		acc = C(acc, e.frequency, e.cumulative, cdf.m());

		// Developers, developers, developers
		if (i < MESSAGE_MAX_PRINT || i == message.size() - 1)
		{
			if (normalizations == 0)
				std::cout << " - '" << e.symbol << "' (f: " << e.frequency << ", c: " << e.cumulative << ")\t->\t"
				          << acc << "\n";
			else
			{
				if (normalizations == 1)
					std::cout << " - '" << e.symbol << "' (f: " << e.frequency << ", c: " << e.cumulative << ")\t->\t"
					          << acc << " (1 normalization, " << output->at(0) << ")\n";
				else
					std::cout << " - '" << e.symbol << "' (f: " << e.frequency << ", c: " << e.cumulative << ")\t->\t"
					          << acc << " (" << normalizations << " normalizations)\n";
			}
		}

		if (i > MESSAGE_MAX_PRINT && i == message.size() - 1)
			std::cout << " - (...)\n";
	}

	// Accumulator remainder (output)
	while (acc != 0)
	{
		output->insert(output->begin(), 1, static_cast<OutputT>(acc % static_cast<acc_encode_t>(B)));
		acc = acc / static_cast<acc_encode_t>(B);

		total_normalizations++;
		std::cout << " - acc remainder\t->\t" << acc << " (" << output->at(0) << ")\n";
	}

	// Bye!
	std::cout << "Total normalizations: " << total_normalizations << "\n";
	std::cout << "Bitstream size: " << total_normalizations * sizeof(OutputT) << " bytes\n";
	return output;
}


template <typename OutputT, typename InputT>
std::unique_ptr<std::vector<OutputT>> AnsDecode(const Cdf<OutputT>& cdf, const std::vector<InputT>& bitcode,
                                                size_t message_len)
{
	std::cout << "\nDecode:\n";

	auto output = std::make_unique<std::vector<OutputT>>();
	output->reserve(message_len);

	acc_decode_t acc = 0;
	size_t input_i = 0;

	size_t normalizations = 0;       // For stats
	size_t total_normalizations = 0; // Ditto

	// Accumulator initialization (input)
	while (acc < L && input_i != bitcode.size())
	{
		acc = acc * static_cast<acc_decode_t>(B) + bitcode[input_i];
		input_i++;

		total_normalizations++;
		std::cout << " - acc initialization\t->\t" << acc << " (" << bitcode[input_i - 1] << ")\n";
	}

	// Main loop
	for (size_t i = 0; i < message_len; i++)
	{
		// Decode
		const uint32_t modulo_point = (acc % cdf.m());

		const CdfEntry<OutputT>& e = cdf.of_point(modulo_point);
		acc = D(acc, e.frequency, e.cumulative, cdf.m(), modulo_point);

		output->emplace_back(e.symbol);

		// Normalize (input)
		normalizations = 0;
		while (acc < L && input_i != bitcode.size())
		{
			acc = acc * static_cast<acc_decode_t>(B) + bitcode[input_i];
			input_i++;

			normalizations++;
			total_normalizations++;
		}

		// Developers, developers, developers
		if (i == message_len - 1 && message_len > MESSAGE_MAX_PRINT)
			std::cout << " - (...)\n";

		if (i < MESSAGE_MAX_PRINT || i == message_len - 1)
		{
			if (normalizations == 0)
				std::cout << " - '" << e.symbol << "' (f: " << e.frequency << ", c: " << e.cumulative << ")\t->\t"
				          << acc << "\n";
			else
			{
				if (normalizations == 1)
					std::cout << " - '" << e.symbol << "' (f: " << e.frequency << ", c: " << e.cumulative << ")\t->\t"
					          << acc << " (1 normalization, " << bitcode[input_i - 1] << ")\n";
				else
					std::cout << " - '" << e.symbol << "' (f: " << e.frequency << ", c: " << e.cumulative << ")\t->\t"
					          << acc << " (" << normalizations << " normalizations)\n";
			}
		}
	}

	// Bye!
	std::cout << "Total normalizations: " << total_normalizations << "\n";
	std::cout << "Bitstream size: " << total_normalizations * sizeof(InputT) << " bytes\n";

	if (acc != ACC_INITIAL_STATE)
		throw std::runtime_error("Out of sync.");

	return output;
}


template <typename OutputT> std::unique_ptr<std::vector<OutputT>> ReadFile(const std::string& filename)
{
	const size_t READ_BLOCK_SIZE = 512 * 1024;

	auto data = std::make_unique<std::vector<OutputT>>();
	auto file = std::fstream(filename, std::ios::in | std::ios::binary);

	if (file.is_open() == false)
		throw std::runtime_error("Input error.");

	for (auto blocks = 0;; blocks++)
	{
		const auto append_at = data->size();

		data->resize(data->size() + READ_BLOCK_SIZE / sizeof(OutputT));
		file.read(reinterpret_cast<char*>(data->data()) + append_at * sizeof(OutputT), READ_BLOCK_SIZE);

		if (file.eof() == true)
		{
			data->resize(data->size() - (READ_BLOCK_SIZE - static_cast<size_t>(file.gcount())) / sizeof(OutputT));
			std::cout << "Input: '" << filename << "', " << data->size() << " symbols, read in " << blocks + 1
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
		const auto cdf = Cdf<uint8_t>(*data, 1 << 16); // [1]
		const auto bitstream = AnsEncode<uint8_t, uint16_t>(cdf, *data);
		const auto decoded_data = AnsDecode<uint8_t, uint16_t>(cdf, *bitstream, data->size());

		if (argc > 2)
			WriteFile(*decoded_data, argv[2]);
	}
	catch (const std::exception& e)
	{
		std::cout << e.what() << "\n";
		return 1;
	}

	return 0;
}
