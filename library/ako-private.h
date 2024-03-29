
#ifndef AKO_PRIVATE_H
#define AKO_PRIVATE_H

#include "ako.h"


#if (AKO_FREESTANDING == 0)
#include <stdio.h>
#include <stdlib.h>
// #define AKO_DEV_PRINTF(...) printf(__VA_ARGS__)
#endif

// clang-format off
#ifndef AKO_DEV_PRINTF
#define AKO_DEV_PRINTF(...) {} // Whitespace
#endif
// clang-format on

#define AKO_DEV_NOISE 10


#define AKO_EXPORT __attribute__((visibility("default")))


typedef int16_t coeff_t;   // For future monomorphization...
typedef uint16_t ucoeff_t; // Ditto


struct akoLiftHead
{
	int16_t quantization;
};

// compression.c:

size_t akoCompress(enum akoCompression, size_t channels, size_t tile_w, size_t tile_h, coeff_t* input, void* output);
size_t akoDecompress(enum akoCompression, size_t decompressed_size, size_t output_size, const void* input,
                     void* output);

// developer.c:

void akoSavePgmI16(size_t width, size_t height, size_t in_stride, const int16_t* in, const char* filename);

// format.c:

void akoFormatToPlanarI16Yuv(int discard_non_visible, enum akoColor, size_t channels, size_t width, size_t height,
                             size_t in_stride, size_t out_planes_spacing, const uint8_t* in, int16_t* out);
void akoFormatToInterleavedU8Rgb(enum akoColor, size_t channels, size_t width, size_t height, size_t in_planes_spacing,
                                 size_t out_stride, int16_t* in,
                                 uint8_t* out); // Destroys 'in'

// head.c:

enum akoStatus akoHeadWrite(size_t channels, size_t image_w, size_t image_h, const struct akoSettings*, void* out);
enum akoStatus akoHeadRead(const void* in, size_t* out_channels, size_t* out_image_w, size_t* out_image_h,
                           struct akoSettings* out_s);

// kagari.c

#define AKO_ELIAS_ACCUMULATOR_LEN 64 // In bits
#define AKO_ELIAS_MAX 65535
#define AKO_ELIAS_MIN 1

struct akoEliasState
{
	uint64_t accumulator;
	int accumulator_usage;
};

int akoEliasEncodeStep(struct akoEliasState* s, uint16_t v, uint8_t** cursor, const uint8_t* end);
size_t akoEliasEncodeEnd(struct akoEliasState* s, uint8_t** cursor, const uint8_t* end, void* out_start);

uint16_t akoEliasDecodeStep(struct akoEliasState* s, const uint8_t** cursor, const uint8_t* end, int* out_bits);

size_t akoKagariEncode(size_t input_size, size_t output_size, const void* input, void* output);
size_t akoKagariDecode(size_t no, size_t input_size, size_t output_size, const void* input, void* output);

// lifting.c

void akoLift(size_t tile_no, const struct akoSettings*, size_t channels, size_t tile_w, size_t tile_h,
             size_t planes_space, int16_t* in, int16_t* output);
void akoUnlift(const struct akoSettings* s, size_t channels, size_t tile_no, size_t tile_w, size_t tile_h,
               size_t out_planes_space, coeff_t* input, coeff_t* out);

// misc.c:

size_t akoDividePlusOneRule(size_t x);
size_t akoPlanesSpacing(size_t tile_w, size_t tile_h);

size_t akoTileDataSize(size_t tile_w, size_t tile_h);
size_t akoTileDimension(size_t tile_pos, size_t image_d, size_t tiles_dimension);

size_t akoImageTilesNo(size_t image_w, size_t image_h, size_t tiles_dimension);
size_t akoImageMaxTileDataSize(size_t image_w, size_t image_h, size_t tiles_dimension);
size_t akoImageMaxPlanesSpacingSize(size_t image_w, size_t image_h, size_t tiles_dimension);

void* akoIterateLifts(const struct akoSettings* s, size_t channels, size_t tile_w, size_t tile_h, void* input,
                      void (*lp_callback)(const struct akoSettings*, size_t ch, size_t tile_w, size_t tile_h,
                                          size_t target_w, size_t target_h, coeff_t* input_lp, void* user_data),
                      void (*hp_callback)(const struct akoSettings*, size_t ch, size_t tile_w, size_t tile_h,
                                          const struct akoLiftHead*, size_t current_w, size_t current_h,
                                          size_t target_w, size_t target_h, coeff_t* aux, coeff_t* hp_c, coeff_t* hp_b,
                                          coeff_t* hp_d, void* user_data),
                      void* user_data);

// quantization.c:

int16_t akoGate(int factor, int factor_mul, size_t tile_w, size_t tile_h, size_t current_w, size_t current_h);
int16_t akoQuantization(int factor, int factor_mul, size_t tile_w, size_t tile_h, size_t current_w, size_t current_h);

// wavelet-cdf53.c:

void akoCdf53LiftH(enum akoWrap, size_t current_h, size_t target_w, size_t fake_last, size_t in_stride,
                   const int16_t* in, int16_t* out);
void akoCdf53LiftV(enum akoWrap, size_t target_w, size_t target_h, const int16_t* in, int16_t* out);

void akoCdf53UnliftH(enum akoWrap, size_t current_w, size_t current_h, size_t out_stride, size_t ignore_last,
                     const int16_t* in_lp, const int16_t* in_hp, int16_t* out);
void akoCdf53InPlaceishUnliftV(enum akoWrap, size_t current_w, size_t current_h, const int16_t* in_lp,
                               const int16_t* in_hp, int16_t* out_lp, int16_t* out_hp);

// wavelet-dd137.c:

void akoDd137LiftH(enum akoWrap, size_t current_h, size_t target_w, size_t fake_last, size_t in_stride,
                   const int16_t* in, int16_t* out);
void akoDd137LiftV(enum akoWrap, size_t target_w, size_t target_h, const int16_t* in, int16_t* out);

void akoDd137UnliftH(enum akoWrap, size_t current_w, size_t current_h, size_t out_stride, size_t ignore_last,
                     const int16_t* in_lp, const int16_t* in_hp, int16_t* out);
void akoDd137InPlaceishUnliftV(enum akoWrap, size_t current_w, size_t current_h, const int16_t* in_lp,
                               const int16_t* in_hp, int16_t* out_lp, int16_t* out_hp);

// wavelet-haar.c:

void akoHaarLiftH(size_t current_h, size_t target_w, size_t fake_last, size_t in_stride, const int16_t* in,
                  int16_t* out);
void akoHaarLiftV(size_t target_w, size_t target_h, const int16_t* in, int16_t* out);

void akoHaarUnliftH(size_t current_w, size_t current_h, size_t out_stride, size_t ignore_last, const int16_t* in_lp,
                    const int16_t* in_hp, int16_t* out);
void akoHaarInPlaceishUnliftV(size_t current_w, size_t current_h, const int16_t* in_lp, const int16_t* in_hp,
                              int16_t* out_even, int16_t* out_odd);
#endif
