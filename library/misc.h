/*-----------------------------

 [misc.h]
 - Alexander Brandt 2021
-----------------------------*/

#ifndef MISC_H
#define MISC_H

#include <stdlib.h>

size_t TilesNo(size_t image_w, size_t image_h, size_t tiles_dimension);

size_t TileTotalLifts(size_t tile_w, size_t tile_h);
size_t TileTotalLength(size_t tile_w, size_t tile_h);

size_t WorkareaLength(size_t image_w, size_t image_h, size_t tiles_dimension);

#endif
