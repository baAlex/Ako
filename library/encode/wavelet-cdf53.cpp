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

template <typename T>
static void sCdf53HorizontalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                                    const T* input, T* output)
{
	(void)width;
	(void)height;
	(void)input_stride;
	(void)output_stride;
	(void)input;
	(void)output;
}


template <typename T>
static void sCdf53VerticalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                                  const T* input, T* output)
{
	(void)width;
	(void)height;
	(void)input_stride;
	(void)output_stride;
	(void)input;
	(void)output;
}


template <>
void Cdf53HorizontalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                            const int16_t* input, int16_t* output)
{
	sCdf53HorizontalForward(width, height, input_stride, output_stride, input, output);
}

template <>
void Cdf53VerticalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                          const int16_t* input, int16_t* output)
{
	sCdf53VerticalForward(width, height, input_stride, output_stride, input, output);
}

template <>
void Cdf53HorizontalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                            const int32_t* input, int32_t* output)
{
	sCdf53HorizontalForward(width, height, input_stride, output_stride, input, output);
}

template <>
void Cdf53VerticalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                          const int32_t* input, int32_t* output)
{
	sCdf53VerticalForward(width, height, input_stride, output_stride, input, output);
}

} // namespace ako
