#include "gc_semispace.h"
#include "type_descriptors.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <malloc.h>
#include <arch/cache.h>
#include <kos/dbglog.h>

gc_heap_t gc_heap = {
    .space = {NULL, NULL},
    .active_space = 0,
    .alloc_ptr = NULL,
    .alloc_limit = NULL,
    .scan_ptr = NULL,
    .space_size = GC_SEMISPACE_SIZE,
    .bytes_allocated = 0,
    .total_bytes_allocated = 0,
    .total_alloc_count = 0,
    .bytes_copied = 0,
    .gc_count = 0,
    .last_pause_us = 0,
    .total_pause_us = 0,
    .initialized = false,
    .gc_in_progress = false};

gc_roots_t gc_root_table = {.roots = {NULL}, .count = 0};
gc_root_list_t *gc_global_roots = NULL;
uint8_t gc_zerobase[8] __attribute__((aligned(8))) = {0};

void gc_init(void)
{
    if (gc_heap.initialized)
        return;

    gc_heap.space[0] = (uint8_t *)memalign(32, GC_SEMISPACE_SIZE);
    gc_heap.space[1] = (uint8_t *)memalign(32, GC_SEMISPACE_SIZE);

    if (!gc_heap.space[0] || !gc_heap.space[1])
    {
        if (gc_heap.space[0])
            free(gc_heap.space[0]);
        if (gc_heap.space[1])
            free(gc_heap.space[1]);
        gc_heap.space[0] = NULL;
        gc_heap.space[1] = NULL;
        runtime_throw("gc_init: alloc failed");
    }

    memset(gc_heap.space[0], 0, GC_SEMISPACE_SIZE);
    memset(gc_heap.space[1], 0, GC_SEMISPACE_SIZE);

    gc_heap.active_space = 0;
    gc_heap.alloc_ptr = gc_heap.space[0];
    gc_heap.alloc_limit = gc_heap.space[0] + GC_SEMISPACE_SIZE;
    gc_heap.scan_ptr = gc_heap.space[0];
    gc_heap.space_size = GC_SEMISPACE_SIZE;
    gc_heap.bytes_allocated = 0;
    gc_heap.bytes_copied = 0;
    gc_heap.gc_count = 0;
    gc_heap.initialized = true;
    gc_heap.gc_in_progress = false;
}

static inline size_t gc_align_size(size_t size)
{
    return (size + GC_ALIGN_MASK) & ~GC_ALIGN_MASK;
}

void *gc_alloc(size_t size, struct __go_type_descriptor *type)
{
    if (!gc_heap.initialized)
        gc_init();

    /* Fix misalignment if present */
    if (((uintptr_t)gc_heap.alloc_ptr & GC_ALIGN_MASK) != 0)
    {
        uintptr_t correction = GC_ALIGN - ((uintptr_t)gc_heap.alloc_ptr & GC_ALIGN_MASK);
        gc_heap.alloc_ptr += correction;
    }

    if (size == 0)
        return gc_zerobase;

    /* Large objects bypass GC heap */
    if (size > GC_LARGE_OBJECT_THRESHOLD)
    {
        gc_heap.large_alloc_count++;
        gc_heap.large_alloc_total += size;
        return gc_external_alloc(size);
    }

    size_t aligned_size = gc_align_size(size);
    size_t total_size = GC_HEADER_SIZE + aligned_size;

    extern volatile int gc_inhibit_count;
    extern int32_t gc_percent; /* From gc_runtime.c */

    /* gc_percent < 0 disables automatic GC (only explicit runtime.GC()) */
    bool gc_allowed = (gc_inhibit_count == 0) && (gc_percent >= 0);

    if (gc_allowed && !gc_heap.gc_in_progress)
    {
        size_t used = gc_heap.alloc_ptr - gc_heap.space[gc_heap.active_space];
        /* Threshold based on gc_percent (default 100 = 75% of heap) */
        size_t threshold = (gc_heap.space_size * 3) / 4;
        if (gc_percent > 0 && gc_percent != 100)
            threshold = gc_heap.space_size * (size_t)gc_percent / 100;

        if (used > threshold)
            gc_collect();
    }

    if (gc_heap.alloc_ptr + total_size > gc_heap.alloc_limit)
    {
        /* Must collect even if disabled - we're out of space */
        if (gc_inhibit_count == 0)
            gc_collect();
        if (gc_heap.alloc_ptr + total_size > gc_heap.alloc_limit)
            runtime_throw("out of memory");
    }

    gc_header_t *header = (gc_header_t *)gc_heap.alloc_ptr;
    gc_heap.alloc_ptr += total_size;
    gc_heap.bytes_allocated += total_size;
    gc_heap.total_bytes_allocated += total_size;
    gc_heap.total_alloc_count++;

    uint8_t type_tag = type ? (type->__code & GC_KIND_MASK) : 0;
    GC_HEADER_SET(header, type_tag, total_size);
    header->type = type;

    /* NOSCAN only if type is known and has no pointers */
    bool noscan = (type != NULL) && (type->__ptrdata == 0);
    if (noscan)
        GC_HEADER_SET_NOSCAN(header);

    void *user_ptr = gc_get_user_ptr(header);
    memset(user_ptr, 0, aligned_size);
    return user_ptr;
}

