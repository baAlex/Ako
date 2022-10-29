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
#include <cstdint>
#include <cstdio>
#include <stdexcept>


template <typename T> inline constexpr size_t MaxSymbols()
{
	return (1 << (sizeof(T) * 8));
}


template <typename T> inline T NearPowerOfTwo(const T x)
{
	T y = 2;
	while (y < x)
		y = y << 1;
	return y;
}


template <typename T> struct CdfEntry
{
	T symbol;
	uint32_t frequency;
	uint32_t cumulative;
};


template <typename T> class Cdf
{
	// Looks weird, but think it as an horizontal struct array.
	// Is for Simd optimizations and better cache-locality, as most frequent
	// operation on a Cdf is to iterate its values
	T symbols[MaxSymbols<T>()] = {};
	uint32_t frequencies[MaxSymbols<T>()] = {};
	uint32_t cumulatives[MaxSymbols<T>()] = {};

	// Like this:
	//	Struct CdfEntry {
	//		symbols
	//		frequencies
	//		cumulative }
	//	CdfEntry entries[]

	uint32_t length = 0;
	uint32_t maximum = 0;

	size_t QuicksortPartition(const size_t lo, const size_t hi)
	{
		const auto pivot = static_cast<size_t>(this->frequencies[(hi + lo) >> 1]);
		auto i = lo - 1;
		auto j = hi + 1;

		while (1)
		{
			// Move the left index to the right at least once and while the element at
			// the left index is less than the pivot
			do
			{
				i = i + 1;
			} while (this->frequencies[i] > pivot);

			// Move the right index to the left at least once and while the element at
			// the right index is greater than the pivot
			do
			{
				j = j - 1;
			} while (this->frequencies[j] < pivot);

			// If the indices crossed, return
			if (i >= j)
				break;

			// Swap the elements at the left and right indices
			std::swap(this->symbols[i], this->symbols[j]);
			std::swap(this->frequencies[i], this->frequencies[j]);
		}

		return j;
	}

	void Quicksort(const size_t lo, const size_t hi)
	{
		if (lo >= 0 && hi >= 0 && lo < hi)
		{
			const auto p = QuicksortPartition(lo, hi);
			Quicksort(lo, p);
			Quicksort(p + 1, hi);
		}
	}

	void InternalGenerationPhase(const uint32_t unique_symbols, const size_t sort_up_to, const size_t data_len,
	                             const uint32_t downscale_to = 0)
	{
		if (downscale_to != 0 && downscale_to != NearPowerOfTwo(downscale_to))
			throw std::runtime_error("Sir, this is an Ans codec");

		// Sort frequencies
		if (1)
			Quicksort(0, sort_up_to + 0);
		else
		{
			auto changes = true;
			for (size_t i = 0; i <= sort_up_to && changes == true; i += 1)
			{
				changes = false;
				for (size_t u = sort_up_to; u > i; u--)
				{
					if (this->frequencies[u] > this->frequencies[u - 1])
					{
						std::swap(this->symbols[u], this->symbols[u - 1]);
						std::swap(this->frequencies[u], this->frequencies[u - 1]);
						changes = true;
					}
				}
			}
		}

		// Accumulate frequencies, if needed, up-scaling them to nearest power of two
		// (an Ans requirement)
		uint32_t max_cumulative = 0;
		double scale = 1.0;
		{
			// A power of two data length means that, after accumulation, 'max_cumulative' will
			// be also a power of two... in fact its: 'length = max_cumulative', so no need of
			// up-scaling. Btw, this is the best and average scenario.
			if (data_len == NearPowerOfTwo(data_len))
			{
				uint32_t cumulative = 0;
				for (uint32_t i = 0; i < unique_symbols; i += 1)
				{
					this->cumulatives[i] = cumulative;
					cumulative += this->frequencies[i];
				}

				max_cumulative = this->cumulatives[unique_symbols - 1] + 1;
			}

			// If 'max_cumulative' isn't going to be a power of two, we can force it by
			// up-scaling frequencies (as long data length isn't zero, as it defines the scale)
			else if (data_len != 0)
			{
				// Up-scale frequencies and accumulate
				scale = static_cast<double>(NearPowerOfTwo(data_len)) / static_cast<double>(data_len);
				uint32_t cumulative = 0;

				for (uint32_t i = 0; i < unique_symbols; i += 1)
				{
					const auto f = static_cast<double>(this->frequencies[i]);

					this->frequencies[i] = static_cast<uint32_t>(f * scale);
					this->cumulatives[i] = cumulative;
					cumulative += this->frequencies[i];
				}

				// Validate 'max_cumulative', maths assumptions may fail me
				max_cumulative = this->cumulatives[unique_symbols - 1] + 1;
				if (static_cast<size_t>(max_cumulative) >= NearPowerOfTwo(data_len))
					throw std::runtime_error("Round error 1");

				max_cumulative = static_cast<uint32_t>(NearPowerOfTwo(data_len));
			}

			// Ops, frequencies doesn't even come from actual data!, we need to improvise
			// (the assumption of 'data_len' doesn't hold any more)
			else
			{
				// Discover 'max_cumulative' by accumulating as normal, but without storing anything
				uint32_t cumulative = 0;
				for (uint32_t i = 0; i < unique_symbols; i += 1)
				{
					cumulative += this->frequencies[i];
					max_cumulative = cumulative + 1;
				}

				// Up-scale frequencies and accumulate, scale defined by an ideal 'max_cumulative'
				scale = static_cast<double>(NearPowerOfTwo(max_cumulative)) / static_cast<double>(max_cumulative);
				cumulative = 0;

				for (uint32_t i = 0; i < unique_symbols; i += 1)
				{
					const auto f = static_cast<double>(this->frequencies[i]);

					this->frequencies[i] = static_cast<uint32_t>(f * scale);
					this->cumulatives[i] = cumulative;
					cumulative += this->frequencies[i];
				}

				// Validate 'max_cumulative', maths assumptions may fail me
				if (NearPowerOfTwo(max_cumulative) <= this->cumulatives[unique_symbols - 1] + 1)
					throw std::runtime_error("Round error 2");

				max_cumulative = NearPowerOfTwo(max_cumulative);
			}
		}

		// Downscale/quantization, we are doing it for transmission purposes
		if (downscale_to != 0 && max_cumulative > downscale_to)
		{
			// Count non-one values, those that we are going to scale down
			// (as scaling ones, means ending up with zeros, something that we do not want)
			uint32_t initial_max_cumulative = max_cumulative;
			uint32_t non_ones = 0;
			uint32_t cumulative = 0;

			for (uint32_t i = 0; i < unique_symbols && this->frequencies[i] != 1; i += 1)
			{
				cumulative += this->frequencies[i];
				non_ones += 1;
			}

			// Down-scale frequencies and accumulate
			scale = static_cast<double>(downscale_to - (unique_symbols - non_ones)) / static_cast<double>(cumulative);
			cumulative = 0;

			for (uint32_t i = 0; i < non_ones; i += 1)
			{
				const auto f = static_cast<double>(this->frequencies[i]);

				this->frequencies[i] = static_cast<uint32_t>(f * scale);
				if (this->frequencies[i] == 0)
					this->frequencies[i] = 1; // Aaahhh!!!!, we fix it later

				this->cumulatives[i] = cumulative;
				cumulative += this->frequencies[i];

				// Developers, developers, developers
				// printf("# %u, %u\n", this->frequencies[i], this->cumulatives[i]);
			}

			for (uint32_t i = non_ones; i < unique_symbols; i += 1)
			{
				this->cumulatives[i] = cumulative;
				cumulative += this->frequencies[i];

				// Developers, developers, developers
				// printf("# %u, %u\n", this->frequencies[i], this->cumulatives[i]);
			}

			// Things to fix!, we simply subtract one from those values larger to... one,
			// until the maximum cumulative match our downscale value
			if (this->cumulatives[unique_symbols - 1] + 1 > downscale_to)
			{
				auto to_fix = this->cumulatives[unique_symbols - 1] + 1 - downscale_to;

				while (to_fix != 0) // Not great, but math-y solutions are hard
				{
					auto fixed = 0;
					for (uint32_t i = non_ones - 1; i < non_ones && to_fix != 0; i -= 1) // Underflows
					{
						if (this->frequencies[i] > 1)
						{
							this->frequencies[i] -= 1;
							to_fix -= 1;
							fixed += 1;
						}
					}

					if (fixed == 0)
						throw std::runtime_error("Logic here needs some revision");
				}

				// Accumulate again!
				cumulative = 0;
				for (uint32_t i = 0; i < unique_symbols; i += 1)
				{
					this->cumulatives[i] = cumulative;
					cumulative += this->frequencies[i];
				}

				max_cumulative = this->cumulatives[unique_symbols - 1] + 1;
			}

			// Validate 'max_cumulative', the 'fix' procedure may be wrong
			if (this->cumulatives[unique_symbols - 1] + 1 >= downscale_to)
				throw std::runtime_error("Round error 3");

			max_cumulative = downscale_to;

			// And for printing purposes
			scale = static_cast<double>(downscale_to) / static_cast<double>(initial_max_cumulative);
		}

		// Developers, developers, developers
		if (0)
		{
			printf("\n# Cdf (scale: %.4fx):\n", scale);

			for (uint32_t i = 0; i < unique_symbols; i += 1)
			{
				const auto cdf_s = this->symbols[i];
				const auto cdf_f = this->frequencies[i];
				const auto cdf_c = this->cumulatives[i];

				printf(" - Symbol %u", i);

				if (sizeof(T) == 1)
				{
					if (cdf_s >= '!' && cdf_s <= '~')
						printf(" (%c), ", cdf_s);
					else
						printf(" (%02X),", cdf_s);
				}
				else
					printf(" (%u),", cdf_s);

				printf(" frqnc/cmltv: [%u, %u]\n", cdf_f, cdf_c);
			}

			printf(" - Max cumulative: %u\n", max_cumulative);
		}

		// Bye!
		this->length = unique_symbols;
		this->maximum = max_cumulative;
	}

  public:
	// clang-format off
	uint32_t Length() const  { return this->length; };
	uint32_t Maximum() const { return this->maximum; };
	// clang-format on

	Cdf()
	{
		static_assert(sizeof(T) <= 2, "No!!!, think on your heap.");
	}

	void GenerateFromData(const size_t data_len, const T* data, const uint32_t downscale_to = 0)
	{
		if (data_len == 0 || data == NULL)
			throw std::runtime_error("Weird arguments");
		if (data_len > UINT32_MAX)
			throw std::runtime_error("We only have 32 bits");

		memset(this->frequencies, 0, sizeof(uint32_t) * MaxSymbols<T>());

		// Count symbols
		uint32_t unique_symbols = 0;
		size_t largest_symbol = 0;

		for (size_t i = 0; i < data_len; i += 1)
		{
			const auto symbol = data[i];
			largest_symbol = std::max(largest_symbol, static_cast<size_t>(symbol));

			this->frequencies[symbol] += 1;
			if (this->frequencies[symbol] == 1)
			{
				this->symbols[symbol] = symbol;
				unique_symbols += 1;
			}
		}

		//
		InternalGenerationPhase(unique_symbols, largest_symbol, data_len, downscale_to);
	}

	void GenerateFromFrequencies(const size_t len, const uint32_t* f, const T* s, const uint32_t downscale_to = 0)
	{
		if (len == 0 || f == NULL || s == NULL)
			throw std::runtime_error("Weird arguments");
		if (len > MaxSymbols<T>())
			throw std::runtime_error("Cdf template-type doesn't matches");
		if (len > UINT32_MAX)
			throw std::runtime_error("We only have 32 bits");

		memset(this->frequencies, 0, sizeof(uint32_t) * MaxSymbols<T>());

		// Copy values
		uint32_t unique_symbols = 0;

		for (size_t i = 0; i < len; i += 1)
		{
			this->frequencies[i] = f[i];
			this->symbols[i] = s[i];

			if (this->frequencies[i] != 0)
				unique_symbols += 1;
		}

		//
		InternalGenerationPhase(unique_symbols, len, 0, downscale_to);
	}

	int OfSymbol(const T s, CdfEntry<T>& out) const
	{
		for (uint32_t i = 0; i < this->length; i += 1)
		{
			if (this->symbols[i] == s)
			{
				out.symbol = this->symbols[i];
				out.frequency = this->frequencies[i];
				out.cumulative = this->cumulatives[i];
				return 0;
			}
		}

		return 1;
	}

	CdfEntry<T> OfIndex(const uint32_t i) const
	{
		CdfEntry<T> out = {};

		if (i >= this->length)
			throw std::runtime_error("Index out of bounds");

		out.symbol = this->symbols[i];
		out.frequency = this->frequencies[i];
		out.cumulative = this->cumulatives[i];
		return out;
	}
};

#endif
