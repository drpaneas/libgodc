#include "runtime.h"
#include "gc_semispace.h"
#include "type_descriptors.h"
#include "map_dreamcast.h"
#include "godc_config.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

extern void stack_pool_preallocate(void);

#define gc_alloc_impl(size, type) gc_alloc(size, type)
#define gc_init_impl() gc_init()
#define gc_collect_impl() gc_collect()
#define gc_full_collect_impl() gc_collect()
#define gc_collect_if_needed_impl(sz) gc_collect_if_needed(sz)

#define DC_MAX_ALLOC_SIZE (8 * 1024 * 1024)

void *runtime_newobject(struct __go_type_descriptor *type) __asm__("_runtime.newobject");
void *runtime_newobject(struct __go_type_descriptor *type)
{
    if (!type)
    {
        LIBGODC_ERROR("newobject: NULL type");
        return NULL;
    }

    size_t size = type->__size;
    if (size == 0)
    {
        extern uint8_t gc_zerobase[];
        return gc_zerobase;
    }

    return gc_alloc_impl(size, type);
}

void *runtime_mallocgc(size_t size, struct __go_type_descriptor *type, bool needzero)
{
    if (size > DC_MAX_ALLOC_SIZE)
    {
        runtime_panicstring("mallocgc: too large");
        return NULL;
    }

    (void)needzero;
    return gc_alloc_impl(size, type);
}

void *_runtime_mallocgc(uintptr_t size, struct __go_type_descriptor *type, int needzero)
{
    return runtime_mallocgc(size, type, needzero != 0);
}

void *runtime_makeslice(struct __go_type_descriptor *elem_type, intptr_t len, intptr_t cap) __asm__("_runtime.makeslice");
void *runtime_makeslice(struct __go_type_descriptor *elem_type, intptr_t len, intptr_t cap)
{
    if (!elem_type)
        return NULL;

    if (len < 0)
        runtime_panicstring("makeslice: len out of range");
    if (cap < 0)
        runtime_panicstring("makeslice: cap out of range");
    if (len > cap)
        runtime_panicstring("makeslice: len > cap");

    size_t elem_size = elem_type->__size;
    if (elem_size == 0)
        elem_size = 1;

    if (cap > 0 && elem_size > SIZE_MAX / (size_t)cap)
        runtime_panicstring("makeslice: cap overflow");

    size_t total_size = elem_size * cap;

    if (total_size > DC_MAX_ALLOC_SIZE)
        runtime_panicstring("makeslice: too large");

    if (cap == 0)
    {
        static uintptr_t zerobase = 0;
        return &zerobase;
    }

    return gc_alloc_impl(total_size, elem_type);
}

void *runtime_makeslice64(struct __go_type_descriptor *elem_type, int64_t len, int64_t cap) __asm__("_runtime.makeslice64");
void *runtime_makeslice64(struct __go_type_descriptor *elem_type, int64_t len, int64_t cap)
{
    if (len > INTPTR_MAX || cap > INTPTR_MAX)
        runtime_panicstring("makeslice: size out of range");
    return runtime_makeslice(elem_type, (intptr_t)len, (intptr_t)cap);
}

uintptr_t runtime_checkMakeSlice(struct __go_type_descriptor *t, intptr_t len, intptr_t cap) __asm__("_runtime.checkMakeSlice");
uintptr_t runtime_checkMakeSlice(struct __go_type_descriptor *t, intptr_t len, intptr_t cap)
{
    if (len < 0)
        runtime_panicstring("makeslice: len out of range");
    if (cap < 0)
        runtime_panicstring("makeslice: cap out of range");
    if (len > cap)
        runtime_panicstring("makeslice: len > cap");

    size_t elem_size = t ? t->__size : 1;
    if (cap > 0 && elem_size > SIZE_MAX / (size_t)cap)
        runtime_panicstring("makeslice: cap overflow");

    return (uintptr_t)(elem_size * cap);
}

int runtime_typedslicecopy(struct __go_type_descriptor *t,
                           void *dst, int dstLen,
                           void *src, int srcLen) __asm__("_runtime.typedslicecopy");
int runtime_typedslicecopy(struct __go_type_descriptor *t,
                           void *dst, int dstLen,
                           void *src, int srcLen)
{
    if (dstLen == 0 || srcLen == 0 || !dst || !src)
        return 0;

    int n = (dstLen < srcLen) ? dstLen : srcLen;
    size_t elem_size = t ? t->__size : 1;

    if (elem_size > 0)
        memmove(dst, src, (size_t)n * elem_size);
    return n;
}

void runtime_unsafeslice(struct __go_type_descriptor *t, void *ptr, intptr_t len) __asm__("_runtime.unsafeslice");
void runtime_unsafeslice(struct __go_type_descriptor *t, void *ptr, intptr_t len)
{
    if (len < 0)
        runtime_panicstring("unsafe.Slice: len out of range");
    if (!ptr && len > 0)
        runtime_panicstring("unsafe.Slice: nil ptr with len > 0");

    size_t size = t ? t->__size : 1;
    if (size > 0 && len > 0 && (size_t)len > SIZE_MAX / size)
        runtime_panicstring("unsafe.Slice: overflow");
}

void runtime_unsafeslice64(struct __go_type_descriptor *t, void *ptr, int64_t len) __asm__("_runtime.unsafeslice64");
void runtime_unsafeslice64(struct __go_type_descriptor *t, void *ptr, int64_t len)
{
    if (len > INTPTR_MAX)
        runtime_panicstring("unsafe.Slice: len out of range");
    runtime_unsafeslice(t, ptr, (intptr_t)len);
}

/* GC trigger threshold as percentage of heap size.
 * 100 = collect when heap is full (default)
 * 50 = collect when heap is 50% full (more frequent, shorter pauses)
 * -1 = disable automatic GC (only explicit runtime.GC() triggers collection)
 */
#ifdef GODC_DEFAULT_GC_PERCENT
int32_t gc_percent = GODC_DEFAULT_GC_PERCENT;
#else
int32_t gc_percent = 100;
#endif

void runtime_GC(void) __asm__("_runtime.GC");
void runtime_GC(void)
{
    gc_full_collect_impl();
}

int32_t debug_SetGCPercent(int32_t percent) __asm__("debug.SetGCPercent");
int32_t debug_SetGCPercent(int32_t percent)
{
    int32_t old = gc_percent;
    gc_percent = percent;
    return old;
}

extern void proc_init(void);
extern void sudog_pool_init(void);

__attribute__((no_split_stack)) void runtime_init(void)
{
    static bool initialized = false;
    if (initialized)
        return;

    gc_init_impl();
    map_init();
    sudog_pool_init();
    stack_pool_preallocate();
    proc_init();
    panic_init();
    initialized = true;
}

void *runtime_malloc(size_t size)
{
    return gc_alloc_impl(size, NULL);
}
