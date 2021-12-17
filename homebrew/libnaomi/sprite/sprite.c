#include <stdint.h>
#include "naomi/ta.h"
#include "naomi/matrix.h"
#include "naomi/sprite/sprite.h"

void sprite_draw_box(int x0, int y0, int x1, int y1, color_t color)
{
    int left = x0 < x1 ? x0 : x1;
    int right = x0 > x1 ? x0 : x1;
    int top = y0 < y1 ? y0 : y1;
    int bottom = y0 > y1 ? y0 : y1;
    vertex_t box[4] = {
        { (float)left, bottom, 1.0 },
        { (float)left, top, 1.0 },
        { (float)right, top, 1.0 },
        { (float)right, bottom, 1.0 },
    };

    ta_fill_box(TA_CMD_POLYGON_TYPE_TRANSPARENT, box, color);
}

void sprite_draw_simple(int x, int y, texture_description_t *texture)
{
    /* Set up the four corners of the quad so that the sprite is exactly 1:1 sized
     * on the screen. */
    textured_vertex_t sprite[4] = {
        { (float)x, (float)(y + texture->height), 1.0, 0.0, 1.0 },
        { (float)x, (float)y, 1.0, 0.0, 0.0 },
        { (float)(x + texture->width), (float)y, 1.0, 1.0, 0.0 },
        { (float)(x + texture->width), (float)(y + texture->height), 1.0, 1.0, 1.0 }
    };

    /* Draw the sprite to the screen. */
    ta_draw_quad(TA_CMD_POLYGON_TYPE_TRANSPARENT, sprite, texture);
}

void sprite_draw_rotated(int x, int y, float angle, texture_description_t *texture)
{
    /* Set up the four corners of the quad so that the sprite is exactly 1:1 sized
     * on the screen. */
    textured_vertex_t sprite[4] = {
        { (float)x, (float)(y + texture->height), 1.0, 0.0, 1.0 },
        { (float)x, (float)y, 1.0, 0.0, 0.0 },
        { (float)(x + texture->width), (float)y, 1.0, 1.0, 0.0 },
        { (float)(x + texture->width), (float)(y + texture->height), 1.0, 1.0, 1.0 }
    };
    vertex_t origin = { (float)x + ((float)texture->width / 2.0), (float)y + ((float)texture->height / 2.0), 0.0 };

    /* Rotate the sprite based around its center point. */
    matrix_push();
    matrix_init_identity();
    matrix_rotate_origin_z(&origin, angle);
    matrix_affine_transform_textured_vertex(sprite, sprite, 4);
    matrix_pop();

    /* Draw the sprite to the screen. */
    ta_draw_quad(TA_CMD_POLYGON_TYPE_TRANSPARENT, sprite, texture);
}

void sprite_draw_nonsquare(int x, int y, int width, int height, texture_description_t *texture)
{
    /* Set up the four corners of the quad so that the sprite is exactly 1:1 sized
     * on the screen. */
    textured_vertex_t sprite[4] = {
        { (float)x, (float)(y + height), 1.0, 0.0, (float)height / (float)texture->height },
        { (float)x, (float)y, 1.0, 0.0, 0.0 },
        { (float)(x + width), (float)y, 1.0, (float)width / (float)texture->width, 0.0 },
        { (float)(x + width), (float)(y + height), 1.0, (float)width / (float)texture->width, (float)height / (float)texture->height }
    };

    /* Draw the sprite to the screen. */
    ta_draw_quad(TA_CMD_POLYGON_TYPE_TRANSPARENT, sprite, texture);
}

void sprite_draw_rotated_nonsquare(int x, int y, int width, int height, float angle, texture_description_t *texture)
{
    /* Set up the four corners of the quad so that the sprite is exactly 1:1 sized
     * on the screen. */
    textured_vertex_t sprite[4] = {
        { (float)x, (float)(y + height), 1.0, 0.0, (float)height / (float)texture->height },
        { (float)x, (float)y, 1.0, 0.0, 0.0 },
        { (float)(x + width), (float)y, 1.0, (float)width / (float)texture->width, 0.0 },
        { (float)(x + width), (float)(y + height), 1.0, (float)width / (float)texture->width, (float)height / (float)texture->height }
    };
    vertex_t origin = { (float)x + ((float)width / 2.0), (float)y + ((float)height / 2.0), 0.0 };

    /* Rotate the sprite based around its center point. */
    matrix_push();
    matrix_init_identity();
    matrix_rotate_origin_z(&origin, angle);
    matrix_affine_transform_textured_vertex(sprite, sprite, 4);
    matrix_pop();

    /* Draw the sprite to the screen. */
    ta_draw_quad(TA_CMD_POLYGON_TYPE_TRANSPARENT, sprite, texture);
}
