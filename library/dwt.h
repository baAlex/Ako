/*-----------------------------

 [dwt.h]
 - Alexander Brandt 2021
-----------------------------*/

#ifndef DWT_H
#define DWT_H

#include <stdint.h>
#include <stdlib.h>

void DwtTransform(size_t width, size_t height, size_t channels, void* aux_memory, const int16_t* input,
                  int16_t* output);
void InverseDwtTransform(size_t width, size_t height, size_t channels, void* aux_memory, const int16_t* input,
                         int16_t* output);

#endif
