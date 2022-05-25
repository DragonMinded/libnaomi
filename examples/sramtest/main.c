#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <naomi/video.h>
#include <naomi/sramfs/sramfs.h>

void main()
{
    // We just want a simple framebuffer display.
    video_init(VIDEO_COLOR_1555);
    video_set_background_color(rgb(48, 48, 48));

    // Initialize SRAM FS hooks.
    sramfs_init_default();

    // Load stats about our last boot.
    int boot_count = 0;
    char *boot_time = 0;

    FILE *fp = fopen("sram://boot_count", "w+");
    if (fp)
    {
        if (fread(&boot_count, sizeof(boot_count), 1, fp) != sizeof(boot_count))
        {
            boot_count = 0;
        }

        fseek(fp, 0, SEEK_SET);
        boot_count++;
        fwrite(&boot_count, sizeof(boot_count), 1, fp);
    }
    fclose(fp);

    fp = fopen("sram://boot_time", "w+");
    if (fp)
    {
        fseek(fp, 0, SEEK_END);
        int size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        if (size > 0)
        {
            boot_time = malloc(size);
            if (fread(boot_time, 1, size, fp) != size)
            {
                free(boot_time);
                boot_time = 0;
            }
        }

        fseek(fp, 0, SEEK_SET);
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        char timebuf[1024];
        sprintf(timebuf, "%d-%02d-%02d %02d:%02d:%02d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        fwrite(timebuf, 1, strlen(timebuf) + 1, fp);
    }
    fclose(fp);

    uint32_t counter = 0;
    while ( 1 )
    {
#ifndef FEATURE_LITTLEFS
        // Nothing really to do here...
        video_draw_debug_text(100, 180, rgb(255, 255, 255), "libnaomi not compiled with littlefs!");
#else
        // Display stats about how many boots.
        video_draw_debug_text(100, 180, rgb(255, 255, 255), "This example has been booted %d time(s) in a row!", boot_count);
        if (boot_time)
        {
            video_draw_debug_text(100, 200, rgb(255, 255, 255), "The previous boot was at %s!", boot_time);
        }
        else
        {
            video_draw_debug_text(100, 200, rgb(255, 255, 255), "We were never previously booted!");
        }
#endif

        // Display a liveness counter that goes up 60 times a second.
        video_draw_debug_text(100, 260, rgb(200, 200, 20), "Aliveness counter: %d", counter++);

        // Actually draw the framebuffer.
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
