/*-----------------------------

 [dwt.h]
 - Alexander Brandt 2021
-----------------------------*/

#ifndef DWT_H
#define DWT_H

#include <stdint.h>
#include <stdlib.h>

#include "ako.h"

void DwtTransform(const struct AkoSettings* s, size_t width, size_t height, size_t channels, size_t planes_space,
                  int16_t* aux_memory, int16_t* input,
                  int16_t* output); // Destroys 'input'
void InverseDwtTransform(size_t width, size_t height, size_t channels, size_t planes_space, int16_t* aux_memory,
                         int16_t* input,
                         int16_t* output); // Destroys 'input'

#endif
