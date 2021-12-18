#include <stdio.h>
#include <stdint.h>
#include <naomi/video.h>
#include <naomi/audio.h>
#include <naomi/maple.h>
#include <naomi/eeprom.h>

// Our sound, as linked by our makefile.
extern uint8_t *success_raw_data;
extern unsigned int success_raw_len;

extern uint8_t *one_raw_data;
extern unsigned int one_raw_len;

extern uint8_t *two_raw_data;
extern unsigned int two_raw_len;

extern uint8_t *three_raw_data;
extern unsigned int three_raw_len;

extern uint8_t *four_raw_data;
extern unsigned int four_raw_len;

extern uint8_t *five_raw_data;
extern unsigned int five_raw_len;

extern uint8_t *six_raw_data;
extern unsigned int six_raw_len;

extern uint8_t *doit_raw_data;
extern unsigned int doit_raw_len;

extern uint8_t *doot_raw_data;
extern unsigned int doot_raw_len;

void main()
{
    // Get settings so we know how many controls to read.
    eeprom_t settings;
    eeprom_read(&settings);

    // Initialize some crappy video.
    video_init(VIDEO_COLOR_1555);
    video_set_background_color(rgb(48, 48, 48));

    // Display status, since loading the binary can take awhile.
    video_draw_debug_text(20, 20, rgb(255, 255, 255), "Loading AICA binary...");
    video_display_on_vblank();

    // Initialize audio system.
    audio_init();

    // Register sounds for playback on keypress.
    int countsounds[7] = { 0 };
    countsounds[1] = audio_register_sound(AUDIO_FORMAT_16BIT, 44100, one_raw_data, one_raw_len / 2);
    countsounds[2] = audio_register_sound(AUDIO_FORMAT_16BIT, 44100, two_raw_data, two_raw_len / 2);
    countsounds[3] = audio_register_sound(AUDIO_FORMAT_16BIT, 44100, three_raw_data, three_raw_len / 2);
    countsounds[4] = audio_register_sound(AUDIO_FORMAT_16BIT, 44100, four_raw_data, four_raw_len / 2);
    countsounds[5] = audio_register_sound(AUDIO_FORMAT_16BIT, 44100, five_raw_data, five_raw_len / 2);
    countsounds[6] = audio_register_sound(AUDIO_FORMAT_16BIT, 44100, six_raw_data, six_raw_len / 2);

    int doit = audio_register_sound(AUDIO_FORMAT_16BIT, 44100, doit_raw_data, doit_raw_len / 2);

    // Register an annoying looping sound when you hold service.
    int doot = audio_register_sound(AUDIO_FORMAT_16BIT, 44100, doot_raw_data, doot_raw_len / 2);
    audio_set_registered_sound_loop(doot, 0);

    // Request a sound be played immediately.
    audio_play_sound(AUDIO_FORMAT_8BIT, 44100, SPEAKER_LEFT | SPEAKER_RIGHT, 1.00, success_raw_data, success_raw_len);

    unsigned int counter = 0;
    while ( 1 )
    {
        // Display instructions.
        video_draw_debug_text(20, 20, rgb(255, 255, 255), "Press buttons to activate sounds!");

        // Grab inputs.
        maple_poll_buttons();
        jvs_buttons_t pressed = maple_buttons_pressed();
        jvs_buttons_t held = maple_buttons_held();
        jvs_buttons_t released = maple_buttons_released();

        // Figure out panning based on L/R pressed on joysticks.
        unsigned int panning = SPEAKER_LEFT | SPEAKER_RIGHT;
        float volume = 0.90;
        if (held.player1.left || (settings.system.players >= 2 && held.player2.left))
        {
            panning = SPEAKER_LEFT;
        }
        else if (held.player1.right || (settings.system.players >= 2 && held.player2.right))
        {
            panning = SPEAKER_RIGHT;
        }
        if (held.player1.up || (settings.system.players >= 2 && held.player2.up))
        {
            volume = 1.00;
        }
        else if (held.player1.down || (settings.system.players >= 2 && held.player2.down))
        {
            volume = 0.80;
        }

        if (pressed.player1.start || (settings.system.players >= 2 && pressed.player2.start))
        {
            audio_play_registered_sound(doit, panning, volume);
        }
        if (pressed.player1.button1 || (settings.system.players >= 2 && pressed.player2.button1))
        {
            audio_play_registered_sound(countsounds[1], panning, volume);
        }
        if (pressed.player1.button2 || (settings.system.players >= 2 && pressed.player2.button2))
        {
            audio_play_registered_sound(countsounds[2], panning, volume);
        }
        if (pressed.player1.button3 || (settings.system.players >= 2 && pressed.player2.button3))
        {
            audio_play_registered_sound(countsounds[3], panning, volume);
        }
        if (pressed.player1.button4 || (settings.system.players >= 2 && pressed.player2.button4))
        {
            audio_play_registered_sound(countsounds[4], panning, volume);
        }
        if (pressed.player1.button5 || (settings.system.players >= 2 && pressed.player2.button5))
        {
            audio_play_registered_sound(countsounds[5], panning, volume);
        }
        if (pressed.player1.button6 || (settings.system.players >= 2 && pressed.player2.button6))
        {
            audio_play_registered_sound(countsounds[6], panning, volume);
        }

        if (pressed.player1.service || (settings.system.players >= 2 && pressed.player2.service))
        {
            audio_play_registered_sound(doot, SPEAKER_LEFT | SPEAKER_RIGHT, 1.0);
        }
        if (released.player1.service || (settings.system.players >= 2 && released.player2.service))
        {
            audio_stop_registered_sound(doot);
        }

        // Display a liveness counter that goes up 60 times a second.
        video_draw_debug_text(
            20,
            40,
            rgb(200, 200, 20),
            "Aliveness counter: %d (%lu)",
            counter++,
            audio_aica_uptime()
        );
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
