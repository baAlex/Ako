/*-----------------------------

 [format.h]
 - Alexander Brandt 2021
-----------------------------*/

#ifndef FORMAT_H
#define FORMAT_H

#include <stdint.h>
#include <stdlib.h>

void FormatToPlanarI16YUV(size_t width, size_t height, size_t channels, size_t in_pitch, const uint8_t* in,
                          int16_t* out);
void FormatToInterlacedU8RGB(size_t width, size_t height, size_t channels, size_t out_pitch, int16_t* in,
                             uint8_t* out); // Destroys 'in'

#endif
