
#ifndef AKO_PRIVATE_H
#define AKO_PRIVATE_H

#include "ako.h"


// format.c:

void akoFormatToPlanarI16Yuv(int keep_transparent_pixels, enum akoColorspace, size_t channels, size_t width,
                             size_t height, size_t in_stride, size_t out_planes_spacing, const uint8_t* in,
                             int16_t* out);
void akoFormatToInterleavedU8Rgb(enum akoColorspace, size_t channels, size_t width, size_t height,
                                 size_t in_planes_spacing, size_t out_stride, int16_t* in,
                                 uint8_t* out); // Destroys 'in'

#endif
