#include <naomi/video.h>
#include <naomi/system.h>
#include <naomi/timer.h>
#include <naomi/maple.h>
#include <naomi/interrupt.h>
#include <naomi/matrix.h>
#include <naomi/ta.h>
#include <math.h>

// PVR/TA example based heavily off of the Hardware 3D example by marcus.

#define FOV   60.0
#define ZNEAR 1.0
#define ZFAR  100.0
#define ZOFFS 5.0

#define WHITE_THRESHOLD 200

void init_palette()
{
    uint32_t *palette[4] = {
        ta_palette_bank(TA_PALETTE_CLUT8, 0),
        ta_palette_bank(TA_PALETTE_CLUT8, 1),
        ta_palette_bank(TA_PALETTE_CLUT8, 2),
        ta_palette_bank(TA_PALETTE_CLUT8, 3),
    };

    for(int n = 0; n < 256; n++)
    {
        // Blue
        palette[0][n] = ta_palette_entry(rgb(n > WHITE_THRESHOLD ? n : 0, n > WHITE_THRESHOLD ? n : 0, n));

        // Green
        palette[1][n] = ta_palette_entry(rgb(n > WHITE_THRESHOLD ? n : 0, n, n > WHITE_THRESHOLD ? n : 0));

        // Purple
        palette[2][n] = ta_palette_entry(rgb(n, n > WHITE_THRESHOLD ? n : 0, n));

        // Yellow
        palette[3][n] = ta_palette_entry(rgb(n, n, n > WHITE_THRESHOLD ? n : 0));
    }
}

/* Draw a textured polygon for one of the faces of the cube */
void draw_face(vertex_t p1, vertex_t p2, vertex_t p3, vertex_t p4, texture_description_t *tex)
{
    textured_vertex_t verticies[4] = {
        { p1.x, p1.y, p1.z, 0.0, 1.0 },
        { p2.x, p2.y, p2.z, 1.0, 1.0 },
        { p3.x, p3.y, p3.z, 0.0, 0.0 },
        { p4.x, p4.y, p4.z, 1.0, 0.0 },
    };

    ta_draw_triangle_strip(TA_CMD_POLYGON_TYPE_TRANSPARENT, TA_CMD_POLYGON_STRIPLENGTH_2, verticies, tex);
}

// 8-bit textures that we're loading per side.
extern uint8_t *tex1_png_data;
extern uint8_t *tex2_png_data;
extern uint8_t *tex3_png_data;
extern uint8_t *tex4_png_data;
extern uint8_t *tex5_png_data;
extern uint8_t *tex6_png_data;
extern uint8_t *sprite1_png_data;
extern uint8_t *sprite2_png_data;

