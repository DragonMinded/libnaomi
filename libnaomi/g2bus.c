#include "naomi/thread.h"

/* Global hardware access mutexes. */
mutex_t g2bus_mutex;

void _g2bus_init()
{
    // Alow maple and audio exclusive access to the hardware.
    mutex_init(&g2bus_mutex);
}

void _g2bus_free()
{
    // Do the reverse of the above init.
    mutex_free(&g2bus_mutex);
}
