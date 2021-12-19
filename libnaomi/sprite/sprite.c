#include <stdint.h>
#include "naomi/ta.h"
#include "naomi/matrix.h"
#include "naomi/sprite/sprite.h"

// Where on the Z axis we put the sprites. This is a 1/Z value so higher is closer.
#define Z_LOCATION 1000000000.0
#define Z_INCREMENT 10000.0

// Code for incrementing Z location so that we guarantee each sprite is in front of
// all previously placed ones.
extern unsigned int buffer_loc;
static int last_buffer = -1;
static float zloc = Z_LOCATION;

float __sprite_z_location()
{
    // This is intentionally not made thread-safe as video/audio routines
    // in libnaomi and sub-libraries are explicitly documented as not
    // thread safe. You should only be doing graphics updates from one
    // thread, so the overhead of locking can significantly slow rendering.
    if ((int)buffer_loc != last_buffer)
    {
        zloc = Z_LOCATION;
        last_buffer = (int)buffer_loc;
    }

    float cur_z = zloc;
    zloc += Z_INCREMENT;
    return cur_z;
}

void sprite_draw_box(int x0, int y0, int x1, int y1, color_t color)
{
    int left = x0 < x1 ? x0 : x1;
    int right = x0 > x1 ? x0 : x1;
    int top = y0 < y1 ? y0 : y1;
    int bottom = y0 > y1 ? y0 : y1;
    float z = __sprite_z_location();
    vertex_t box[4] = {
        { (float)left, bottom, z },
        { (float)left, top, z },
        { (float)right, top, z },
        { (float)right, bottom, z },
    };

    ta_fill_box(TA_CMD_POLYGON_TYPE_TRANSPARENT, box, color);
}

void sprite_draw_simple(int x, int y, texture_description_t *texture)
{
    /* Set up the four corners of the quad so that the sprite is exactly 1:1 sized
     * on the screen. */
    float z = __sprite_z_location();
    textured_vertex_t sprite[4] = {
        { (float)x, (float)(y + texture->height), z, 0.0, 1.0 },
        { (float)x, (float)y, z, 0.0, 0.0 },
        { (float)(x + texture->width), (float)y, z, 1.0, 0.0 },
        { (float)(x + texture->width), (float)(y + texture->height), z, 1.0, 1.0 }
    };

    /* Draw the sprite to the screen. */
    ta_draw_quad(TA_CMD_POLYGON_TYPE_TRANSPARENT, sprite, texture);
}

