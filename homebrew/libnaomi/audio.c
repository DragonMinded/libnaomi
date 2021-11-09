#include <memory.h>
#include "naomi/audio.h"
#include "naomi/system.h"
#include "aica/common.h"
#include "irqinternal.h"

// Base registers for controlling the AICA.
#define AICA_BASE 0xA0700000

// Register offset definitions.
#define AICA_VERSION (0x2800 >> 2)
#define AICA_RESET (0x2C00 >> 2)

// Our command buffer for talking to the AICA.
#define AICA_CMD_BUFFER(x) *((volatile uint32_t *)((SOUNDRAM_BASE | UNCACHED_MIRROR) + 0x20000 + x))

// FIFO wait register, so we don't overrun SH-4/AICA FIFO
#define AICA_FIFO_STATUS (*((volatile uint32_t *)0xA05F688C))

// Whether we've initialized this module or not.
static int initialized = 0;

void __aica_fifo_wait()
{
    while ((AICA_FIFO_STATUS & 0x11) != 0) { ; }
}

void __aica_memcpy(void *dst, void *src, unsigned int length)
{
    if ((((uint32_t)dst) & 0x3) != 0)
    {
        _irq_display_invariant("invalid memcpy location", "dst %08lx is not aligned to 4-byte boundary", (uint32_t)dst);
    }
    if ((((uint32_t)src) & 0x3) != 0)
    {
        _irq_display_invariant("invalid memcpy location", "src %08lx is not aligned to 4-byte boundary", (uint32_t)src);
    }

    // Round up to next 4-byte boundary.
    length = (length + 3) & 0xFFFFFFFC;

    uint32_t *dstptr = (uint32_t *)dst;
    uint32_t *srcptr = (uint32_t *)src;
    uint32_t copyamount = 0;
    while (length > 0)
    {
        // Don't overrun the FIFO or we could get flaky transfers.
        if ((copyamount & 0x1F) == 0)
        {
            __aica_fifo_wait();
        }

        *dstptr = *srcptr;
        dstptr++;
        srcptr++;
        length-=4;
        copyamount+=4;
    }

    // Make sure we exit only when the FIFO is done.
    __aica_fifo_wait();
}

void load_aica_binary(void *binary, unsigned int length)
{
    // Note that for this to work without bugs or weirdness on the ARM side, the ARM
    // binary pointer should be 4-byte aligned, and the length should be a multiple
    // of 4. Any other configuration asks for undefined behavior.
    volatile unsigned int *aicabase = (volatile unsigned int *)AICA_BASE;

    // Set up 16-bit wave memory size.
    aicabase[AICA_VERSION] = 0x200;

    // Pull the AICA MCU in reset.
    aicabase[AICA_RESET] |= 0x1;

    // Copy the binary to the AICA MCU. Its safe to do this here since the AICA
    // is in reset, so there will be no G1 FIFO contention.
    __aica_memcpy((void *)(SOUNDRAM_BASE | UNCACHED_MIRROR), binary, length);

    // Pull the AICA MCU back out of reset.
    aicabase[AICA_RESET] &= ~0x1;
}

uint32_t audio_aica_uptime()
{
    // Make sure we have room in the FIFO to read the uptime.
    __aica_fifo_wait();

    // Return the uptime in milliseconds as written by the AICA binary.
    return AICA_CMD_BUFFER(CMD_BUFFER_UPTIME);
}

