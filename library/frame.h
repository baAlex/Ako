/*-----------------------------

 [frame.h]
 - Alexander Brandt 2021
-----------------------------*/

#ifndef FRAME_H
#define FRAME_H

#include <stdlib.h>

void FrameWrite(size_t dimension, size_t channels, size_t compressed_data_size, void* output);
void FrameRead(const void* input, size_t* out_dimension, size_t* out_channels, size_t* out_compressed_data_size);

#endif
