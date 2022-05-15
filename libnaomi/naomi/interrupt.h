#ifndef __INTERRUPT_H
#define __INTERRUPT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Ensure interrupts are disabled, returning the old SR. When you are done with
// the code that needs exclusive HW access, restore interrupts with irq_restore().
// Note that under most circumstances you should not use this in your code. It is
// much safer to instead guard code with a mutex. Using irq_disable()/irq_restore()
// without extreme care can cause system code which might need to make syscalls to
// crash.
uint32_t irq_disable();

// Restore interrupts, after calling irq_disable(). Note that you should always have
// an irq_restore() call matching an irq_disable() call, otherwise you will leave
// interrupts disabled permanently and many parts of the system will no longer work.
void irq_restore(uint32_t oldstate);

// Run an enclosed statement or group of statements atomically. The most common place
// where you might use this is for wrapping C stdlib calls such as sprintf in order
// to make them threadsafe with respect to common buffers. In most circumstances you
// should probably use a mutex instead, but this handy macro can save you a bit of
// setup and teardown. Note that you can include a block of statements by replacing
// the single statement by angle brackets. If you do this, do note that return
// statements inside that code are not safe! This appears to work similar to a
// resource guard in modern languages, but C doesn't have finally support!
#define ATOMIC(stmt) \
do { \
    uint32_t old_ints = irq_disable(); \
    stmt; \
    irq_restore(old_ints); \
} while( 0 )

// Statistics about interrupts on the system.
typedef struct
{
    // The last interrupt source.
    uint32_t last_source;

    // The last interrupt event.
    uint32_t last_event;

    // The number of interrupts the system has seen.
    uint32_t num_interrupts;
} irq_stats_t;

#define IRQ_SOURCE_GENERAL_EXCEPTION 0x100
#define IRQ_SOURCE_TLB_EXCEPTION 0x400
#define IRQ_SOURCE_INTERRUPT 0x600

#define IRQ_EVENT_MEMORY_READ_ERROR 0x0E0
#define IRQ_EVENT_MEMORY_WRITE_ERROR 0x100
#define IRQ_EVENT_FPU_EXCEPTION 0x120
#define IRQ_EVENT_TRAPA 0x160
#define IRQ_EVENT_ILLEGAL_INSTRUCTION 0x180
#define IRQ_EVENT_ILLEGAL_SLOT_INSTRUCTION 0x1A0
#define IRQ_EVENT_NMI 0x1C0
#define IRQ_EVENT_HOLLY_LEVEL6 0x0320
#define IRQ_EVENT_HOLLY_LEVEL4 0x0360
#define IRQ_EVENT_HOLLY_LEVEL2 0x03A0
#define IRQ_EVENT_TMU0 0x400
#define IRQ_EVENT_TMU1 0x420
#define IRQ_EVENT_TMU2 0x440

// Grab interrupt statistics. This is mostly for debugging and doesn't contain the level
// of detail necessary to make much use of it.
irq_stats_t irq_stats();

#ifdef __cplusplus
}
#endif

#endif
