#include <string.h>
#include <naomi/video.h>
#include <naomi/ta.h>
#include <naomi/font.h>
#include <naomi/maple.h>
#include <naomi/sprite/sprite.h>

#ifdef FEATURE_FREETYPE
#define draw_text(x, y, font, color, msg...) ta_draw_text(x, y, font, color, msg)
#else
#define draw_text(x, y, font, color, msg...) video_draw_debug_text(x, y, color, msg)
#endif

#define MAXPAGES 3

#define WPAD (crate_png_width + 2)
#define HPAD (crate_png_height + 2)

void main()
{
    /* Set up PowerVR display and tile accelerator hardware */
    video_init(VIDEO_COLOR_8888);
    ta_set_background_color(rgb(48, 48, 48));

#ifdef FEATURE_FREETYPE
    /* Load a nicer font than the console font. */
    extern uint8_t *dejavusans_ttf_data;
    extern unsigned int dejavusans_ttf_len;
    font_t *font = font_add(dejavusans_ttf_data, dejavusans_ttf_len);
    font_set_size(font, 12);
#endif

    /* Load page arrows into VRAM. */
    extern uint8_t *arrowlt_png_data;
    extern unsigned int arrowlt_png_width;
    texture_description_t *leftarrow = ta_texture_desc_malloc_direct(arrowlt_png_width, arrowlt_png_data, TA_TEXTUREMODE_ARGB4444);
    extern uint8_t *arrowrt_png_data;
    extern unsigned int arrowrt_png_width;
    texture_description_t *rightarrow = ta_texture_desc_malloc_direct(arrowrt_png_width, arrowrt_png_data, TA_TEXTUREMODE_ARGB4444);

    /* Load our crate texture into VRAM. Note that the image is purposefully square and a multiple
     * of 2 with a minimum value of 8x8. */
    extern uint8_t *crate_png_data;
    extern unsigned int crate_png_width;
    extern unsigned int crate_png_height;
    texture_description_t *crate = ta_texture_desc_malloc_direct(crate_png_width, crate_png_data, TA_TEXTUREMODE_ARGB4444);

    /* Load our diagonal texture into VRAM. */
    extern uint8_t *diagonal_png_data;
    extern unsigned int diagonal_png_width;
    texture_description_t *diagonal = ta_texture_desc_malloc_direct(diagonal_png_width, diagonal_png_data, TA_TEXTUREMODE_ARGB4444);

    /* Load our sonic texture into VRAM. Note that the larger side needs to be a multiple of 2
     * and at least 8 pixels long, so we round up (wasteful to VRAM, but shows how to do this). */
    extern uint8_t *sonic_png_data;
    extern unsigned int sonic_png_width;
    extern unsigned int sonic_png_height;
    unsigned int uvsize = ta_round_uvsize(sonic_png_width > sonic_png_height ? sonic_png_width : sonic_png_height);
    void *sonicvram = ta_texture_malloc(uvsize, 16);
    ta_texture_load_sprite(sonicvram, uvsize, 16, 0, 0, sonic_png_width, sonic_png_height, sonic_png_data);
    texture_description_t *sonic = ta_texture_desc_direct(sonicvram, uvsize, TA_TEXTUREMODE_ARGB4444);

    /* Load our sprite sheets for a few simple animations. */
    extern uint8_t *coins_png_data;
    extern unsigned int coins_png_width;
    texture_description_t *coins = ta_texture_desc_malloc_direct(coins_png_width, coins_png_data, TA_TEXTUREMODE_ARGB4444);
    extern uint8_t *color_gems_png_data;
    extern unsigned int color_gems_png_width;
    texture_description_t *color_gems = ta_texture_desc_malloc_direct(color_gems_png_width, color_gems_png_data, TA_TEXTUREMODE_ARGB4444);
    extern uint8_t *gray_gem_png_data;
    extern unsigned int gray_gem_png_width;
    texture_description_t *gray_gem = ta_texture_desc_malloc_direct(gray_gem_png_width, gray_gem_png_data, TA_TEXTUREMODE_ARGB4444);

    /* Display sprite demo. */
    int tickcount = 0;
    int page = 0;
    while (1)
    {
        /* Listen to inputs so we can change pages */
        maple_poll_buttons();
        jvs_buttons_t buttons = maple_buttons_pressed();

        if (buttons.player1.left)
        {
            if (page == 0)
            {
                page = MAXPAGES - 1;
            }
            else
            {
                page--;
            }
        }
        else if (buttons.player1.right || buttons.player1.start)
        {
            if (page == (MAXPAGES - 1))
            {
                page = 0;
            }
            else
            {
                page++;
            }
        }

        /* Begin sending commands to the TA to draw stuff */
        ta_commit_begin();

        switch (page)
        {
            case 0:
            {
                /* Simple and 90 degree sprite display. */
                draw_text(64, 46, font, rgb(255, 255, 255), "Simple, non-rotated as well as rotated sprites:");

                /* Simple sprite */
                sprite_draw_simple(64, 64, crate);

                /* Rotated sprites */
                sprite_draw_rotated(144, 64, 90, crate);
                sprite_draw_rotated(224, 64, 180, crate);
                sprite_draw_rotated(304, 64, 270, crate);

                /* Arbitrarily rotated sprite. */
                sprite_draw_rotated(390, 64, tickcount % 360, crate);

                /* Odd shaped (non-square) sprites. */
                draw_text(64, 138, font, rgb(255, 255, 255), "Odd-shaped (non-square) sprites,\nnormal and rotated:");

                /* Simple odd sprite */
                sprite_draw_nonsquare(64, 172, sonic_png_width, sonic_png_height, sonic);

                /* Rotated odd sprite */
                sprite_draw_nonsquare_rotated(150, 172, sonic_png_width, sonic_png_height, 360 - (tickcount % 360), sonic);

                /* Transparency example */
                draw_text(280, 146, font, rgb(255, 255, 255), "Proper alpha blending:");

                /* Draw two sprites overlappint */
                sprite_draw_simple(280, 172 + sonic_png_height - crate_png_height, crate);
                sprite_draw_nonsquare(295, 172, sonic_png_width, sonic_png_height, sonic);

                /* Tilemaps (easy animation, 2D maps, etc). */
                draw_text(64, 270, font, rgb(255, 255, 255), "Tilemap support, both normal and rotated:");

                /* Draw animated coins, with full animation cycle every 30 frames. */
                sprite_draw_tilemap_entry(64, 288, 32, (tickcount / 6) % 5, coins);
                sprite_draw_tilemap_entry(100, 288, 32, (((tickcount / 6) + 1) % 5) + 5, coins);
                sprite_draw_tilemap_entry(134, 288, 32, (((tickcount / 6) + 3) % 5) + 10, coins);

                /* Draw animated gems, with full cycle animation cycle every 60 frames. */
                sprite_draw_tilemap_entry_rotated(180, 288, 32, (tickcount / 15) % 4, (tickcount * 9) % 360, color_gems);
                sprite_draw_tilemap_entry_rotated(220, 288, 32, ((tickcount / 15) % 4) + 4, (tickcount * 9) % 360 + 60, color_gems);
                sprite_draw_tilemap_entry_rotated(260, 288, 32, ((tickcount / 15) % 4) + 8, (tickcount * 9) % 360 + 120, color_gems);
                sprite_draw_tilemap_entry_rotated(300, 288, 32, ((tickcount / 15) % 4) + 12, (tickcount * 9) % 360 + 180, color_gems);
                sprite_draw_tilemap_entry_rotated(340, 288, 32, (tickcount / 15) % 4, (tickcount * 9) % 360 + 240, gray_gem);

                break;
            }
            case 1:
            {
                /* Sprites using scaling to flip one or more axis. */
                draw_text(64, 46, font, rgb(255, 255, 255), "Using scaling to mirror sprites:");

                /* Mirrored in neither, then x, then y, then both x and y. */
                sprite_draw_scaled(64, 64, 1.0, 1.0, diagonal);
                sprite_draw_scaled(64 + 40, 64, -1.0, 1.0, diagonal);
                sprite_draw_scaled(64 + 80, 64, 1.0, -1.0, diagonal);
                sprite_draw_scaled(64 + 120, 64, -1.0, -1.0, diagonal);

                /* Sprites that scale normally. */
                draw_text(275, 46, font, rgb(255, 255, 255), "Normal scaling/rotation of sprites:");
                sprite_draw_scaled(275, 64, 1.0, 2.0, diagonal);
                sprite_draw_scaled(275 + 40, 64, 2.0, 1.0, diagonal);
                sprite_draw_scaled(275 + 120, 64, 2.0, 2.0, diagonal);
                sprite_draw_scaled_rotated(275 + 200, 64, 2.0, 2.0, (tickcount * 2) % 360, diagonal);

                /* Odd shaped (non-square) sprites. */
                draw_text(64, 138, font, rgb(255, 255, 255), "Odd-shaped (non-square) sprites can be scaled and rotated:");

                /* Scaled odd sprite */
                sprite_draw_nonsquare_scaled(64, 172, sonic_png_width, sonic_png_height, -2.0, 2.0, sonic);
                sprite_draw_nonsquare_scaled(64 + 140, 172, sonic_png_width, sonic_png_height, 2.0, 2.0, sonic);

                /* Rotated and scaled odd sprite */
                sprite_draw_nonsquare_scaled_rotated(64 + 305, 172, sonic_png_width, sonic_png_height, 1.5, 1.5, (tickcount * 2) % 360, sonic);
                sprite_draw_nonsquare_scaled_rotated(64 + 305, 172 + 150, sonic_png_width, sonic_png_height, 0.5, 0.5, -((tickcount * 3) % 360), sonic);
                sprite_draw_nonsquare_scaled_rotated(64 + 360, 172 + 150, sonic_png_width, sonic_png_height, -0.5, 0.5, (tickcount * 3) % 360, sonic);

                break;
            }
            case 2:
            {
                /* Tilemaps (easy animation, 2D maps, etc). */
                draw_text(64, 46, font, rgb(255, 255, 255), "Tilemap support with scaling:");

                /* Draw animated coins, with full animation cycle every 30 frames. */
                sprite_draw_tilemap_entry_scaled(64, 64, 32, ((tickcount / 6) % 5), 1.0, 2.0, coins);
                sprite_draw_tilemap_entry_scaled(100, 64, 32, ((tickcount / 6) % 5), 2.0, 1.0, coins);
                sprite_draw_tilemap_entry_scaled(164, 64, 32, ((tickcount / 6) % 5), 2.0, 2.0, coins);

                sprite_draw_tilemap_entry_scaled(64, 140, 32, (((tickcount / 6) + 1) % 5) + 5, -1.0, -2.0, coins);
                sprite_draw_tilemap_entry_scaled(100, 140, 32, (((tickcount / 6) + 1) % 5) + 5, -2.0, -1.0, coins);
                sprite_draw_tilemap_entry_scaled(164, 140, 32, (((tickcount / 6) + 1) % 5) + 5, -2.0, -2.0, coins);

                sprite_draw_tilemap_entry_scaled(64, 216, 32, (((tickcount / 6) + 3) % 5) + 10, 1.5, 1.5, coins);
                sprite_draw_tilemap_entry_scaled(110, 216, 32, (((tickcount / 6) + 3) % 5) + 10, 0.75, 0.75, coins);
                sprite_draw_tilemap_entry_scaled(138, 216, 32, (((tickcount / 6) + 3) % 5) + 10, 0.25, 0.25, coins);

                /* Tilemaps with rotation and scaling applied. */
                draw_text(320, 46, font, rgb(255, 255, 255), "Tilemap support with scaling and rotation:");

                /* Draw animated gems, with full cycle animation cycle every 60 frames. */
                sprite_draw_tilemap_entry_scaled_rotated(320, 64, 32, (tickcount / 15) % 4, 2.0, 2.0, (tickcount * 3 + 40) % 360, color_gems);
                sprite_draw_tilemap_entry_scaled_rotated(380, 64, 32, ((tickcount / 15) % 4) + 4, 2.0, 2.0, (tickcount * 3 + 80) % 360, color_gems);
                sprite_draw_tilemap_entry_scaled_rotated(320, 138, 32, ((tickcount / 15) % 4) + 8, 1.5, 1.5, (tickcount * 3 + 120) % 360, color_gems);
                sprite_draw_tilemap_entry_scaled_rotated(370, 138, 32, ((tickcount / 15) % 4) + 12, 1.5, 1.5, (tickcount * 3 + 160) % 360, color_gems);
                sprite_draw_tilemap_entry_scaled_rotated(330, 192, 32, (tickcount / 15) % 4, 1.25, 1.25, (tickcount * 3 + 200) % 360, gray_gem);
                sprite_draw_tilemap_entry_scaled_rotated(360, 192, 32, (tickcount / 15) % 4, 2.5, 1.25, (tickcount * 3 + 240) % 360, gray_gem);
                sprite_draw_tilemap_entry_scaled_rotated(330, 240, 32, (tickcount / 15) % 4, 1.25, 2.5, (tickcount * 3 + 280) % 360, gray_gem);

                break;
            }
        }

        /* Draw page markers at the bottom */
        sprite_draw_box(32 + 8, video_height() - 32 + 8, video_width() - 32 + 8, video_height() - 64 + 8, rgb(90, 90, 90));
        sprite_draw_box(32 + 4, video_height() - 32 + 4, video_width() - 32 + 4, video_height() - 64 + 4, rgb(180, 180, 180));
        sprite_draw_box(32, video_height() - 32, video_width() - 32, video_height() - 64, rgb(255, 255, 255));

        char instructions[] = "press arrow keys to change demo screen";
#ifdef FEATURE_FREETYPE
        font_metrics_t metrics = font_get_text_metrics(font, instructions);
        int instwidth = metrics.width + 40;
#else
        int instwidth = (strlen(instructions) * 8) + 40;
#endif

        sprite_draw_simple((video_width() - instwidth) / 2, video_height() - 64 + 8, leftarrow);
        sprite_draw_simple(((video_width() - instwidth) / 2) + 20, video_height() - 64 + 8, rightarrow);
        draw_text(((video_width() - instwidth) / 2) + 40, video_height() - 64 + 8, font, rgb(32, 32, 32), instructions);

        /* Mark the end of the command list */
        ta_commit_end();

        /* Now, request to render it */
        ta_render();

        /* Now, display after waiting for vertical refresh */
        video_display_on_vblank();
        tickcount++;
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
