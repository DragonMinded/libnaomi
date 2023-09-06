#ifndef PALETTE_ANIMATION_H
#define PALETTE_ANIMATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>

#include <naomi/color.h>
#include <naomi/system.h>
#include <naomi/ta.h>

// ta_palette_animate_forward and backward are example functions
// demonstrating how to perform pallete animation.
// Each invocation of these functions will shift the specified 
// palette bank entries by one index (either forward or backward).

// palette_size must be a palette type (TA_PALETTE_CLUT4 or TA_PALETTE_CLUT8)
// bank_number must be a palette bank index
void ta_palette_animate_forward(int palette_size, int bank_number);
void ta_palette_animate_backward(int palette_size, int bank_number);


// ta_subpalette_animate_forward and backward are example functions
// demonstrating the ability to animate only a sub-section of a palette.

// Note that start_index + count must be <= the given palette size 
// 0...15 for TA_PALETTE_CLUT4, 0...255 for TA_PALETTE_CLUT8
void ta_subpalette_animate_forward(int palette_size, int bank_number, uint_fast8_t start_index, uint_fast8_t count);
void ta_subpalette_animate_backward(int palette_size, int bank_number, uint_fast8_t start_index, uint_fast8_t count);

#ifdef __cplusplus
}
#endif

#endif