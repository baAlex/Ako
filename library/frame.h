/*-----------------------------

 [frame.h]
 - Alexander Brandt 2021
-----------------------------*/

#ifndef FRAME_H
#define FRAME_H

#include <stdlib.h>

void FrameWrite(size_t width, size_t height, size_t channels, size_t tiles_dimension, void* output);
void FrameRead(const void* input, size_t input_size, size_t* out_width, size_t* out_height, size_t* out_channels,
               size_t* out_tiles_dimensions);

#endif
