#include <naomi/video.h>
#include <naomi/ta.h>
#include <naomi/font.h>
#include <naomi/sprite/sprite.h>

#ifdef FEATURE_FREETYPE
#define draw_text ta_draw_text
#else
#define draw_text video_draw_text
#endif

// Display a white border around sprite locations.
#define DISPLAY_GUIDEBOXES 0

#if DISPLAY_GUIDEBOXES
#define draw_guidebox(x0, y0, x1, y1, color) sprite_draw_box(x0, y0, x1, y1, color)
#else
#define draw_guidebox(x0, y0, x1, y1, color) do { } while(0)
#endif

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

    /* Load our crate texture into VRAM. Note that the image is purposefully square and a multiple
     * of 2 with a minimum value of 8x8. */
    extern uint8_t *crate_png_data;
    extern unsigned int crate_png_width;
    extern unsigned int crate_png_height;
    texture_description_t *crate = ta_texture_desc_malloc_direct(crate_png_width, crate_png_data, TA_TEXTUREMODE_ARGB4444);

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
    while (1)
    {
        /* Begin sending commands to the TA to draw stuff */
        ta_commit_begin();

        /* Simple and 90 degree sprite display. */
        draw_text(64, 46, font, rgb(255, 255, 255), "Simple, non-rotated as well as rotated sprites:");

        /* Simple sprite */
        draw_guidebox(63, 63, 63 + WPAD, 63 + HPAD, rgb(255, 255, 255));
        sprite_draw_simple(64, 64, crate);

        /* Rotated sprites */
        draw_guidebox(143, 63, 143 + WPAD, 63 + HPAD, rgb(255, 255, 255));
        sprite_draw_rotated(144, 64, 90, crate);
        draw_guidebox(223, 63, 223 + WPAD, 63 + HPAD, rgb(255, 255, 255));
        sprite_draw_rotated(224, 64, 180, crate);
        draw_guidebox(303, 63, 303 + WPAD, 63 + HPAD, rgb(255, 255, 255));
        sprite_draw_rotated(304, 64, 270, crate);

        draw_guidebox(389, 63, 389 + WPAD, 63 + HPAD, rgb(255, 255, 255));
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
