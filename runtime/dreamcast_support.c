/* libgodc/runtime/dreamcast_support.c - atomic fallbacks for SH-4 */

#include <stdint.h>
#include <kos.h>

#define WEAK_ATOMIC __attribute__((weak))

/* 32-bit atomics */

WEAK_ATOMIC
unsigned int __atomic_load_4(const volatile void *ptr, int memorder)
{
    (void)memorder;
    int irq = irq_disable();
    unsigned int val = *(const volatile unsigned int *)ptr;
    irq_restore(irq);
    return val;
}

WEAK_ATOMIC
void __atomic_store_4(volatile void *ptr, unsigned int val, int memorder)
{
    (void)memorder;
    int irq = irq_disable();
    *(volatile unsigned int *)ptr = val;
    irq_restore(irq);
}

WEAK_ATOMIC
unsigned int __atomic_exchange_4(volatile void *ptr, unsigned int val, int memorder)
{
    (void)memorder;
    int irq = irq_disable();
    unsigned int old = *(volatile unsigned int *)ptr;
    *(volatile unsigned int *)ptr = val;
    irq_restore(irq);
    return old;
}

WEAK_ATOMIC
_Bool __atomic_compare_exchange_4(volatile void *ptr, void *expected,
                                  unsigned int desired, _Bool weak,
                                  int success_memorder, int failure_memorder)
{
    (void)weak;
    (void)success_memorder;
    (void)failure_memorder;

    int irq = irq_disable();
    unsigned int current = *(volatile unsigned int *)ptr;
    unsigned int exp = *(unsigned int *)expected;
    _Bool result;

    if (current == exp)
    {
        *(volatile unsigned int *)ptr = desired;
        result = 1;
    }
    else
    {
        *(unsigned int *)expected = current;
        result = 0;
    }

    irq_restore(irq);
    return result;
}

WEAK_ATOMIC
unsigned int __atomic_fetch_add_4(volatile void *ptr, unsigned int val, int memorder)
{
    (void)memorder;
    int irq = irq_disable();
    unsigned int old = *(volatile unsigned int *)ptr;
    *(volatile unsigned int *)ptr = old + val;
    irq_restore(irq);
    return old;
}

WEAK_ATOMIC
unsigned int __atomic_fetch_sub_4(volatile void *ptr, unsigned int val, int memorder)
{
    (void)memorder;
    int irq = irq_disable();
    unsigned int old = *(volatile unsigned int *)ptr;
    *(volatile unsigned int *)ptr = old - val;
    irq_restore(irq);
    return old;
}

WEAK_ATOMIC
unsigned int __atomic_fetch_and_4(volatile void *ptr, unsigned int val, int memorder)
{
    (void)memorder;
    int irq = irq_disable();
    unsigned int old = *(volatile unsigned int *)ptr;
    *(volatile unsigned int *)ptr = old & val;
    irq_restore(irq);
    return old;
}

WEAK_ATOMIC
unsigned int __atomic_fetch_or_4(volatile void *ptr, unsigned int val, int memorder)
{
    (void)memorder;
    int irq = irq_disable();
    unsigned int old = *(volatile unsigned int *)ptr;
    *(volatile unsigned int *)ptr = old | val;
    irq_restore(irq);
    return old;
}

WEAK_ATOMIC
unsigned int __atomic_fetch_xor_4(volatile void *ptr, unsigned int val, int memorder)
{
    (void)memorder;
    int irq = irq_disable();
    unsigned int old = *(volatile unsigned int *)ptr;
    *(volatile unsigned int *)ptr = old ^ val;
    irq_restore(irq);
    return old;
}

/* 8-bit and 16-bit atomics */

WEAK_ATOMIC
unsigned char __atomic_load_1(const volatile void *ptr, int memorder)
{
    (void)memorder;
    int irq = irq_disable();
    unsigned char val = *(const volatile unsigned char *)ptr;
    irq_restore(irq);
    return val;
}

WEAK_ATOMIC
void __atomic_store_1(volatile void *ptr, unsigned char val, int memorder)
{
    (void)memorder;
    int irq = irq_disable();
    *(volatile unsigned char *)ptr = val;
    irq_restore(irq);
}

WEAK_ATOMIC
unsigned char __atomic_exchange_1(volatile void *ptr, unsigned char val, int memorder)
{
    (void)memorder;
    int irq = irq_disable();
    unsigned char old = *(volatile unsigned char *)ptr;
    *(volatile unsigned char *)ptr = val;
    irq_restore(irq);
    return old;
}

WEAK_ATOMIC
unsigned short __atomic_load_2(const volatile void *ptr, int memorder)
{
    (void)memorder;
    int irq = irq_disable();
    unsigned short val = *(const volatile unsigned short *)ptr;
    irq_restore(irq);
    return val;
}

WEAK_ATOMIC
void __atomic_store_2(volatile void *ptr, unsigned short val, int memorder)
{
    (void)memorder;
    int irq = irq_disable();
    *(volatile unsigned short *)ptr = val;
    irq_restore(irq);
}

WEAK_ATOMIC
unsigned short __atomic_exchange_2(volatile void *ptr, unsigned short val, int memorder)
{
    (void)memorder;
    int irq = irq_disable();
    unsigned short old = *(volatile unsigned short *)ptr;
    *(volatile unsigned short *)ptr = val;
    irq_restore(irq);
    return old;
}