uint32_t __audio_exchange_command(uint32_t command, uint32_t param1, uint32_t param2, uint32_t param3, uint32_t param4)
{
    // Make sure we have room in the FIFO to read busy register.
    __aica_fifo_wait();

    // Wait for the AICA to be ready for a command.
    while (AICA_CMD_BUFFER(CMD_BUFFER_BUSY) != 0) { ; }

    // Make sure we have room in the FIFO to write the command.
    __aica_fifo_wait();

    // Set up the command and param registers.
    AICA_CMD_BUFFER(CMD_BUFFER_REQUEST) = command;
    AICA_CMD_BUFFER(CMD_BUFFER_PARAMS + 0x0) = param1;
    AICA_CMD_BUFFER(CMD_BUFFER_PARAMS + 0x4) = param2;
    AICA_CMD_BUFFER(CMD_BUFFER_PARAMS + 0x8) = param3;
    AICA_CMD_BUFFER(CMD_BUFFER_PARAMS + 0xC) = param4;

    // Trigger the AICA to react to the command.
    AICA_CMD_BUFFER(CMD_BUFFER_BUSY) = 1;

    // Make sure we have room in the FIFO to wait for done.
    __aica_fifo_wait();

    // Wait for the AICA to finish the command.
    while (AICA_CMD_BUFFER(CMD_BUFFER_BUSY) != 0) { ; }

    // Make sure we have room in the FIFO to read the response.
    __aica_fifo_wait();

    // Return the response.
    return AICA_CMD_BUFFER(CMD_BUFFER_RESPONSE);
}

void audio_init()
{
    if (!initialized)
    {
        // Load up our binary so we can talk to it.
        load_aica_binary(AICA_DEFAULT_BINARY, AICA_DEFAULT_BINARY_SIZE);
        initialized = 1;
    }
}

void audio_free()
{
    if (initialized)
    {
        // Kill all running sounds.
        __audio_exchange_command(REQUEST_SILENCE, 0, 0, 0, 0);
        initialized = 0;
    }
}

uint32_t __audio_get_location(int format, unsigned int samplerate, unsigned int num_samples)
{
    return __audio_exchange_command(
        REQUEST_ALLOCATE,
        num_samples,
        format == AUDIO_FORMAT_16BIT ? ALLOCATE_AUDIO_FORMAT_16BIT : ALLOCATE_AUDIO_FORMAT_8BIT,
        samplerate,
        0
    );
}

uint32_t __audio_load_location(int format, unsigned int samplerate, void *data, unsigned int num_samples)
{
    uint32_t location = __audio_get_location(format, samplerate, num_samples);
    uint32_t size = (format == AUDIO_FORMAT_16BIT) ? (num_samples * 2) : num_samples;

    if (location != 0)
    {
        __aica_memcpy((void *)((SOUNDRAM_BASE | UNCACHED_MIRROR) + location), data, size);
    }

    return location;
}

int audio_play_sound(int format, unsigned int samplerate, uint32_t speakers, void *data, unsigned int num_samples)
{
    uint32_t location = __audio_load_location(format, samplerate, data, num_samples);
    if (location != 0)
    {
        uint32_t panning = 0;
        if (speakers & SPEAKER_LEFT)
        {
            panning |= ALLOCATE_SPEAKER_LEFT;
        }
        if (speakers & SPEAKER_RIGHT)
        {
            panning |= ALLOCATE_SPEAKER_RIGHT;
        }

        if (__audio_exchange_command(REQUEST_DISCARD_AFTER_USE, location, 0, 0, 0) != RESPONSE_SUCCESS)
        {
            return -1;
        }

        // Now, play the track.
        return __audio_exchange_command(REQUEST_START_PLAY, location, panning, 0, 0) == RESPONSE_SUCCESS ? 0 : -1;
    }
    else
    {
        return -1;
    }
}

int audio_register_sound(int format, unsigned int samplerate, void *data, unsigned int num_samples)
{
    uint32_t location = __audio_load_location(format, samplerate, data, num_samples);

    if (location)
    {
        return location;
    }
    else
    {
        return -1;
    }
}

void audio_unregister_sound(int sound)
{
    if (sound > 0)
    {
        __audio_exchange_command(REQUEST_FREE, sound, 0, 0, 0);
    }
}

int audio_play_registered_sound(int sound, uint32_t speakers)
{
    if (sound > 0)
    {
        uint32_t panning = 0;
        if (speakers & SPEAKER_LEFT)
        {
            panning |= ALLOCATE_SPEAKER_LEFT;
        }
        if (speakers & SPEAKER_RIGHT)
        {
            panning |= ALLOCATE_SPEAKER_RIGHT;
        }

        return __audio_exchange_command(REQUEST_START_PLAY, sound, panning, 0, 0) == RESPONSE_SUCCESS ? 0 : -1;
    }
    else
    {
        return -1;
    }
}