void sprite_draw_rotated(int x, int y, float angle, texture_description_t *texture)
{
    /* Set up the four corners of the quad so that the sprite is exactly 1:1 sized
     * on the screen. */
    float z = __sprite_z_location();
    textured_vertex_t sprite[4] = {
        { (float)x, (float)(y + texture->height), z, 0.0, 1.0 },
        { (float)x, (float)y, z, 0.0, 0.0 },
        { (float)(x + texture->width), (float)y, z, 1.0, 0.0 },
        { (float)(x + texture->width), (float)(y + texture->height), z, 1.0, 1.0 }
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

void sprite_draw_scaled(int x, int y, float xscale, float yscale, texture_description_t *texture)
{
    /* Set up the four corners of the quad so that the sprite is exactly 1:1 sized
     * on the screen. */
    float ulow;
    float vlow;
    float uhigh;
    float vhigh;
    if (xscale < 0.0)
    {
        ulow = 1.0;
        uhigh = 0.0;
        xscale = -xscale;
    }
    else
    {
        ulow = 0.0;
        uhigh = 1.0;
    }
    if (yscale < 0.0)
    {
        vlow = 1.0;
        vhigh = 0.0;
        yscale = -yscale;
    }
    else
    {
        vlow = 0.0;
        vhigh = 1.0;
    }

    float z = __sprite_z_location();
    textured_vertex_t sprite[4] = {
        { (float)x, (float)y + ((float)texture->height * yscale), z, ulow, vhigh },
        { (float)x, (float)y, z, ulow, vlow },
        { (float)x + ((float)texture->width * xscale), (float)y, z, uhigh, vlow },
        { (float)x + ((float)texture->width * xscale), (float)y + ((float)texture->height * yscale), z, uhigh, vhigh }
    };

    /* Draw the sprite to the screen. */
    ta_draw_quad(TA_CMD_POLYGON_TYPE_TRANSPARENT, sprite, texture);
}

void sprite_draw_scaled_rotated(int x, int y, float xscale, float yscale, float angle, texture_description_t *texture)
{
    /* Set up the four corners of the quad so that the sprite is exactly 1:1 sized
     * on the screen. */
    float ulow;
    float vlow;
    float uhigh;
    float vhigh;
    if (xscale < 0.0)
    {
        ulow = 1.0;
        uhigh = 0.0;
        xscale = -xscale;
    }
    else
    {
        ulow = 0.0;
        uhigh = 1.0;
    }
    if (yscale < 0.0)
    {
        vlow = 1.0;
        vhigh = 0.0;
        yscale = -yscale;
    }
    else
    {
        vlow = 0.0;
        vhigh = 1.0;
    }

    float z = __sprite_z_location();
    textured_vertex_t sprite[4] = {
        { (float)x, (float)y + ((float)texture->height * yscale), z, ulow, vhigh },
        { (float)x, (float)y, z, ulow, vlow },
        { (float)x + ((float)texture->width * xscale), (float)y, z, uhigh, vlow },
        { (float)x + ((float)texture->width * xscale), (float)y + ((float)texture->height * yscale), z, uhigh, vhigh }
    };
    vertex_t origin = { (float)x + ((float)texture->width * (xscale / 2.0)), (float)y + ((float)texture->height * (yscale / 2.0)), 0.0 };

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
    float z = __sprite_z_location();
    textured_vertex_t sprite[4] = {
        { (float)x, (float)(y + height), z, 0.0, (float)height / (float)texture->height },
        { (float)x, (float)y, z, 0.0, 0.0 },
        { (float)(x + width), (float)y, z, (float)width / (float)texture->width, 0.0 },
        { (float)(x + width), (float)(y + height), z, (float)width / (float)texture->width, (float)height / (float)texture->height }
    };

    /* Draw the sprite to the screen. */
    ta_draw_quad(TA_CMD_POLYGON_TYPE_TRANSPARENT, sprite, texture);
}

void sprite_draw_nonsquare_rotated(int x, int y, int width, int height, float angle, texture_description_t *texture)
{
    /* Set up the four corners of the quad so that the sprite is exactly 1:1 sized
     * on the screen. */
    float z = __sprite_z_location();
    textured_vertex_t sprite[4] = {
        { (float)x, (float)(y + height), z, 0.0, (float)height / (float)texture->height },
        { (float)x, (float)y, z, 0.0, 0.0 },
        { (float)(x + width), (float)y, z, (float)width / (float)texture->width, 0.0 },
        { (float)(x + width), (float)(y + height), z, (float)width / (float)texture->width, (float)height / (float)texture->height }
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

void sprite_draw_nonsquare_scaled(int x, int y, int width, int height, float xscale, float yscale, texture_description_t *texture)
{
    /* Set up the four corners of the quad so that the sprite is exactly 1:1 sized
     * on the screen. */
    float ulow;
    float vlow;
    float uhigh;
    float vhigh;
    if (xscale < 0.0)
    {
        ulow = (float)width / (float)texture->width;
        uhigh = 0.0;
        xscale = -xscale;
    }
    else
    {
        ulow = 0.0;
        uhigh = (float)width / (float)texture->width;
    }
    if (yscale < 0.0)
    {
        vlow = (float)height / (float)texture->height;
        vhigh = 0.0;
        yscale = -yscale;
    }
    else
    {
        vlow = 0.0;
        vhigh = (float)height / (float)texture->height;
    }

    float z = __sprite_z_location();
    textured_vertex_t sprite[4] = {
        { (float)x, (float)y + ((float)height * yscale), z, ulow, vhigh },
        { (float)x, (float)y, z, ulow, vlow },
        { (float)x + ((float)width * xscale), (float)y, z, uhigh, vlow },
        { (float)x + ((float)width * xscale), (float)y + ((float)height * yscale), z, uhigh, vhigh }
    };

    /* Draw the sprite to the screen. */
    ta_draw_quad(TA_CMD_POLYGON_TYPE_TRANSPARENT, sprite, texture);
}

void sprite_draw_nonsquare_scaled_rotated(int x, int y, int width, int height, float xscale, float yscale, float angle, texture_description_t *texture)
{
    /* Set up the four corners of the quad so that the sprite is exactly 1:1 sized
     * on the screen. */
    float ulow;
    float vlow;
    float uhigh;
    float vhigh;
    if (xscale < 0.0)
    {
        ulow = (float)width / (float)texture->width;
        uhigh = 0.0;
        xscale = -xscale;
    }
    else
    {
        ulow = 0.0;
        uhigh = (float)width / (float)texture->width;
    }
    if (yscale < 0.0)
    {
        vlow = (float)height / (float)texture->height;
        vhigh = 0.0;
        yscale = -yscale;
    }
    else
    {
        vlow = 0.0;
        vhigh = (float)height / (float)texture->height;
    }

    float z = __sprite_z_location();
    textured_vertex_t sprite[4] = {
        { (float)x, (float)y + ((float)height * yscale), z, ulow, vhigh },
        { (float)x, (float)y, z, ulow, vlow },
        { (float)x + ((float)width * xscale), (float)y, z, uhigh, vlow },
        { (float)x + ((float)width * xscale), (float)y + ((float)height * yscale), z, uhigh, vhigh }
    };
    vertex_t origin = { (float)x + ((float)width * (xscale / 2.0)), (float)y + ((float)height * (yscale / 2.0)), 0.0 };

    /* Rotate the sprite based around its center point. */
    matrix_push();
    matrix_init_identity();
    matrix_rotate_origin_z(&origin, angle);
    matrix_affine_transform_textured_vertex(sprite, sprite, 4);
    matrix_pop();

    /* Draw the sprite to the screen. */
    ta_draw_quad(TA_CMD_POLYGON_TYPE_TRANSPARENT, sprite, texture);
}

void sprite_draw_tilemap_entry(int x, int y, int tilesize, int which, texture_description_t *texture)
{
    /* Calculate the x/y position of the UV coordinates for this sprite in the tilemap. */
    int tilestride = texture->width / tilesize;
    int tiley = which / tilestride;
    int tilex = which % tilestride;

    float ulow = (float)(tilesize * tilex) / (float)texture->width;
    float uhigh = (float)(tilesize * (tilex + 1)) / (float)texture->width;
    float vlow = (float)(tilesize * tiley) / (float)texture->height;
    float vhigh = (float)(tilesize * (tiley + 1)) / (float)texture->height;

    /* Set up the four corners of the quad so that the sprite is exactly 1:1 sized
     * on the screen. */
    float z = __sprite_z_location();
    textured_vertex_t sprite[4] = {
        { (float)x, (float)(y + tilesize), z, ulow, vhigh },
        { (float)x, (float)y, z, ulow, vlow },
        { (float)(x + tilesize), (float)y, z, uhigh, vlow },
        { (float)(x + tilesize), (float)(y + tilesize), z, uhigh, vhigh }
    };

    /* Draw the sprite to the screen. */
    ta_draw_quad(TA_CMD_POLYGON_TYPE_TRANSPARENT, sprite, texture);
}

void sprite_draw_tilemap_entry_rotated(int x, int y, int tilesize, int which, float angle, texture_description_t *texture)
{
    /* Calculate the x/y position of the UV coordinates for this sprite in the tilemap. */
    int tilestride = texture->width / tilesize;
    int tiley = which / tilestride;
    int tilex = which % tilestride;

    float ulow = (float)(tilesize * tilex) / (float)texture->width;
    float uhigh = (float)(tilesize * (tilex + 1)) / (float)texture->width;
    float vlow = (float)(tilesize * tiley) / (float)texture->height;
    float vhigh = (float)(tilesize * (tiley + 1)) / (float)texture->height;

    /* Set up the four corners of the quad so that the sprite is exactly 1:1 sized
     * on the screen. */
    float z = __sprite_z_location();
    textured_vertex_t sprite[4] = {
        { (float)x, (float)(y + tilesize), z, ulow, vhigh },
        { (float)x, (float)y, z, ulow, vlow },
        { (float)(x + tilesize), (float)y, z, uhigh, vlow },
        { (float)(x + tilesize), (float)(y + tilesize), z, uhigh, vhigh }
    };
    vertex_t origin = { (float)x + ((float)tilesize / 2.0), (float)y + ((float)tilesize / 2.0), 0.0 };

    /* Rotate the sprite based around its center point. */
    matrix_push();
    matrix_init_identity();
    matrix_rotate_origin_z(&origin, angle);
    matrix_affine_transform_textured_vertex(sprite, sprite, 4);
    matrix_pop();

    /* Draw the sprite to the screen. */
    ta_draw_quad(TA_CMD_POLYGON_TYPE_TRANSPARENT, sprite, texture);
}

void sprite_draw_tilemap_entry_scaled(int x, int y, int tilesize, int which, float xscale, float yscale, texture_description_t *texture)
{
    /* Calculate the x/y position of the UV coordinates for this sprite in the tilemap. */
    int tilestride = texture->width / tilesize;
    int tiley = which / tilestride;
    int tilex = which % tilestride;

    float ulow;
    float vlow;
    float uhigh;
    float vhigh;
    if (xscale < 0.0)
    {
        ulow = (float)(tilesize * (tilex + 1)) / (float)texture->width;
        uhigh = (float)(tilesize * tilex) / (float)texture->width;
        xscale = -xscale;
    }
    else
    {
        ulow = (float)(tilesize * tilex) / (float)texture->width;
        uhigh = (float)(tilesize * (tilex + 1)) / (float)texture->width;
    }
    if (yscale < 0.0)
    {
        vlow = (float)(tilesize * (tiley + 1)) / (float)texture->height;
        vhigh = (float)(tilesize * tiley) / (float)texture->height;
        yscale = -yscale;
    }
    else
    {
        vlow = (float)(tilesize * tiley) / (float)texture->height;
        vhigh = (float)(tilesize * (tiley + 1)) / (float)texture->height;
    }


    /* Set up the four corners of the quad so that the sprite is exactly 1:1 sized
     * on the screen. */
    float z = __sprite_z_location();
    textured_vertex_t sprite[4] = {
        { (float)x, (float)y + ((float)tilesize * yscale), z, ulow, vhigh },
        { (float)x, (float)y, z, ulow, vlow },
        { (float)x + ((float)tilesize * xscale), (float)y, z, uhigh, vlow },
        { (float)x + ((float)tilesize * xscale), (float)y + ((float)tilesize * yscale), z, uhigh, vhigh }
    };

    /* Draw the sprite to the screen. */
    ta_draw_quad(TA_CMD_POLYGON_TYPE_TRANSPARENT, sprite, texture);
}

void sprite_draw_tilemap_entry_scaled_rotated(int x, int y, int tilesize, int which, float xscale, float yscale, float angle, texture_description_t *texture)
{
    /* Calculate the x/y position of the UV coordinates for this sprite in the tilemap. */
    int tilestride = texture->width / tilesize;
    int tiley = which / tilestride;
    int tilex = which % tilestride;

    float ulow;
    float vlow;
    float uhigh;
    float vhigh;
    if (xscale < 0.0)
    {
        ulow = (float)(tilesize * (tilex + 1)) / (float)texture->width;
        uhigh = (float)(tilesize * tilex) / (float)texture->width;
        xscale = -xscale;
    }
    else
    {
        ulow = (float)(tilesize * tilex) / (float)texture->width;
        uhigh = (float)(tilesize * (tilex + 1)) / (float)texture->width;
    }
    if (yscale < 0.0)
    {
        vlow = (float)(tilesize * (tiley + 1)) / (float)texture->height;
        vhigh = (float)(tilesize * tiley) / (float)texture->height;
        yscale = -yscale;
    }
    else
    {
        vlow = (float)(tilesize * tiley) / (float)texture->height;
        vhigh = (float)(tilesize * (tiley + 1)) / (float)texture->height;
    }


    /* Set up the four corners of the quad so that the sprite is exactly 1:1 sized
     * on the screen. */
    float z = __sprite_z_location();
    textured_vertex_t sprite[4] = {
        { (float)x, (float)y + ((float)tilesize * yscale), z, ulow, vhigh },
        { (float)x, (float)y, z, ulow, vlow },
        { (float)x + ((float)tilesize * xscale), (float)y, z, uhigh, vlow },
        { (float)x + ((float)tilesize * xscale), (float)y + ((float)tilesize * yscale), z, uhigh, vhigh }
    };
    vertex_t origin = { (float)x + ((float)tilesize * (xscale / 2.0)), (float)y + ((float)tilesize * (yscale / 2.0)), 0.0 };

    /* Rotate the sprite based around its center point. */
    matrix_push();
    matrix_init_identity();
    matrix_rotate_origin_z(&origin, angle);
    matrix_affine_transform_textured_vertex(sprite, sprite, 4);
    matrix_pop();

    /* Draw the sprite to the screen. */
    ta_draw_quad(TA_CMD_POLYGON_TYPE_TRANSPARENT, sprite, texture);
}
