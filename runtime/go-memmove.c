/* libgodc/runtime/go-memmove.c - memory move */

#include <string.h>
#include <stdint.h>
#include "type_descriptors.h"

void runtime_memmove(void *dst, void *src, uintptr_t n)
{
    if (dst != src && n > 0)
        memmove(dst, src, n);
}

void runtime_typedmemmove(void *typ, void *dst, void *src) __asm__("_runtime.typedmemmove");
void runtime_typedmemmove(void *typ, void *dst, void *src)
{
    if (!typ || !dst || !src)
        return;

    struct __go_type_descriptor *td = (struct __go_type_descriptor *)typ;
    uintptr_t size = td->__size;
    if (size > 0)
        runtime_memmove(dst, src, size);
}

void __go_memmove(void *dst, void *src, uintptr_t n) __attribute__((alias("runtime_memmove")));
