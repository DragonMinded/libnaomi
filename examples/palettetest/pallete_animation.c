#include "pallete_animation.h"

// ta_palette_animate_forward and backward are example functions
// demonstrating how to perform pallete animation.
// It is probably possible to optimize these functions further 
// (EG: with memmove()).
void ta_palette_animate_forward(int palette_size, int bank_number)
{
    uint32_t *palette = ta_palette_bank(palette_size, bank_number);

    uint_fast8_t last_index;
    if(palette_size == TA_PALETTE_CLUT8)
        last_index = 255;
    else
        last_index = 15;

    uint32_t pop = palette[last_index];
    for(uint_fast8_t i = last_index; i >= 1; i--)
    {
        palette[i] = palette[i-1];
    }
    // memmove(&palette[1], &palette[0], (last_index * 4));    // This doesn't work?
    palette[0] = pop;
}

void ta_palette_animate_backward(int palette_size, int bank_number)
{
    uint32_t *palette = ta_palette_bank(palette_size, bank_number);

    uint_fast8_t last_index;
    if(palette_size == TA_PALETTE_CLUT8)
        last_index = 255;
    else
        last_index = 15;

    uint32_t pop = palette[0];
    for(uint_fast8_t i = 0; i < last_index; i++)
    {
        palette[i] = palette[i+1];
    }
    // memmove(&palette[0], &palette[1], (last_index * 4));    // This works
    palette[last_index] = pop;
}

// ta_subpalette_animate_forward and backward are example functions
// demonstrating the ability to animate only a sub-section of a palette.
// Again, it is probably possible to optimize these functions.

// Note that start_index + count must be <= the given palette size 
// 0...15 for TA_PALETTE_CLUT4, 0...255 for TA_PALETTE_CLUT8
void ta_subpalette_animate_forward(int palette_size, int bank_number, uint_fast8_t start_index, uint_fast8_t count)
{
    uint32_t *palette = ta_palette_bank(palette_size, bank_number);

    uint_fast8_t last_index;
    if(palette_size == TA_PALETTE_CLUT8)
    {
        if(255 - count < start_index)
            return;
        last_index = start_index + count;
    } else {
        last_index = start_index + count;
        if(last_index > 15) 
            return;
    }

    uint32_t pop = palette[last_index];
    for(uint_fast8_t i = last_index; i >= start_index + 1; i--)
    {
        palette[i] = palette[i-1];
    }
    palette[start_index] = pop;
}

// Note that start_index + count must be <= the given palette size 
// 0...15 for TA_PALETTE_CLUT4, 0...255 for TA_PALETTE_CLUT8
void ta_subpalette_animate_backward(int palette_size, int bank_number, uint_fast8_t start_index, uint_fast8_t count)
{
    uint32_t *palette = ta_palette_bank(palette_size, bank_number);

    uint_fast8_t last_index;
    if(palette_size == TA_PALETTE_CLUT8)
    {
        if(255 - count < start_index)
            return;
        last_index = start_index + count;
    } else {
        last_index = start_index + count;
        if(last_index > 15) 
            return;
    }

    uint32_t pop = palette[start_index];
    for(uint_fast8_t i = start_index; i < last_index; i++)
    {
        palette[i] = palette[i+1];
    }
    palette[last_index] = pop;
}