/*-----------------------------

 [entropy.h]
 - Alexander Brandt 2021
-----------------------------*/

#ifndef ENTROPY_H
#define ENTROPY_H

#include <stdlib.h>

size_t EntropyCompress(size_t input_length, const int16_t* in, void* out);
void EntropyDecompress(size_t input_size, size_t output_length, const void* in, int16_t* out);

#endif
