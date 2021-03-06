#ifndef __SPRITE_H
#define __SPRITE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "naomi/color.h"

// Convenience functions for drawing sprites of various configurations using
// hardware, specifically the PowerVR. This can drastically speed up the
// render time of your game. There are, however, some caveats and limitations.
// For starters, textures must be preloaded into VRAM before they can be
// displayed. You can do that using ta_texture_desc_malloc_paletted() and
// ta_texture_desc_malloc_direct() if you have pre-converted sprites that
// are of the following dimensions: 8x8, 16x16, 32x32, 64x64, 128x128,
// 256x256, 512x512 or 1024x1024. If you have odd-sized sprites, you will
// have to pick a square size that's equal to or larger than the width and
// height of your sprite and then use ta_texture_malloc() followed by
// ta_texture_load_sprite() and finally ta_texture_desc_paletted() or
// ta_texture_desc_direct() to load it into VRAM. Note that you can make your
// life a lot easier by just sizing your sprite images up to the next allowed
// square size and setting all the new pixels to fully transparent. It will
// display exactly the same.
//
// Not all permutations of possible draw commands are represented here. If you
// need extra complicated display routines such as full affine transformations
// it is recommended to use ta commands directly. These functions are presented
// as an easy wrapper layer to the most common sprite routines and so more
// advanced functionality is left out.
//
// Note that in all cases, sprites are drawn back to front in the order submitted.
// So, if you want a sprite to overlap another, be careful to draw the one which
// is to be behind first, and the one which is to be in front second.

// Given a top left as well as bottom right x, y coordinate and a color to
// fill a box in, draw a solid box to the screen. Note that this is monitor
// orientation aware.
void sprite_draw_box(int x0, int y0, int x1, int y1, color_t color);

// Given an X and Y pixel location on the screen (based on the top left of the
// screen) and a previously loaded texture, draw that texture as a sprite with
// no scaling or rotation. The sprite is drawn with its upper left corner at
// the x, y coordinates requested. Note that this is monitor orientation aware.
void sprite_draw_simple(int x, int y, texture_description_t *texture);

// Given an X and Y pixel location on the screen, an angle in degrees and a
// previously loaded texture, draw that texture as a sprite on the screen,
// rotated about its center. Note that if you give this function an angle of
// 0 it will display identically to sprite_draw_simple(). If you are
// working with square images and rotate at 90, 180 or 270, the bounding box
// of the sprite will be identical to if it was rotated at 0 degrees. The
// sprite is drawn at the position where if it was rotated at 0 degrees, the
// x, y coordinates requested would be the top left of the sprite. Note that
// this is monitor orientation aware.
void sprite_draw_rotated(int x, int y, float angle, texture_description_t *texture);

// Given an X any Y pixel location on the screen as well as an x and y scaling
// factor, draw that texture as a sprite scaled horizontally and vertically
// according to the scale factors. The sprite is drawn with its upper corner
// at the x, y coordinates requested, so the scaling happens downward and
// rightward. Note that you can use a negative scaling value to flip the sprite
// in the x or y axis. The sprite is still drawn in the same location but the
// pixels will be mirrored as requested. Note that this is monitor orientation
// aware.
void sprite_draw_scaled(int x, int y, float xscale, float yscale, texture_description_t *texture);

// Given an X any Y pixel location on the screen as well as an x and y scaling
// factor, draw that texture as a sprite scaled horizontally and vertically
// according to the scale factors and rotated about its center according to
// the angle in degrees. The sprite is drawn at a position where if it was
// rotated 0 dgrees, the upper corner of the sprite would lay at the x, y
// coordinates requested. Note that you can use a negative scaling value to flip
// the sprite in the x or y axis. The sprite is still drawn in the same location
// but the pixels will be mirrored as requested. Note that this is monitor
// orientation aware.
void sprite_draw_scaled_rotated(int x, int y, float xscale, float yscale, float angle, texture_description_t *texture);

// Given an X and Y pixel location on the screen (based on the top left of the
// screen), a sprite width and height and a previously loaded texture, draw that
// texture as a sprite with no scaling or rotation. The sprite is drawn with its
// upper left corner at the x, y coordinates requested. This is most useful when
// combined with ta_texture_load_sprite(). Note that this is monitor orientation
// aware.
void sprite_draw_nonsquare(int x, int y, int width, int height, texture_description_t *texture);

