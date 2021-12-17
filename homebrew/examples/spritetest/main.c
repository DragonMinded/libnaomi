#include <naomi/video.h>
#include <naomi/ta.h>
#include <naomi/font.h>
#include <naomi/maple.h>
#include <naomi/sprite/sprite.h>

#ifdef FEATURE_FREETYPE
#define draw_text ta_draw_text
#else
#define draw_text video_draw_text
#endif

#define MAXPAGES 1

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
        else if (buttons.player1.right)
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
        }

        /* Draw page markers at the bottom */
        sprite_draw_box(32 + 8, video_height() - 32 + 8, video_width() - 32 + 8, video_height() - 64 + 8, rgb(90, 90, 90));
        sprite_draw_box(32 + 4, video_height() - 32 + 4, video_width() - 32 + 4, video_height() - 64 + 4, rgb(180, 180, 180));
        sprite_draw_box(32, video_height() - 32, video_width() - 32, video_height() - 64, rgb(255, 255, 255));

        char instructions[] = "press arrow keys to change demo screen";
        font_metrics_t metrics = font_get_text_metrics(font, instructions);
        int instwidth = metrics.width + 40;

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
