/* libgodc/runtime/timer.c - Simple heap-based timers */

#include "goroutine.h"
#include "gc_semispace.h"
#include "godc_config.h"
#include "runtime.h"
#include <string.h>
#include <kos.h>
#include <arch/timer.h>

#define MAX_TIMERS 256

typedef struct go_timer {
    uint64_t when;
    int64_t period;
    void (*f)(void *);
    void *arg;
    G *gp;
    bool active;
    int heap_index;
} go_timer_t;

static go_timer_t *timer_heap[MAX_TIMERS];
static int heap_size = 0;

static go_timer_t timer_pool[MAX_TIMERS];
static go_timer_t *timer_free_list = NULL;
static bool timer_pool_initialized = false;

static void timer_pool_init(void)
{
    if (timer_pool_initialized)
        return;

    for (int i = 0; i < MAX_TIMERS - 1; i++) {
        timer_pool[i].heap_index = -1;
        timer_pool[i].active = false;
        timer_pool[i].gp = (G *)&timer_pool[i + 1];
    }
    timer_pool[MAX_TIMERS - 1].heap_index = -1;
    timer_pool[MAX_TIMERS - 1].active = false;
    timer_pool[MAX_TIMERS - 1].gp = NULL;

    timer_free_list = &timer_pool[0];
    timer_pool_initialized = true;
}

static void heap_swap(int i, int j)
{
    go_timer_t *tmp = timer_heap[i];
    timer_heap[i] = timer_heap[j];
    timer_heap[j] = tmp;
    timer_heap[i]->heap_index = i;
    timer_heap[j]->heap_index = j;
}

static void heap_up(int i)
{
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (timer_heap[parent]->when <= timer_heap[i]->when)
            break;
        heap_swap(i, parent);
        i = parent;
    }
}

static void heap_down(int i)
{
    while (1) {
        int smallest = i;
        int left = 2 * i + 1;
        int right = 2 * i + 2;

        if (left < heap_size && timer_heap[left]->when < timer_heap[smallest]->when)
            smallest = left;
        if (right < heap_size && timer_heap[right]->when < timer_heap[smallest]->when)
            smallest = right;

        if (smallest == i)
            break;

        heap_swap(i, smallest);
        i = smallest;
    }
}

static void heap_insert(go_timer_t *t)
{
    if (heap_size >= MAX_TIMERS)
        return;

    int i = heap_size++;
    timer_heap[i] = t;
    t->heap_index = i;
    heap_up(i);
}

static void heap_remove(go_timer_t *t)
{
    int i = t->heap_index;
    if (i < 0 || i >= heap_size)
        return;

    t->heap_index = -1;
    heap_size--;

    if (i == heap_size)
        return;

    timer_heap[i] = timer_heap[heap_size];
    timer_heap[i]->heap_index = i;

    if (i > 0 && timer_heap[i]->when < timer_heap[(i - 1) / 2]->when)
        heap_up(i);
    else
        heap_down(i);
}

static go_timer_t *heap_peek(void)
{
    return (heap_size > 0) ? timer_heap[0] : NULL;
}

static go_timer_t *go_timer_alloc(void)
{
    if (!timer_pool_initialized)
        timer_pool_init();

    if (!timer_free_list)
        return NULL;

    go_timer_t *t = timer_free_list;
    timer_free_list = (go_timer_t *)t->gp;

    memset(t, 0, sizeof(go_timer_t));
    t->heap_index = -1;

    return t;
}

static void go_timer_free(go_timer_t *t)
{
    if (!t)
        return;

    if (t->heap_index >= 0)
        heap_remove(t);

    t->active = false;
    t->gp = (G *)timer_free_list;
    timer_free_list = t;
}

static inline uint64_t now_us(void)
{
    return timer_us_gettime64();
}

/* time.Sleep */
void timeSleep(int64_t ns)
{
    if (ns <= 0)
        return;

    G *gp = getg();
    if (!gp || gp == g0) {
        thd_sleep((int)(ns / 1000000));
        return;
    }

    go_timer_t *t = go_timer_alloc();
    if (!t) {
        thd_sleep((int)(ns / 1000000));
        return;
    }

    t->when = now_us() + (uint64_t)(ns / 1000);
    t->period = 0;
    t->f = NULL;
    t->arg = NULL;
    t->gp = gp;
    t->active = true;

    heap_insert(t);
    gopark(NULL, NULL, waitReasonSleep);
    go_timer_free(t);
}

void runtime_timeSleep(int64_t ns) __asm__("time.Sleep");
void runtime_timeSleep(int64_t ns)
{
    timeSleep(ns);
}

/* Check expired timers */
int64_t check_timers(void)
{
    uint64_t now = now_us();
    int processed = 0;

    while (processed < TIMER_PROCESS_MAX) {
        go_timer_t *t = heap_peek();
        if (!t)
            return -1;

        if (t->when > now)
            return (int64_t)(t->when - now);

        processed++;
        heap_remove(t);

        if (t->gp) {
            G *gp = t->gp;
            t->gp = NULL;
            goready(gp);
        } else if (t->f) {
            void (*f)(void *) = t->f;
            void *arg = t->arg;

            if (t->period > 0) {
                t->when = now_us() + (uint64_t)t->period;
                heap_insert(t);
            } else {
                t->active = false;
            }

            f(arg);
            now = now_us();
        } else {
            t->active = false;
        }
    }

    return 0;
}
