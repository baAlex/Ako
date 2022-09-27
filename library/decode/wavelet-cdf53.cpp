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
static void sCdf53HorizontalInverse(unsigned height, unsigned lp_w, unsigned hp_w, unsigned out_stride,
                                    const T* lowpass, const T* highpass, T* output)
{
	(void)height;
	(void)lp_w;
	(void)hp_w;
	(void)out_stride;
	(void)lowpass;
	(void)highpass;
	(void)output;
}


template <typename T>
static void sCdf53InPlaceishVerticalInverse(unsigned width, unsigned lp_h, unsigned hp_h, const T* lowpass, T* highpass,
                                            T* out_lowpass)
{
	(void)width;
	(void)lp_h;
	(void)hp_h;
	(void)lowpass;
	(void)highpass;
	(void)out_lowpass;
}


template <>
void Cdf53HorizontalInverse(unsigned height, unsigned lp_w, unsigned hp_w, unsigned out_stride, const int16_t* lowpass,
                            const int16_t* highpass, int16_t* output)
{
	sCdf53HorizontalInverse(height, lp_w, hp_w, out_stride, lowpass, highpass, output);
}

template <>
void Cdf53InPlaceishVerticalInverse(unsigned width, unsigned lp_h, unsigned hp_h, const int16_t* lowpass,
                                    int16_t* highpass, int16_t* out_lowpass)
{
	sCdf53InPlaceishVerticalInverse(width, lp_h, hp_h, lowpass, highpass, out_lowpass);
}

template <>
void Cdf53HorizontalInverse(unsigned height, unsigned lp_w, unsigned hp_w, unsigned out_stride, const int32_t* lowpass,
                            const int32_t* highpass, int32_t* output)
{
	sCdf53HorizontalInverse(height, lp_w, hp_w, out_stride, lowpass, highpass, output);
}

template <>
void Cdf53InPlaceishVerticalInverse(unsigned width, unsigned lp_h, unsigned hp_h, const int32_t* lowpass,
                                    int32_t* highpass, int32_t* out_lowpass)
{
	sCdf53InPlaceishVerticalInverse(width, lp_h, hp_h, lowpass, highpass, out_lowpass);
}

} // namespace ako
