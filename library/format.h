/*-----------------------------

 [format.h]
 - Alexander Brandt 2021
-----------------------------*/

#ifndef FORMAT_H
#define FORMAT_H

#include <stdint.h>
#include <stdlib.h>

void FormatToPlanarI16YUV(size_t dimension, size_t channels, const uint8_t* in, int16_t* out);
void FormatToInterlacedU8RGB(size_t dimension, size_t channels, int16_t* in, uint8_t* out); // Destroys 'in'

#endif