void main()
{
    /* Set up PowerVR display and tile accelerator hardware */
    video_init(VIDEO_COLOR_1555);
    ta_set_background_color(rgb(48, 48, 48));

    /* Create palettes for our grayscale (indexed) textures */
    init_palette();

    /* Load our textures into texture RAM */
    texture_description_t *tex[8];
    tex[0] = ta_texture_desc_malloc_paletted(256, tex1_png_data, TA_PALETTE_CLUT8, 0);
    tex[1] = ta_texture_desc_malloc_paletted(256, tex2_png_data, TA_PALETTE_CLUT8, 1);
    tex[2] = ta_texture_desc_malloc_paletted(256, tex3_png_data, TA_PALETTE_CLUT8, 2);
    tex[3] = ta_texture_desc_malloc_paletted(256, tex4_png_data, TA_PALETTE_CLUT8, 3);
    tex[4] = ta_texture_desc_malloc_paletted(256, tex5_png_data, TA_PALETTE_CLUT8, 1);
    tex[5] = ta_texture_desc_malloc_paletted(256, tex6_png_data, TA_PALETTE_CLUT8, 2);
    tex[6] = ta_texture_desc_malloc_paletted(256, sprite1_png_data, TA_PALETTE_CLUT8, 0);
    tex[7] = ta_texture_desc_malloc_direct(256, sprite2_png_data, TA_TEXTUREMODE_ARGB4444);

#ifdef FEATURE_FREETYPE
    /* Set up our font. */
    extern uint8_t *dejavusans_ttf_data;
    extern unsigned int dejavusans_ttf_len;
    font_t *font_18pt = font_add(dejavusans_ttf_data, dejavusans_ttf_len);
    font_set_size(font_18pt, 18);
    font_t *font_12pt = font_add(dejavusans_ttf_data, dejavusans_ttf_len);
    font_set_size(font_12pt, 12);
#endif

    /* x/y/z rotation amount in degrees */
    int i = 45;
    int j = 45;
    int k = 0;

    int count = 0;
    while (1)
    {
        /* Check buttons, rotate cube based on inputs. */
        maple_poll_buttons();
        jvs_buttons_t buttons = maple_buttons_held();
        if (buttons.player1.button1)
        {
            i++;
        }
        if (buttons.player1.button2)
        {
            j++;
        }
        if (buttons.player1.button3)
        {
            k++;
        }
        if (buttons.player1.button4)
        {
            i--;
        }
        if (buttons.player1.button5)
        {
            j--;
        }
        if (buttons.player1.button6)
        {
            k--;
        }

        /* Set up our throbbing cube. */
        float val = 1.0 + (sin((count / 30.0) * M_PI) / 32.0);
        vertex_t coords[8] = {
            { -val, -val, -val },
            {  val, -val, -val },
            { -val,  val, -val },
            {  val,  val, -val },
            { -val, -val,  val },
            {  val, -val,  val },
            { -val,  val,  val },
            {  val,  val,  val },
        };

        /* Set up the hardware transformation in the SH4 with the transformations we need to do */
        matrix_init_perspective(FOV, ZNEAR, ZFAR);
        matrix_translate_z(ZOFFS);

        /* Rotate the camera about the cube. */
        matrix_rotate_x(i);
        matrix_rotate_y(j);
        matrix_rotate_z(k);

        /* Apply the transformation to all the coordinates, and normalize the
           resulting homogenous coordinates into normal 3D coordinates again. */
        matrix_perspective_transform_vertex(coords, coords, 8);

        /* Begin sending commands to the TA to draw stuff */
        ta_commit_begin();

        /* Draw the 6 faces of the cube */
        draw_face(coords[0], coords[1], coords[2], coords[3], tex[0]);
        draw_face(coords[1], coords[5], coords[3], coords[7], tex[1]);
        draw_face(coords[4], coords[5], coords[0], coords[1], tex[2]);
        draw_face(coords[5], coords[4], coords[7], coords[6], tex[3]);
        draw_face(coords[4], coords[0], coords[6], coords[2], tex[4]);
        draw_face(coords[2], coords[3], coords[6], coords[7], tex[5]);

        /* Draw a box */
        float xcenter = 80.0;
        float ycenter = (float)video_height() - 80.0;
        vertex_t origin = { xcenter, ycenter, 1.0 };
        vertex_t box[4] = {
            { xcenter - 50.0, ycenter + 50.0, 1.0 },
            { xcenter - 50.0, ycenter - 50.0, 1.0 },
            { xcenter + 50.0, ycenter - 50.0, 1.0 },
            { xcenter + 50.0, ycenter + 50.0, 1.0 },
        };

        /* Rotate the box about its own axis. Remember that this is from the world
         * perspective, so build it backwards. */
        matrix_init_identity();
        matrix_rotate_origin_z(&origin, -(float)count);
        matrix_affine_transform_vertex(box, box, 4);

        /* Draw the box to the screen. */
        ta_fill_box(TA_CMD_POLYGON_TYPE_TRANSPARENT, box, rgb(255, 255, 0));

        /* Draw a sprite */
        xcenter = (float)video_width() - 80;
        ycenter = (float)video_height() - 80;
        origin.x = xcenter;
        origin.y = ycenter;
        textured_vertex_t sprite[4] = {
            { xcenter - 50.0, ycenter + 50.0, 1.0, 0.0, 1.0 },
            { xcenter - 50.0, ycenter - 50.0, 1.0, 0.0, 0.0 },
            { xcenter + 50.0, ycenter - 50.0, 1.0, 1.0, 0.0 },
            { xcenter + 50.0, ycenter + 50.0, 1.0, 1.0, 1.0 },
        };

        /* Rotate the sprite about its own axis. Remember that this is from the world
         * perspective, so build it backwards. */
        matrix_init_identity();
        matrix_rotate_origin_z(&origin, (float)count);
        matrix_affine_transform_textured_vertex(sprite, sprite, 4);

        /* Draw the sprite to the screen. */
        ta_draw_quad(TA_CMD_POLYGON_TYPE_TRANSPARENT, sprite, tex[6]);

        /* Draw a test RGBA4444 sprite */
        xcenter = (float)video_width() - 80;
        ycenter = 80;
        textured_vertex_t rgba4444sprite[4] = {
            { xcenter - 50.0, ycenter + 50.0, 1.0, 0.0, 1.0 },
            { xcenter - 50.0, ycenter - 50.0, 1.0, 0.0, 0.0 },
            { xcenter + 50.0, ycenter - 50.0, 1.0, 1.0, 0.0 },
            { xcenter + 50.0, ycenter + 50.0, 1.0, 1.0, 1.0 },
        };
        ta_draw_quad(TA_CMD_POLYGON_TYPE_TRANSPARENT, rgba4444sprite, tex[7]);

        /* Draw a translucent box over some sprites. */
        matrix_init_identity();
        vertex_t covered_box[4] = {
            { xcenter - 80.0, ycenter + 30.0, 1.1 },
            { xcenter - 80.0, ycenter - 30.0, 1.1 },
            { xcenter, ycenter - 30.0, 1.1 },
            { xcenter, ycenter + 30.0, 1.1 },
        };
        ta_fill_box(TA_CMD_POLYGON_TYPE_TRANSPARENT, covered_box, rgba(0, 255, 255, 100));

#ifdef FEATURE_FREETYPE
        /* Now, draw some font text. */
        ta_draw_text(32, 85, font_18pt, rgb(255, 255, 255), "Hello, world!");
        ta_draw_text(32, 105, font_12pt, rgb(0, 255, 244), "Hello, world!");
#endif

        /* Mark the end of the command list */
        ta_commit_end();

        /* Now, request to render it */
        ta_render();

#ifdef FEATURE_FREETYPE
        /* Now, draw some font text. */
        video_draw_text(32, 45, font_18pt, rgb(255, 255, 255), "Hello, world!");
        video_draw_text(32, 65, font_12pt, rgb(0, 255, 244), "Hello, world!");
#endif

        /* Now, display some debugging on top of the TA. */
        video_draw_debug_text(32, 32, rgb(255, 255, 255), "Rendering with TA...\nLiveness counter: %d", count++);

#ifndef FEATURE_FREETYPE
        video_draw_debug_text(32, 48, rgb(255, 0, 0), "Font rendering missing due to missing freetype library!");
        video_draw_debug_text(32, 56, rgb(255, 0, 0), "Compile 3rd party libs and then recompile libnaomi.");
#endif

        video_display_on_vblank();
    }
}

void test()
{
    video_init(VIDEO_COLOR_1555);

    while ( 1 )
    {
        video_fill_screen(rgb(48, 48, 48));
        video_draw_debug_text(320 - 56, 236, rgb(255, 255, 255), "test mode stub");
        video_display_on_vblank();
    }
}
