/* libgodc/runtime/go-unsafe-pointer.c - unsafe pointer ops */

#include <stdint.h>

uintptr_t runtime_unsafeptr2uintptr(void *p) { return (uintptr_t)p; }
void *runtime_uintptr2unsafeptr(uintptr_t u) { return (void *)u; }
void *runtime_unsafeptr_add(void *p, intptr_t offset) { return (void *)((uintptr_t)p + offset); }

uintptr_t __go_type_size(void *type)
{
    if (!type) return 0;
    return *(uintptr_t *)type;
}

uintptr_t __go_type_align(void *type)
{
    if (!type) return 1;
    return 4;
}