// Given an X and Y pixel location on the screen, a sprite width and height,
// an angle in degrees and a previously loaded texture, draw that texture as
// a sprite on the screen, rotated about its center. The sprite is drawn at
// the position where if it was rotated at 0 degrees, the x, y coordinates
// requested would be the top left of the sprite. Note that this is monitor
// orientation aware.
void sprite_draw_nonsquare_rotated(int x, int y, int width, int height, float angle, texture_description_t *texture);

// Given an X and Y pixel location on the screen (based on the top left of the
// screen), a sprite width and height, an x any scaling factor and a previously
// loaded texture, draw that texture as a sprite with the requested scaling but
// no rotation. The sprite is drawn with its upper left corner at the x, y
// coordinates requested and the scaling happens down and to the right. This is
// most useful when combined with ta_texture_load_sprite(). Note that you can
// give this function negative scaling values to flip the texture in the x or
// y orientation. Note also that this is monitor orientation aware.
void sprite_draw_nonsquare_scaled(int x, int y, int width, int height, float xscale, float yscale, texture_description_t *texture);

// Given an X and Y pixel location on the screen (based on the top left of the
// screen), a sprite width and height, an x any scaling factor and a previously
// loaded texture, draw that texture as a sprite with the requested scaling and
// rotation in degrees. The sprite is drawn at the position where if it was
// rotated 0 degrees, its upper left corner would be at the x, y coordinates
// requested. This is most useful when combined with ta_texture_load_sprite().
// Note that you can give this function negative scaling values to flip the
// texture in the x or y orientation. Note also that this is monitor orientation
// aware.
void sprite_draw_nonsquare_scaled_rotated(int x, int y, int width, int height, float xscale, float yscale, float angle, texture_description_t *texture);

// Given an X and Y pixel location on the screen, the size of each tile entry
// in the tilemap in pixels (both directions, so the tile must be square) and
// which tile to draw (starting with the top left as tile 0, increasing left
// to right and then top to bottom), draw that sprite out of the tilemap. The
// sprite is drawn with the top left pixel at X, Y. Note that this is monitor
// orientation aware.
void sprite_draw_tilemap_entry(int x, int y, int tilesize, int which, texture_description_t *texture);

// Given an X and Y pixel location on the screen, the size of each tile entry
// in the tilemap in pixels (both directions, so the tile must be square) and
// which tile to draw (starting with the top left as tile 0, increasing left
// to right and then top to bottom), draw that sprite out of the tilemap,
// rotated about its center. The sprite is drawn in the position where if it
// was rotated 0 degrees, the x, y coordinates requested would be the top left
// of the sprite. Note that this is monitor orientation aware.
void sprite_draw_tilemap_entry_rotated(int x, int y, int tilesize, int which, float angle, texture_description_t *texture);

// Given an X and Y pixel location on the screen, the size of each tile entry
// in the tilemap in pixels (both directions, so the tile must be square) and
// which tile to draw (starting with the top left as tile 0, increasing left
// to right and then top to bottom), draw that sprite out of the tilemap
// scaled in the x any y direction by the scale factor provided. The sprite
// is drawn with the top left pixel at X, Y and scaling happens down and to
// the right. You can give a negative scaling factor which has the effect of
// mirroring the texture on that axis, but the sprite is still drawn from the
// x, y coordinate down and right. Note that this is monitor orientation aware.
void sprite_draw_tilemap_entry_scaled(int x, int y, int tilesize, int which, float xscale, float yscale, texture_description_t *texture);

// Given an X and Y pixel location on the screen, the size of each tile entry
// in the tilemap in pixels (both directions, so the tile must be square) and
// which tile to draw (starting with the top left as tile 0, increasing left
// to right and then top to bottom), draw that sprite out of the tilemap,
// scaled by the scaling factors in the x and y axis and rotated about its
// center. The sprite is drawn in the position where if it was rotated 0
// degrees, the x, y coordinates requested would be the top left of the
// sprite. If you give a negative scaling factor for an axis, the sprite will
// be drawn with the texture flipped on that axis. Note that this is monitor
// orientation aware.
void sprite_draw_tilemap_entry_scaled_rotated(int x, int y, int tilesize, int which, float xscale, float yscale, float angle, texture_description_t *texture);

#ifdef __cplusplus
}
#endif

#endif
