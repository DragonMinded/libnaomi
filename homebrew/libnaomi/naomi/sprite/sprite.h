#ifndef __SPRITE_H
#define __SPRITE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "naomi/color.h"

// Given a top left as well as bottom right x, y coordinate and a color to
// fill a box in, draw a solid box to the screen.
void sprite_draw_box(int x0, int y0, int x1, int y1, color_t color);

// Given an X and Y pixel location on the screen (based on the top left of the
// screen) and a previously loaded texture, draw that texture as a sprite with
// no scaling or rotation. The sprite is drawn with its upper left corner at
// the x, y coordinates requested. Note that this is monitor orientation aware.
void sprite_draw_simple(int x, int y, texture_description_t *texture);

// Given an Y and Y pixel location on the screen, an angle in degrees and a
// previously loaded texture, draw that texture as a sprite on the screen,
// rotated about its center. Note that if you give this function an angle of
// 0 it will display identically to sprite_draw_simple(). If you are
// working with square images and rotate at 90, 180 or 270, the bounding box
// of the sprite will be identical to if it was rotated at 0 degrees. The
// sprite is drawn at the position where if it was rotated at 0 degrees, the
// x, y coordinates requested would be the top left of the sprite.
void sprite_draw_rotated(int x, int y, float angle, texture_description_t *texture);

#ifdef __cplusplus
}
#endif

#endif
