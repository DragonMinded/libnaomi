#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <naomi/video.h>
#include <naomi/maple.h>
#include <naomi/color.h>
#include <naomi/system.h>
#include <naomi/ta.h>
#include <naomi/sprite/sprite.h>

#include "pallete_animation.h"

// Copy data from the palette files directly into TA palette banks
void _load_palettes()
{
    // Static palettes
    uint32_t *paletteA = ta_palette_bank(TA_PALETTE_CLUT4, 0);
    extern uint32_t *pal_a_png_palette;
    memcpy(paletteA, pal_a_png_palette, 64);

    uint32_t *paletteB = ta_palette_bank(TA_PALETTE_CLUT4, 1);
    extern uint32_t *pal_b_ase_palette;
    memcpy(paletteB, pal_b_ase_palette, 64);

    uint32_t *paletteC = ta_palette_bank(TA_PALETTE_CLUT4, 2);
    extern uint32_t *pal_c_gpl_palette;
    memcpy(paletteC, pal_c_gpl_palette, 64);

    uint32_t *paletteD = ta_palette_bank(TA_PALETTE_CLUT4, 3);
    extern uint32_t *pal_d_txt_palette;
    memcpy(paletteD, pal_d_txt_palette, 64);

    // Animated palettes
    uint32_t *paletteE = ta_palette_bank(TA_PALETTE_CLUT4, 4);
    extern uint32_t *pal_a_png_palette;
    memcpy(paletteE, pal_a_png_palette, 64);

    uint32_t *paletteF = ta_palette_bank(TA_PALETTE_CLUT4, 5);
    extern uint32_t *pal_b_ase_palette;
    memcpy(paletteF, pal_b_ase_palette, 64);

    uint32_t *paletteG = ta_palette_bank(TA_PALETTE_CLUT4, 6);
    extern uint32_t *pal_c_gpl_palette;
    memcpy(paletteG, pal_c_gpl_palette, 64);

    uint32_t *paletteH = ta_palette_bank(TA_PALETTE_CLUT4, 7);
    extern uint32_t *pal_d_txt_palette;
    memcpy(paletteH, pal_d_txt_palette, 64);
}

