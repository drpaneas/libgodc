#include "goroutine.h"
#include "chan.h"
#include "gc_semispace.h"
#include "type_descriptors.h"
#include <string.h>
#include <stdlib.h>

/* Pool config in godc_config.h: SUDOG_POOL_MAX, SUDOG_PREALLOC_COUNT */
#include "godc_config.h"

/* Global sudog pool. No lock needed - M:1 scheduling, no preemption. */
static sudog *global_pool = NULL;
static int global_pool_count = 0;
static bool sudog_pool_initialized = false;

/**
 * Pre-allocate sudogs at startup to avoid malloc during gameplay.
 */
void sudog_pool_init(void)
{
    if (sudog_pool_initialized)
        return;

    /* Allocate each sudog individually so free() works correctly later */
    for (int i = 0; i < 16; i++)
    {
        sudog *s = (sudog *)malloc(sizeof(sudog));
        if (s == NULL)
            break;
        memset(s, 0, sizeof(sudog));
        s->next = global_pool;
        global_pool = s;
        global_pool_count++;
    }

    sudog_pool_initialized = true;
}

/**
 * Acquire a sudog. Uses malloc (not gc_alloc) so sudogs survive GC cycles.
 */
sudog *acquireSudog(void)
{
    sudog *s = NULL;

    if (global_pool != NULL)
    {
        s = global_pool;
        global_pool = s->next;
        global_pool_count--;
    }
    else
    {
        s = (sudog *)malloc(sizeof(sudog));
        if (s == NULL)
            return NULL;
    }

    memset(s, 0, sizeof(sudog));
    s->g = getg();
    return s;
}

/**
 * Release a sudog back to the pool.
 */
void releaseSudog(sudog *s)
{
    if (s == NULL)
        return;

    s->g = NULL;
    s->elem = NULL;
    s->c = NULL;
    s->waitlink = NULL;
    s->prev = NULL;

    if (global_pool_count < SUDOG_POOL_MAX)
    {
        s->next = global_pool;
        global_pool = s;
        global_pool_count++;
        return;
    }

    free(s);
}

/**
 * Enqueue a sudog at the end of a wait queue.
 */
void waitq_enqueue(waitq *q, sudog *s)
{
    if (q == NULL || s == NULL)
        return;

    s->next = NULL;
    s->prev = q->last;

    if (q->last != NULL)
    {
        q->last->next = s;
    }
    else
    {
        q->first = s;
    }

    q->last = s;
}

/**
 * Dequeue the first sudog from a wait queue.
 * Returns NULL if queue is empty.
 */
sudog *waitq_dequeue(waitq *q)
{
    if (q == NULL || q->first == NULL)
    {
        return NULL;
    }

    sudog *s = q->first;
    q->first = s->next;

    if (q->first != NULL)
    {
        q->first->prev = NULL;
    }
    else
    {
        q->last = NULL;
    }

    s->next = NULL;
    s->prev = NULL;

    return s;
}

/**
 * Remove a specific sudog from a wait queue.
 * Used when a select case is cancelled.
 */
void waitq_remove(waitq *q, sudog *s)
{
    if (q == NULL || s == NULL)
        return;

    if (s->prev != NULL)
    {
        s->prev->next = s->next;
    }
    else
    {
        q->first = s->next;
    }

    if (s->next != NULL)
    {
        s->next->prev = s->prev;
    }
    else
    {
        q->last = s->prev;
    }

    s->next = NULL;
    s->prev = NULL;
}

/**
 * Check if wait queue is empty.
 */
bool waitq_empty(waitq *q)
{
    return q == NULL || q->first == NULL;
}
