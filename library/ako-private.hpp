/*

MIT License

Copyright (c) 2021-2023 Alexander Brandt

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

#ifndef AKO_PRIVATE_HPP
#define AKO_PRIVATE_HPP

#include "ako.hpp"

#ifndef AKO_FREESTANDING
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#endif

namespace ako
{

const uint32_t IMAGE_HEAD_MAGIC = 0x036F6B41; // "Ako-0x03"
const uint32_t TILE_HEAD_MAGIC = 0x03546B41;  // "AkT-0x03"

struct ImageHead
{
	// Little endian:
	uint32_t magic;
	uint32_t a; // Width (25), Depth (5), Color (2)
	uint32_t b; // Height (25), Tiles dimension (5), Wavelet (2)
	uint32_t c; // Channels (25), Wrap (2), Unused (5)
};

struct TileHead
{
	uint32_t magic;
	uint32_t no;
	uint32_t compressed_size;
	uint32_t tags; // Compression (2), Unused (30)
};

enum class Endianness
{
	Little,
	Big
};

template <typename T> using QuantizationCallback = void (*)(float, unsigned, const T*, T*);

template <typename T> class Compressor
{
  public:
	virtual int Step(QuantizationCallback<T>, float quantization_step, unsigned width, unsigned height,
	                 const T* in) = 0;
	virtual size_t Finish() = 0;
};

template <typename T> class Decompressor
{
  public:
	virtual Status Step(unsigned width, unsigned height, T* out) = 0;
};


// common/cdf-tables.cpp:

struct CdfEntry
{
	uint16_t root;
	uint16_t suffix_length;
	uint16_t frequency;
	uint16_t cumulative;
};

extern const uint32_t G_CDF1_LEN;
extern const CdfEntry g_cdf1[255 + 1];


// common/conversions.cpp:

uint32_t ToNumber(Color c);
uint32_t ToNumber(Wavelet w);
uint32_t ToNumber(Wrap w);
uint32_t ToNumber(Compression c);

Color ToColor(uint32_t number, Status& out_status);
Wavelet ToWavelet(uint32_t number, Status& out_status);
Wrap ToWrap(uint32_t number, Status& out_status);
Compression ToCompression(uint32_t number, Status& out_status);


// common/essentials.cpp:

unsigned Half(unsigned length);            // By convention yields highpass length
unsigned HalfPlusOneRule(unsigned length); // By convention yields lowpass length
unsigned NearPowerOfTwo(unsigned v);

unsigned LiftsNo(unsigned width, unsigned height);
void LiftMeasures(unsigned no, unsigned width, unsigned height, unsigned& out_lp_w, unsigned& out_lp_h,
                  unsigned& out_hp_w, unsigned& out_hp_h);

unsigned TilesNo(unsigned tiles_dimension, unsigned image_w, unsigned image_h);
void TileMeasures(unsigned tile_no, unsigned tiles_dimension, unsigned image_w, unsigned image_h, //
                  unsigned& out_w, unsigned& out_h, unsigned& out_x, unsigned& out_y);

template <typename T> size_t TileDataSize(unsigned width, unsigned height, unsigned channels);
template <typename T>
size_t WorkareaSize(unsigned tiles_dimension, unsigned image_w, unsigned image_h, unsigned channels);

Status ValidateCallbacks(const Callbacks& callbacks);
Status ValidateSettings(const Settings& settings);
Status ValidateProperties(unsigned image_w, unsigned image_h, unsigned channels, unsigned depth);
Status ValidateInput(const void* ptr, size_t input_size = 1); // TODO?


// common/utilities.cpp:

void* BetterRealloc(const Callbacks& callbacks, void* ptr, size_t new_size);

void* Memset(void* output, int c, size_t size);
void* Memcpy(void* output, const void* input, size_t size);

template <typename T>
void Memcpy2d(size_t width, size_t height, size_t in_stride, size_t out_stride, const T* in, T* out);

Endianness SystemEndianness();
template <typename T> T EndiannessReverse(T value);

template <typename T> T WrapSubtract(T a, T b);
template <typename T> T WrapAdd(T a, T b);

template <typename T> T SaturateToLower(T v);


// decode/ans.cpp:
// encode/ans.cpp:

class BitReader;
class BitWriter;

enum class AnsEncoderStatus
{
	Ok,
	Error,
	OutputTooShort,
	InputTooLong
};

const uint32_t ANS_STATE_LEN = 32;

const uint32_t ANS_B_LEN = 15;         // Output/input base, Duda's paper uses '1 << 0' as an example
const uint32_t ANS_B = 1 << ANS_B_LEN; // to extract individual bits, also suggests '1 << 8' for bytes.
const uint32_t ANS_B_MASK = ANS_B - 1; // A large number should reduce writes/reads. Here 15 bits is
                                       // the maximum before our 32 bits state wrap/overflows.

const uint32_t ANS_L = 1 << 16; // Needs to be multiple of the sum of frequencies in the Cdf table (also
                                // known as 'last cumulative' or M), and such number is hardcoded to
                                // '1 << 16'. So no choice here.

const uint32_t ANS_M_LEN = 16;         // So called M, Cdf tables are normalized to this number. It can
const uint32_t ANS_M = 1 << ANS_M_LEN; // be thought as 'precision' to represent frequencies
const uint32_t ANS_M_MASK = ANS_M - 1; // (in 0.0, 1.0 range) with integers (as a 0, M range).

const uint32_t ANS_INITIAL_STATE = ANS_L + 123; // 123 addition is arbitrary.

uint32_t AnsDecode(uint32_t input_length, uint32_t output_length, const uint32_t* input, uint16_t* output);
uint32_t AnsEncode(uint32_t input_length, const uint16_t* input, BitWriter& writer, AnsEncoderStatus& out_status);


// decode/bit.cpp:
// encode/bit.cpp:

class BitReader
{
	static const uint32_t ACCUMULATOR_LEN = 32;

	const uint32_t* m_input_end;
	const uint32_t* m_input;

	uint32_t m_accumulator;
	uint32_t m_accumulator_usage;

  public:
	BitReader(uint32_t input_length = 0, const uint32_t* input = nullptr);
	void Reset(uint32_t input_length, const uint32_t* input);
	int Read(uint32_t bit_length, uint32_t& value);
	uint32_t Finish(const uint32_t* input_start) const;
};

class BitWriter
{
	static const uint32_t ACCUMULATOR_LEN = 32;

	uint32_t* m_output_start;
	uint32_t* m_output_end;
	uint32_t* m_output;

	uint32_t m_wrote_values; // As is possible to encode zero, of length zero,
	                         // meaning that 'output' doesn't move actually.
	                         // So this work as an alternative to keep count.

	uint32_t m_accumulator;
	uint32_t m_accumulator_usage;

  public:
	BitWriter(uint32_t output_length = 0, uint32_t* output = nullptr);
	void Reset(uint32_t output_length, uint32_t* output);
	int Write(uint32_t value, uint32_t bit_length);
	uint32_t Finish();
};


// decode/compression.cpp:
// encode/compression.cpp:

template <typename T>
int Decompress(Compression compression, size_t compressed_size, unsigned width, unsigned height, unsigned channels,
               const void* input, T* output, Status&);

template <typename T>
size_t Compress(const Callbacks&, const Settings&, unsigned width, unsigned height, unsigned channels, const T* input,
                void* output, Compression& out_compression);


// decode/format.cpp:
// encode/format.cpp:

template <typename TIn, typename TOut>
void FormatToRgb(const Color& color_transformation, unsigned width, unsigned height, unsigned channels,
                 size_t output_stride, TIn* input, TOut* output); // Destroys input

template <typename TIn, typename TOut>
void FormatToInternal(const Color& color_transformation, bool discard, unsigned width, unsigned height,
                      unsigned channels, size_t input_stride, const TIn* input, TOut* output);


// decode/headers.cpp
// encode/header.cpp

Status TileHeadRead(const TileHead& head_raw, Compression& out_compression, size_t& out_compressed_size);
void TileHeadWrite(unsigned no, Compression compression, size_t compressed_size, TileHead& out);

void ImageHeadWrite(const Settings& settings, unsigned image_w, unsigned image_h, unsigned channels, unsigned depth,
                    ImageHead& out);


// decode/lifting.cpp:
// encode/lifting.cpp:

template <typename T>
void Unlift(const Callbacks&, const Wavelet& wavelet_transformation, unsigned width, unsigned height, unsigned channels,
            T* input, T* output);

template <typename T>
void Lift(const Callbacks&, const Wavelet& wavelet_transformation, unsigned width, unsigned height, unsigned channels,
          T* input, T* output);


// decode/wavelet-cdf53.cpp:
// encode/wavelet-cdf53.cpp:

template <typename T>
void Cdf53HorizontalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                            const T* input, T* output);

template <typename T>
void Cdf53VerticalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                          const T* input, T* output);

template <typename T>
void Cdf53HorizontalInverse(unsigned height, unsigned lp_w, unsigned hp_w, unsigned out_stride, const T* lowpass,
                            const T* highpass, T* output);

template <typename T>
void Cdf53InPlaceishVerticalInverse(unsigned width, unsigned lp_h, unsigned hp_h, const T* lowpass, T* highpass,
                                    T* out_lowpass);


// decode/wavelet-haar.cpp:
// encode/wavelet-haar.cpp:

template <typename T>
void HaarHorizontalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride,
                           const T* input, T* output);

template <typename T>
void HaarVerticalForward(unsigned width, unsigned height, unsigned input_stride, unsigned output_stride, const T* input,
                         T* output);

template <typename T>
void HaarHorizontalInverse(unsigned height, unsigned lp_w, unsigned hp_w, unsigned out_stride, const T* lowpass,
                           const T* highpass, T* output);

template <typename T>
void HaarInPlaceishVerticalInverse(unsigned width, unsigned lp_h, unsigned hp_h, const T* lowpass, T* highpass,
                                   T* out_lowpass);


// Misc:

template <typename T> T Min(T a, T b)
{
	return (a < b) ? a : b;
}

template <typename T> T Max(T a, T b)
{
	return (a > b) ? a : b;
}

inline GenericType GenericTypeDesignatedInitialization(int16_t value) // Humour
{
	GenericType ret;
	ret.s = static_cast<signed long>(value);
	return ret;
}

inline GenericType GenericTypeDesignatedInitialization(int32_t value)
{
	GenericType ret;
	ret.s = static_cast<signed long>(value);
	return ret;
}

inline GenericType GenericTypeDesignatedInitialization(float value)
{
	GenericType ret;
	ret.f = value;
	return ret;
}

} // namespace ako

#endif
