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

#ifndef CDF_HPP
#define CDF_HPP

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

#define DEVELOPERS_DEVELOPERS_DEVELOPERS
#ifdef DEVELOPERS_DEVELOPERS_DEVELOPERS
#include <iostream>
#endif


template <typename T> struct CdfEntry
{
	T symbol;
	uint32_t frequency;
	uint32_t cumulative;
};

template <typename T> class Cdf
{
  private:
	std::vector<CdfEntry<T>> table;
	uint32_t max_cumulative_;

	double entropy(const std::vector<CdfEntry<T>>& table, size_t len)
	{
		double h = 0.0;
		for (const auto& i : table)
		{
			const auto p = static_cast<double>(i.frequency) / static_cast<double>(len);
			h -= p * std::log2(p);
		}

		return h;
	}

  public:
	Cdf(const T* message, size_t len)
	{
		// Cumulative distribution function (CDF) following Pasco (1976, p.10)
		// (on my own nomenclature):

		// CDF(x) = Σ { P(y) }
		// With summation (Σ) while: 'y < x'

		// This is, the CDF of symbol 'x' is the summation of symbols 'y' probabilities (P)
		// as long the conditional 'y < x' is meet. Ambiguous comparision, but explained on
		// that the table/vector/set of symbols where we perform this CDF function, is
		// lexicographically ordered on symbols probabilities.

		// Side note: his formula using summation (Σ) and in companion of two paragraphs of
		// text is a lot more friendly than Wikipedia's CDF and ANS articles.
		// (en.wikipedia.org/wiki/Cumulative_distribution_function)
		// (en.wikipedia.org/wiki/Asymmetric_numeral_systems)

		// Yes, that explains why I'm using a 70's text.

		{
			// Count symbols
			auto hashmap = std::unordered_map<T, uint32_t>();
			for (const T* m = message; m != (message + len); m++)
				hashmap[*m] += 1;

			// Create sorted table
			for (const auto& i : hashmap)
			{
				const auto entry = CdfEntry<T>{i.first, i.second, 0};
				table.emplace_back(entry);
			}

			if (table.size() < 2)
				throw std::runtime_error("A CDF requires at least two symbols.");

			std::sort(table.begin(), table.end(),
			          [](CdfEntry<T> a, CdfEntry<T> b) { return (a.frequency > b.frequency); });
		}

		const auto h = entropy(table, len);

		// Accumulate frequencies
		uint32_t cumulative = 0;

		for (auto& i : table)
		{
			max_cumulative_ = cumulative + 1;
			i.cumulative = cumulative;
			cumulative += i.frequency;
		}

#ifdef DEVELOPERS_DEVELOPERS_DEVELOPERS
		const size_t MESSAGE_MAX_PRINT = 80;
		const size_t SYMBOLS_MAX_PRINT = 10;

		std::cout << "\nMessage: \"";

		for (const T* i = message; i != message + std::min(len, MESSAGE_MAX_PRINT); i++)
			std::cout << *i;

		std::cout << "\"\n";
		std::cout << "Length: " << len << "\n";
		std::cout << "Maximum cumulative: " << max_cumulative_ << "\n";
		std::cout << "Unique symbols: " << table.size() << "\n";
		std::cout << "Entropy: " << h << " bits per symbol\n";
		std::cout << "Shannon target: " << (h * static_cast<double>(len)) / 8.0 << " bytes\n";

		for (size_t i = 0; i < std::min(table.size(), SYMBOLS_MAX_PRINT); i++)
			std::cout << " - '" << table[i].symbol << "' (f: " << table[i].frequency << ", c: " << table[i].cumulative
			          << ")\n";

		if (table.size() > SYMBOLS_MAX_PRINT)
			std::cout << " - (...)\n";
#endif
	}

	const CdfEntry<T>& of_symbol(T s) const
	{
		for (auto& i : table)
		{
			if (i.symbol == s)
				return i;
		}

		throw std::runtime_error("No CDF for specified symbol.");
	}

	const CdfEntry<T>& of_point(uint32_t p) const
	{
		for (size_t i = 1; i < table.size(); i++)
		{
			if (p < table[i].cumulative)
				return table[i - 1];
		}

		return table[table.size() - 1];
	}

	uint32_t max_cumulative() const
	{
		return max_cumulative_;
	}
};
#undef DEVELOPERS_DEVELOPERS_DEVELOPERS
#endif
