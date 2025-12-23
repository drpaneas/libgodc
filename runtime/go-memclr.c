/* libgodc/runtime/go-memclr.c - memory clear */

#include <string.h>
#include <stdint.h>

void runtime_memclr(void *restrict ptr, uintptr_t n) __asm__("_runtime.memclr");
void runtime_memclr(void *restrict ptr, uintptr_t n)
{
    memset(ptr, 0, n);
}

void runtime_memclrNoHeapPointers(void *restrict ptr, uintptr_t n) __asm__("_runtime.memclrNoHeapPointers");
void runtime_memclrNoHeapPointers(void *restrict ptr, uintptr_t n)
{
    memset(ptr, 0, n);
}

void runtime_memclrHasPointers(void *restrict ptr, uintptr_t n) __asm__("_runtime.memclrHasPointers");
void runtime_memclrHasPointers(void *restrict ptr, uintptr_t n)
{
    memset(ptr, 0, n);
}

void __go_memclr(void *restrict ptr, uintptr_t n)
{
    memset(ptr, 0, n);
}
