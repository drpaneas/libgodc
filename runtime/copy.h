/* libgodc/runtime/copy.h - fast copy for small values */
#ifndef GODC_COPY_H
#define GODC_COPY_H

#include <stdint.h>
#include <string.h>
#include "runtime.h"

static inline void fast_copy(void *dst, const void *src, size_t size)
{
    uintptr_t d = (uintptr_t)dst;
    uintptr_t s = (uintptr_t)src;

    if (size == 0)
        return;

    switch (size)
    {
    case 1:
        *(uint8_t *)dst = *(const uint8_t *)src;
        return;
    case 2:
        if (likely(!((d | s) & 1)))
        {
            *(uint16_t *)dst = *(const uint16_t *)src;
            return;
        }
        break;
    case 4:
        if (likely(!((d | s) & 3)))
        {
            *(uint32_t *)dst = *(const uint32_t *)src;
            return;
        }
        break;
    case 8:
        /* SH-4 has no 64-bit load/store */
        if (likely(!((d | s) & 3)))
        {
            ((uint32_t *)dst)[0] = ((const uint32_t *)src)[0];
            ((uint32_t *)dst)[1] = ((const uint32_t *)src)[1];
            return;
        }
        break;
    }

    memcpy(dst, src, size);
}

#endif
