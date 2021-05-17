/*-----------------------------

 [entropy.h]
 - Alexander Brandt 2021
-----------------------------*/

#ifndef ENTROPY_H
#define ENTROPY_H

#include <stdlib.h>

size_t EntropyCompress(size_t input_length, void** aux_buffer, int16_t* inout);
void EntropyDecompress(size_t input_size, size_t output_length, const void* in, int16_t* out);

#endif