/* Allocate without triggering GC - for use during GC or panic */
void *gc_alloc_no_gc(size_t size, struct __go_type_descriptor *type)
{
    if (!gc_heap.initialized)
        return NULL;

    if (((uintptr_t)gc_heap.alloc_ptr & GC_ALIGN_MASK) != 0)
    {
        uintptr_t correction = GC_ALIGN - ((uintptr_t)gc_heap.alloc_ptr & GC_ALIGN_MASK);
        gc_heap.alloc_ptr += correction;
    }

    size_t user_size = size > 0 ? size : 1;
    size_t aligned_size = gc_align_size(user_size);
    size_t total_size = GC_HEADER_SIZE + aligned_size;

    if (gc_heap.alloc_ptr + total_size > gc_heap.alloc_limit)
        runtime_throw("gc_alloc_no_gc: OOM");

    gc_header_t *header = (gc_header_t *)gc_heap.alloc_ptr;
    gc_heap.alloc_ptr += total_size;
    gc_heap.bytes_allocated += total_size;
    gc_heap.total_bytes_allocated += total_size;
    gc_heap.total_alloc_count++;

    uint8_t type_tag = type ? (type->__code & GC_KIND_MASK) : 0;
    GC_HEADER_SET(header, type_tag, total_size);
    header->type = type;

    bool noscan = (type != NULL) && (type->__ptrdata == 0);
    if (noscan)
        GC_HEADER_SET_NOSCAN(header);

    void *user_ptr = gc_get_user_ptr(header);
    memset(user_ptr, 0, aligned_size);
    return user_ptr;
}

void registerGCRoots(gc_root_list_t *roots)
{
    if (!roots)
        return;
    roots->next = gc_global_roots;
    gc_global_roots = roots;
}

void _runtime_registerGCRoots(gc_root_list_t *roots)
    __attribute__((alias("registerGCRoots")));

void gc_add_root(void **root_ptr)
{
    if (!root_ptr || gc_root_table.count >= GC_MAX_ROOTS)
        return;

    for (int i = 0; i < gc_root_table.count; i++)
        if (gc_root_table.roots[i] == root_ptr)
            return;

    gc_root_table.roots[gc_root_table.count++] = root_ptr;
}

void gc_remove_root(void **root_ptr)
{
    if (!root_ptr)
        return;

    for (int i = 0; i < gc_root_table.count; i++)
    {
        if (gc_root_table.roots[i] == root_ptr)
        {
            for (int j = i; j < gc_root_table.count - 1; j++)
                gc_root_table.roots[j] = gc_root_table.roots[j + 1];
            gc_root_table.count--;
            return;
        }
    }
}

void gc_stats(size_t *used, size_t *total, uint32_t *collections)
{
    if (used)
        *used = gc_heap.alloc_ptr - gc_heap.space[gc_heap.active_space];
    if (total)
        *total = gc_heap.space_size;
    if (collections)
        *collections = gc_heap.gc_count;
}

void *gc_external_alloc(size_t size)
{
    void *ptr = malloc(size);
    if (!ptr)
        runtime_throw("gc_external_alloc: OOM");
    memset(ptr, 0, size);
    return ptr;
}

void gc_external_free(void *ptr)
{
    if (ptr)
        free(ptr);
}

/* Exposed to Go as runtime.FreeExternal for freeing large allocations */
void runtime_FreeExternal(void *ptr) __asm__("_runtime.FreeExternal");
void runtime_FreeExternal(void *ptr)
{
    gc_external_free(ptr);
}

#if GC_DEBUG
void gc_verify_heap(void)
{
    if (!gc_heap.initialized)
        return;

    uint8_t *ptr = gc_heap.space[gc_heap.active_space];
    uint8_t *end = gc_heap.alloc_ptr;

    while (ptr < end)
    {
        gc_header_t *header = (gc_header_t *)ptr;
        size_t obj_size = GC_HEADER_GET_SIZE(header);
        if (obj_size == 0 || obj_size > gc_heap.space_size)
            break;
        ptr += obj_size;
    }
}
#endif
