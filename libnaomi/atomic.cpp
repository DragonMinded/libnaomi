#include <naomi/interrupt.h>

typedef int _Atomic_word;

namespace __gnu_cxx
{
    _Atomic_word __exchange_and_add(volatile _Atomic_word* __mem, int __val)
    {
        uint32_t old_irq = irq_disable();
        _Atomic_word old = *__mem;
        *__mem += __val;
        irq_restore(old_irq);
        return old;
    }

    void __atomic_add(volatile _Atomic_word* __mem, int __val)
    {
        __exchange_and_add(__mem, __val);
    }
};
