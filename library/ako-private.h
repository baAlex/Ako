
#ifndef AKO_PRIVATE_H
#define AKO_PRIVATE_H

#include "ako.h"


#if (AKO_FREESTANDING == 0)
#include <stdio.h>
#include <stdlib.h>
#define AKO_DEV_PRINTF(...) printf(__VA_ARGS__)
#else
// clang-format off
#define AKO_DEV_PRINTF(...) {} // Whitespace
// clang-format on
#endif


#define AKO_EXPORT __attribute__((visibility("default")))


struct akoTileHead
{
	uint16_t unused;
};

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

// misc.c:

size_t akoDividePlusOneRule(size_t x);
size_t akoTileDataSize(size_t tile_w, size_t tile_h);
size_t akoTileDimension(size_t x, size_t image_w, size_t tiles_dimension);

size_t akoImageMaxTileDataSize(size_t image_w, size_t image_h, size_t tiles_dimension);
size_t akoImageTilesNo(size_t image_w, size_t image_h, size_t tiles_dimension);

#endif
