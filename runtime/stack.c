/* libgodc/runtime/stack.c - Stack management for goroutines */

#include "goroutine.h"
#include "gc_semispace.h"
#include "runtime.h"
#include <string.h>
#include <kos.h>
#include <malloc.h>

#define STACK_SIZE_CLASSES 3

/* Stack pools */
static stack_segment_t *stack_pools[STACK_SIZE_CLASSES] = {NULL, NULL, NULL};
static int pool_counts[STACK_SIZE_CLASSES] = {0, 0, 0};

static int get_size_class(size_t size)
{
    if (size <= STACK_SIZE_SMALL)
        return 0;
    if (size <= STACK_SIZE_MEDIUM)
        return 1;
    if (size <= STACK_SIZE_LARGE)
        return 2;
    return -1;
}

static size_t round_to_size_class(size_t size)
{
    if (size <= STACK_SIZE_SMALL)
        return STACK_SIZE_SMALL;
    if (size <= STACK_SIZE_MEDIUM)
        return STACK_SIZE_MEDIUM;
    if (size <= STACK_SIZE_LARGE)
        return STACK_SIZE_LARGE;
    return (size + 4095) & ~4095;
}

__attribute__((no_split_stack))
stack_segment_t *stack_alloc(size_t size)
{
    size = round_to_size_class(size);

    stack_segment_t *seg = stack_pool_get(size);
    if (seg)
        return seg;

    void *base = memalign(8, size);
    if (!base)
        runtime_throw("stack_alloc: out of memory");

    seg = (stack_segment_t *)malloc(sizeof(stack_segment_t));
    if (!seg) {
        free(base);
        runtime_throw("stack_alloc: out of memory for header");
    }

    seg->prev = NULL;
    seg->base = base;
    seg->size = size;
    seg->sp_on_entry = NULL;
    seg->guard = (void *)((uintptr_t)base + STACK_GUARD_SIZE);
    seg->pooled = false;

    return seg;
}

__attribute__((no_split_stack))
void stack_free(stack_segment_t *seg)
{
    if (!seg)
        return;

    if (!seg->base || seg->size == 0) {
        free(seg);
        return;
    }

    int class_idx = get_size_class(seg->size);
    if (class_idx >= 0 && pool_counts[class_idx] < STACK_POOL_MAX_SEGMENTS / STACK_SIZE_CLASSES) {
        stack_pool_put(seg);
        return;
    }

    free(seg->base);
    free(seg);
}

stack_segment_t *stack_pool_get(size_t min_size)
{
    int class_idx = get_size_class(min_size);
    if (class_idx < 0)
        return NULL;

    if (stack_pools[class_idx]) {
        stack_segment_t *seg = stack_pools[class_idx];
        stack_pools[class_idx] = seg->pool_next;
        pool_counts[class_idx]--;
        seg->pool_next = NULL;
        seg->pooled = false;
        return seg;
    }

    for (int i = class_idx + 1; i < STACK_SIZE_CLASSES; i++) {
        if (stack_pools[i]) {
            stack_segment_t *seg = stack_pools[i];
            stack_pools[i] = seg->pool_next;
            pool_counts[i]--;
            seg->pool_next = NULL;
            seg->pooled = false;
            return seg;
        }
    }

    return NULL;
}

void stack_pool_put(stack_segment_t *seg)
{
    if (!seg)
        return;

    int class_idx = get_size_class(seg->size);
    if (class_idx < 0) {
        stack_free(seg);
        return;
    }

    if (pool_counts[class_idx] >= STACK_POOL_MAX_SEGMENTS / STACK_SIZE_CLASSES) {
        free(seg->base);
        free(seg);
        return;
    }

    seg->prev = NULL;
    seg->sp_on_entry = NULL;
    seg->pooled = true;
    seg->pool_next = stack_pools[class_idx];
    stack_pools[class_idx] = seg;
    pool_counts[class_idx]++;
}

bool goroutine_stack_init(G *gp, size_t stack_size)
{
    if (!gp)
        return false;

    stack_segment_t *seg = stack_alloc(stack_size);

    gp->stack = seg;
    gp->stack_lo = seg->base;
    gp->stack_hi = (void *)((uintptr_t)seg->base + seg->size);
    gp->stack_guard = seg->guard;

    return true;
}

void goroutine_stack_free(G *gp)
{
    if (!gp)
        return;

    stack_segment_t *seg = gp->stack;
    while (seg) {
        stack_segment_t *prev = seg->prev;
        stack_free(seg);
        seg = prev;
    }

    gp->stack = NULL;
    gp->stack_lo = NULL;
    gp->stack_hi = NULL;
    gp->stack_guard = NULL;
}

void stack_pool_preallocate(void)
{
    /* Optionally pre-allocate stacks at startup */
}
