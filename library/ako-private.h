
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


struct akoLiftHead
{
	uint16_t quantization;
};


// developer.c:

void akoSavePgmI16(size_t width, size_t height, size_t in_stride, const int16_t* in, const char* filename);

// format.c:

void akoFormatToPlanarI16Yuv(int keep_transparent_pixels, enum akoColorspace, size_t channels, size_t width,
                             size_t height, size_t in_stride, size_t out_planes_spacing, const uint8_t* in,
                             int16_t* out);
void akoFormatToInterleavedU8Rgb(enum akoColorspace, size_t channels, size_t width, size_t height,
                                 size_t in_planes_spacing, size_t out_stride, int16_t* in,
                                 uint8_t* out); // Destroys 'in'

// head.c:

enum akoStatus akoHeadWrite(size_t channels, size_t image_w, size_t image_h, const struct akoSettings*, void* out);
enum akoStatus akoHeadRead(const void* in, size_t* out_channels, size_t* out_image_w, size_t* out_image_h,
                           struct akoSettings* out_s);

// kagari.c

#define AKO_ELIAS_ACCUMULATOR_LEN 64 // In bits
#define AKO_ELIAS_MAX 65535
#define AKO_ELIAS_MIN 1

struct akoEliasStateE
{
	uint8_t* out_end;
	uint8_t* out_cursor;

	uint64_t accumulator;
	int accumulator_usage;
};

struct akoEliasStateD
{
	const uint8_t* input_end;
	const uint8_t* input_cursor;

	uint64_t accumulator;
	int accumulator_usage;
};

struct akoEliasStateE akoEliasEncodeSet(void* buffer, size_t size);
struct akoEliasStateD akoEliasDecodeSet(const void* buffer, size_t size);

int akoEliasEncodeStep(struct akoEliasStateE* s, uint16_t v);
size_t akoEliasEncodeEnd(struct akoEliasStateE* s, void* out_start);

int akoEliasDecodeStep(struct akoEliasStateD* s, uint16_t* v_out);

size_t akoKagariEncode(const void* input, size_t input_size, void* output, size_t output_size);
size_t akoKagariDecode(size_t no, const void* input, size_t input_size, void* output, size_t output_size);

// lifting.c

void akoLift(size_t tile_no, const struct akoSettings*, size_t channels, size_t tile_w, size_t tile_h,
             size_t planes_space, int16_t* in, int16_t* output);
void akoUnlift(size_t tile_no, const struct akoSettings* s, size_t channels, size_t tile_w, size_t tile_h,
               size_t planes_space, int16_t* input, int16_t* out);

// misc.c:

size_t akoDividePlusOneRule(size_t x);
size_t akoPlanesSpacing(size_t tile_w, size_t tile_h);

size_t akoTileDataSize(size_t tile_w, size_t tile_h);
size_t akoTileDimension(size_t tile_pos, size_t image_d, size_t tiles_dimension);

size_t akoImageTilesNo(size_t image_w, size_t image_h, size_t tiles_dimension);
size_t akoImageMaxTileDataSize(size_t image_w, size_t image_h, size_t tiles_dimension);
size_t akoImageMaxPlanesSpacingSize(size_t image_w, size_t image_h, size_t tiles_dimension);

// wavelet-haar.c:

void akoHaarLift(enum akoWrap wrap, size_t in_stride, size_t current_w, size_t current_h, size_t target_w,
                 size_t target_h, int16_t* lp, int16_t* aux);
void akoHaarUnlift(enum akoWrap wrap, size_t current_w, size_t current_h, size_t target_w, size_t target_h, int16_t* lp,
                   int16_t* hps, int16_t* aux);

#endif
