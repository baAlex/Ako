/*-----------------------------

 [dwt.h]
 - Alexander Brandt 2021
-----------------------------*/

#ifndef DWT_H
#define DWT_H

#include <stdint.h>
#include <stdlib.h>

struct DwtLiftSettings
{
	float detail_gate;
	size_t limit;
};

void DwtLiftPlane(const struct DwtLiftSettings* s, size_t dimension, void** aux_buffer, int16_t* inout);
void DwtPackImage(size_t dimension, size_t channels, int16_t* inout);

void DwtUnpackUnliftImage(size_t dimension, size_t channels, int16_t* aux_buffer, const int16_t* in, int16_t* out);

#endif
