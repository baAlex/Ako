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

#include "ako-private.hpp"

namespace ako
{

void* BetterRealloc(const Callbacks& callbacks, void* ptr, size_t new_size)
{
	void* prev_ptr = ptr;
	if ((ptr = callbacks.realloc(ptr, new_size)) == nullptr)
	{
		callbacks.free(prev_ptr);
		return nullptr;
	}

	return ptr;
}


void* Memset(void* output, int c, size_t size)
{
#if defined(__clang__) || defined(__GNUC__)
	__builtin_memset(output, c, size);
#else
	auto out = reinterpret_cast<uint8_t*>(output);
	for (; size != 0; size -= 1, out += 1)
		*out = static_cast<uint8_t>(c);
#endif

	return output;
}


void* Memcpy(void* output, const void* input, size_t size)
{
#if defined(__clang__) || defined(__GNUC__)
	__builtin_memcpy(output, input, size);
#else
	auto out = reinterpret_cast<uint8_t*>(output);
	auto in = reinterpret_cast<const uint8_t*>(input);
	for (; size != 0; size -= 1, out += 1, in += 1)
		*out = *in;
#endif

	return output;
}


template <typename T>
static void sMemcpy2d(size_t width, size_t height, size_t in_stride, size_t out_stride, const T* in, T* out)
{
	for (size_t row = 0; row < height; row += 1)
	{
		for (size_t col = 0; col < width; col += 1)
			out[col] = in[col];

		in += in_stride;
		out += out_stride;
	}
}

template <>
void Memcpy2d(size_t width, size_t height, size_t in_stride, size_t out_stride, const int16_t* in, int16_t* out)
{
	return sMemcpy2d(width, height, in_stride, out_stride, in, out);
}

template <>
void Memcpy2d(size_t width, size_t height, size_t in_stride, size_t out_stride, const int32_t* in, int32_t* out)
{
	return sMemcpy2d(width, height, in_stride, out_stride, in, out);
}


Endianness SystemEndianness()
{
	const int16_t i = 1;
	auto p = reinterpret_cast<const int8_t*>(&i);

	if (p[0] != 1)
		return Endianness::Big;

	return Endianness::Little;
}

template <> uint16_t EndiannessReverse(uint16_t value)
{
	const auto b1 = static_cast<uint16_t>(value & 0xFF);
	const auto b2 = static_cast<uint16_t>((value >> 8) & 0xFF);

	return static_cast<uint16_t>((b1 << 8) | b2);
}

template <> uint32_t EndiannessReverse(uint32_t value)
{
	const auto b1 = static_cast<uint32_t>(value & 0xFF);
	const auto b2 = static_cast<uint32_t>((value >> 8) & 0xFF);
	const auto b3 = static_cast<uint32_t>((value >> 16) & 0xFF);
	const auto b4 = static_cast<uint32_t>((value >> 24) & 0xFF);

	return static_cast<uint32_t>((b1 << 24) | (b2 << 16) | (b3 << 8) | b4);
}

template <> int16_t EndiannessReverse(int16_t value)
{
	const auto b1 = static_cast<int16_t>(value & 0xFF);
	const auto b2 = static_cast<int16_t>((value >> 8) & 0xFF);

	return static_cast<int16_t>((b1 << 8) | b2);
}

template <> int32_t EndiannessReverse(int32_t value)
{
	const auto b1 = static_cast<int32_t>(value & 0xFF);
	const auto b2 = static_cast<int32_t>((value >> 8) & 0xFF);
	const auto b3 = static_cast<int32_t>((value >> 16) & 0xFF);
	const auto b4 = static_cast<int32_t>((value >> 24) & 0xFF);

	return static_cast<int32_t>((b1 << 24) | (b2 << 16) | (b3 << 8) | b4);
}


template <> int16_t WrapSubtract(int16_t a, int16_t b)
{
	// This seems to be a non trivial problem: https://en.wikipedia.org/wiki/Modulo_operation
	// In any case following formulas are reversible, in a defined way, in C and C++ compilers:
	return static_cast<int16_t>((static_cast<int32_t>(a) - static_cast<int32_t>(b)) % 65536);

	// Also: https://bugzilla.mozilla.org/show_bug.cgi?id=1031653
}

template <> int16_t WrapAdd(int16_t a, int16_t b)
{
	return static_cast<int16_t>((static_cast<int32_t>(a) + static_cast<int32_t>(b)) % 65536);
}

template <> int32_t WrapSubtract(int32_t a, int32_t b)
{
	return static_cast<int32_t>((static_cast<int64_t>(a) - static_cast<int64_t>(b)) % 4294967296);
}

template <> int32_t WrapAdd(int32_t a, int32_t b)
{
	return static_cast<int32_t>((static_cast<int64_t>(a) + static_cast<int64_t>(b)) % 4294967296);
}


template <> int16_t SaturateToLower(int16_t v)
{
	return (Max<int16_t>(0, Min<int16_t>(v, 255)));
}

template <> int32_t SaturateToLower(int32_t v)
{
	return (Max<int32_t>(0, Min<int32_t>(v, 65535)));
}

} // namespace ako