void main()
{
    video_init(VIDEO_COLOR_8888);

    // video_set_background_color(rgb(0, 0, 0));
    ta_set_background_color(rgb(48, 48, 48));

    uint8_t drawMode = 0;

    extern uint8_t *sample_arrow_png_data;
    extern unsigned int sample_arrow_png_width;

    extern uint8_t *sample_gradient_png_data;
    extern unsigned int sample_gradient_png_width;

    extern uint8_t *sample_spinner_png_data;
    extern unsigned int sample_spinner_png_width;

    // For TA_PALETTE_CLUT4 mode, alloc for a 4-bit size (4 bits -> 16 color indices)
    // Similarly, for TA_PALETTE_CLUT8, alloc for 8-bit size (256 color indices)
    void *arrow_vram = ta_texture_malloc(sample_arrow_png_width, 4);
    ta_texture_load(arrow_vram, sample_arrow_png_width, 4, sample_arrow_png_data);

    void *gradient_vram = ta_texture_malloc(sample_gradient_png_width, 4);
    ta_texture_load(gradient_vram, sample_gradient_png_width, 4, sample_gradient_png_data);

    void *spinner_vram = ta_texture_malloc(sample_spinner_png_width, 4);
    ta_texture_load(spinner_vram, sample_spinner_png_width, 4, sample_spinner_png_data);

    _load_palettes();

    // Pre-loaded shared sprites, no malloc
    // Assign different palettes to each texture description

    // Gradients use static palettes (A-D)
    texture_description_t *gradient_A = ta_texture_desc_paletted(
        gradient_vram,
        sample_gradient_png_width,
        TA_PALETTE_CLUT4,
        0
    );
    texture_description_t *gradient_B = ta_texture_desc_paletted(
        gradient_vram,
        sample_gradient_png_width,
        TA_PALETTE_CLUT4,
        1
    );
    texture_description_t *gradient_C = ta_texture_desc_paletted(
        gradient_vram,
        sample_gradient_png_width,
        TA_PALETTE_CLUT4,
        2
    );
    texture_description_t *gradient_D = ta_texture_desc_paletted(
        gradient_vram,
        sample_gradient_png_width,
        TA_PALETTE_CLUT4,
        3
    );

    // Spinners use animated palettes (E-H)
    texture_description_t *spinner_E = ta_texture_desc_paletted(
        spinner_vram,
        sample_spinner_png_width,
        TA_PALETTE_CLUT4,
        4
    );
    texture_description_t *spinner_F = ta_texture_desc_paletted(
        spinner_vram,
        sample_spinner_png_width,
        TA_PALETTE_CLUT4,
        5
    );
    texture_description_t *spinner_G = ta_texture_desc_paletted(
        spinner_vram,
        sample_spinner_png_width,
        TA_PALETTE_CLUT4,
        6
    );
    texture_description_t *spinner_H = ta_texture_desc_paletted(
        spinner_vram,
        sample_spinner_png_width,
        TA_PALETTE_CLUT4,
        7
    );

    // Arrows use animated palettes (E-H)
    texture_description_t *arrow_E = ta_texture_desc_paletted(
        arrow_vram,
        sample_arrow_png_width,
        TA_PALETTE_CLUT4,
        4
    );
    texture_description_t *arrow_F = ta_texture_desc_paletted(
        arrow_vram,
        sample_arrow_png_width,
        TA_PALETTE_CLUT4,
        5
    );
    texture_description_t *arrow_G = ta_texture_desc_paletted(
        arrow_vram,
        sample_arrow_png_width,
        TA_PALETTE_CLUT4,
        6
    );
    texture_description_t *arrow_H = ta_texture_desc_paletted(
        arrow_vram,
        sample_arrow_png_width,
        TA_PALETTE_CLUT4,
        7
    );

    uint8_t anim_time = 0;
    uint8_t anim_delay = 2;
    while ( 1 )
    {

        ta_set_background_color(rgb(48, 48, 48));

        // Poll inputs
        maple_poll_buttons();
        jvs_buttons_t pressed = maple_buttons_pressed();

        if(pressed.player1.right || pressed.player1.left)
        {
            drawMode = !drawMode;
        }
        if(pressed.player1.up)
        {
            if(anim_delay > 1) 
                anim_delay -= 1;
        }
        if(pressed.player1.down)
        {
            if(anim_delay < 15)
                anim_delay += 1;
        }

        if(anim_time >= anim_delay)
        {
            if(drawMode)
            {
                ta_palette_animate_forward(TA_PALETTE_CLUT4, 4);
                ta_palette_animate_forward(TA_PALETTE_CLUT4, 5);
                ta_palette_animate_forward(TA_PALETTE_CLUT4, 6);
                ta_palette_animate_forward(TA_PALETTE_CLUT4, 7);
            } else {
                ta_palette_animate_backward(TA_PALETTE_CLUT4, 4);
                ta_palette_animate_backward(TA_PALETTE_CLUT4, 5);
                ta_palette_animate_backward(TA_PALETTE_CLUT4, 6);
                ta_palette_animate_backward(TA_PALETTE_CLUT4, 7);
            }
            anim_time = 0;
        }

        ta_commit_begin();

        sprite_draw_scaled(30, 30, 6, 6, gradient_A);
        sprite_draw_scaled(30, 90, 6, 6, gradient_B);
        sprite_draw_scaled(30, 150, 6, 6, gradient_C);
        sprite_draw_scaled(30, 210, 6, 6, gradient_D);

        sprite_draw_scaled(90, 30, 6, 6, spinner_E);
        sprite_draw_scaled(90, 90, 6, 6, spinner_F);
        sprite_draw_scaled(90, 150, 6, 6, spinner_G);
        sprite_draw_scaled(90, 210, 6, 6, spinner_H);

        sprite_draw_scaled(150, 30, 6, 6, arrow_E);
        sprite_draw_scaled(150, 90, 6, 6, arrow_F);
        sprite_draw_scaled(150, 150, 6, 6, arrow_G);
        sprite_draw_scaled(150, 210, 6, 6, arrow_H);

        ta_commit_end();

        ta_render();

        video_draw_debug_text(30, 270, rgb(255, 255, 255), "Demonstration of palette import and palette animation.");

        video_draw_debug_text(30, 300, rgb(255, 255, 255), "Press Left or Right to change animation direction.");
        video_draw_debug_text(30, 330, rgb(255, 255, 255), "Press Up or Down to change animation speed.");

        // Perform the buffer flip after waiting for vertical refresh
        video_display_on_vblank();

        anim_time++;
    }
}

void test()
{
    video_init(VIDEO_COLOR_8888);
    while(1)
    {
        video_fill_screen(rgb(48, 48, 48));
        video_draw_debug_text((video_width() / 2) - (8 * 7), video_height() / 2, rgb(255, 255, 255), "Hello, world!");
        video_display_on_vblank();
    }
    
}