/* libgodc/runtime/go-memequal.c - memory comparison */

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

_Bool runtime_memequal(const void *restrict p, const void *restrict q, uintptr_t size) __asm__("_runtime.memequal");
_Bool runtime_memequal8_f(const void *restrict p, const void *restrict q) __asm__("_runtime.memequal8..f");
_Bool runtime_memequal16_f(const void *restrict p, const void *restrict q) __asm__("_runtime.memequal16..f");
_Bool runtime_memequal32_f(const void *restrict p, const void *restrict q) __asm__("_runtime.memequal32..f");
_Bool runtime_memequal64_f(const void *restrict p, const void *restrict q) __asm__("_runtime.memequal64..f");
_Bool runtime_memequal128_f(const void *restrict p, const void *restrict q) __asm__("_runtime.memequal128..f");

_Bool runtime_memequal(const void *restrict p, const void *restrict q, uintptr_t size)
{
    return memcmp(p, q, size) == 0;
}

_Bool runtime_memequal8(const void *restrict p, const void *restrict q)
{
    return *(const uint8_t *)p == *(const uint8_t *)q;
}

_Bool runtime_memequal8_f(const void *restrict p, const void *restrict q)
{
    return *(const uint8_t *)p == *(const uint8_t *)q;
}

_Bool runtime_memequal16(const void *restrict p, const void *restrict q)
{
    return *(const uint16_t *)p == *(const uint16_t *)q;
}

_Bool runtime_memequal16_f(const void *restrict p, const void *restrict q)
{
    return *(const uint16_t *)p == *(const uint16_t *)q;
}

_Bool runtime_memequal32(const void *restrict p, const void *restrict q)
{
    return *(const uint32_t *)p == *(const uint32_t *)q;
}

_Bool runtime_memequal32_f(const void *restrict p, const void *restrict q)
{
    return *(const uint32_t *)p == *(const uint32_t *)q;
}

_Bool runtime_memequal64(const void *restrict p, const void *restrict q)
{
    return *(const uint64_t *)p == *(const uint64_t *)q;
}

_Bool runtime_memequal64_f(const void *restrict p, const void *restrict q)
{
    return *(const uint64_t *)p == *(const uint64_t *)q;
}

_Bool runtime_memequal128(const void *restrict p, const void *restrict q)
{
    const uint64_t *a = (const uint64_t *)p;
    const uint64_t *b = (const uint64_t *)q;
    return a[0] == b[0] && a[1] == b[1];
}

_Bool runtime_memequal128_f(const void *restrict p, const void *restrict q)
{
    const uint64_t *a = (const uint64_t *)p;
    const uint64_t *b = (const uint64_t *)q;
    return a[0] == b[0] && a[1] == b[1];
}

void runtime_memequal_varlen(void *restrict result, uintptr_t size,
                             const void *restrict p, const void *restrict q)
{
    *(_Bool *)result = memcmp(p, q, size) == 0;
}
