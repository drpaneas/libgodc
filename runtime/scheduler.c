/* libgodc/runtime/scheduler.c - Simple FIFO cooperative scheduler */

#include "goroutine.h"
#include "gc_semispace.h"
#include "runtime.h"
#include "godc_config.h"
#include <string.h>
#include <stdio.h>
#include <kos.h>
#include <arch/timer.h>
#include <arch/irq.h>

extern int64_t check_timers(void);

/* Global scheduler state */
G *g0 = NULL;
G *freegs = NULL;
uint64_t next_goid = 1;
uint32_t goroutine_count = 0;

/* allgs array for GC iteration */
#define ALLGS_ARRAY_MAX 512
static G *allgs_array[ALLGS_ARRAY_MAX];
static int allgs_count = 0;

void allgs_add(G *gp)
{
    if (allgs_count >= ALLGS_ARRAY_MAX)
        runtime_throw("too many goroutines");
    gp->allgs_index = allgs_count;
    allgs_array[allgs_count++] = gp;
}

void allgs_remove(G *gp)
{
    int idx = gp->allgs_index;
    if (idx < 0 || idx >= allgs_count)
        return;
    int last = allgs_count - 1;
    if (idx != last) {
        G *last_gp = allgs_array[last];
        allgs_array[idx] = last_gp;
        last_gp->allgs_index = idx;
    }
    allgs_count--;
    gp->allgs_index = -1;
}

G *allgs_iterate(int index)
{
    if (index < 0 || index >= allgs_count)
        return NULL;
    return allgs_array[index];
}

int allgs_get_count(void)
{
    return allgs_count;
}

/* Simple FIFO run queue */
static G *runq_head = NULL;
static G *runq_tail = NULL;

static void runq_put(G *gp)
{
    if (!gp) return;
    gp->schedlink = NULL;
    if (runq_tail) {
        runq_tail->schedlink = gp;
    } else {
        runq_head = gp;
    }
    runq_tail = gp;
}

static G *runq_get(void)
{
    G *gp = runq_head;
    if (gp) {
        runq_head = gp->schedlink;
        if (!runq_head)
            runq_tail = NULL;
        gp->schedlink = NULL;
    }
    return gp;
}

static inline bool runq_empty(void)
{
    return runq_head == NULL;
}

/* Scheduler context */
sh4_context_t sched_context;
static void *sched_kos_saved_stack = NULL;
static size_t sched_kos_saved_stack_size = 0;

/* Run a goroutine until it yields or exits */
static void run_goroutine(G *gp)
{
    kthread_t *cur_thd;
    int old_irq;

    gp->atomicstatus = Grunning;
    current_g = gp;
    switch_to_goroutine(gp);

    /* Save KOS thread stack, switch to goroutine stack */
    cur_thd = thd_current;
    old_irq = irq_disable();
    sched_kos_saved_stack = cur_thd->stack;
    sched_kos_saved_stack_size = cur_thd->stack_size;
    cur_thd->stack = gp->stack_lo;

    __go_swapcontext(&sched_context, &gp->context);

    /* Returned from goroutine - restore KOS stack */
    irq_disable();
    cur_thd = thd_current;
    cur_thd->stack = sched_kos_saved_stack;
    cur_thd->stack_size = sched_kos_saved_stack_size;
    irq_restore(old_irq);

    __asm__ volatile("" ::: "memory");

    current_g = g0;
    setg(g0);
}

void schedule(void)
{
    G *gp;

    setg(g0);
    cleanup_dead_goroutines();

    while ((gp = runq_get()) != NULL) {
        run_goroutine(gp);
        cleanup_dead_goroutines();
    }

    /* Wait for blocked goroutines */
    while (goroutine_count > 1) {
        gc_invalidate_incremental();
        thd_pass();
        if ((gp = runq_get()) != NULL) {
            run_goroutine(gp);
            cleanup_dead_goroutines();
        }
    }
}

/* Park current goroutine */
__attribute__((no_split_stack))
void gopark(bool (*unlockf)(void *), void *lock, WaitReason reason)
{
    G *gp = getg();
    if (!gp || gp == g0)
        runtime_throw("gopark on g0 or nil");

    gp->atomicstatus = Gwaiting;
    gp->waitreason = reason;

    if (unlockf && !unlockf(lock)) {
        gp->atomicstatus = Grunnable;
        gp->waitreason = waitReasonZero;
        runq_put(gp);
        return;
    }

    __go_swapcontext(&gp->context, &sched_context);
    __asm__ volatile("" ::: "memory");

    /* Re-enable interrupts after wakeup */
    int sr;
    __asm__ volatile("stc sr, %0" : "=r"(sr));
    sr &= ~0xF0;
    __asm__ volatile("ldc %0, sr" : : "r"(sr));
}

/* Wake a goroutine */
void goready(G *gp)
{
    if (!gp) return;

    Gstatus status = gp->atomicstatus;
    if (status == Gdead || status == Grunnable || status == Grunning)
        return;

    gp->atomicstatus = Grunnable;
    gp->waitreason = waitReasonZero;
    runq_put(gp);
}

/* Prepare for yield - called by go_yield assembly.
 * Returns 1 if swap should proceed, 0 to skip. */
__attribute__((no_split_stack))
int go_yield_prepare(void)
{
    G *gp = getg();
    if (!gp || gp == g0)
        return 0;

    gp->atomicstatus = Grunnable;
    gp->waitreason = waitReasonZero;
    runq_put(gp);
    return 1;
}

/* Yield to scheduler */
void goroutine_yield_to_scheduler(void)
{
    G *gp = getg();
    if (!gp || gp == g0)
        return;

    gp->atomicstatus = Grunnable;
    gp->waitreason = waitReasonZero;
    runq_put(gp);

    __go_swapcontext(&gp->context, &sched_context);
    __asm__ volatile("" ::: "memory");
}

/* Initialize scheduler */
__attribute__((no_split_stack))
void scheduler_init(void)
{
    /* Allocate g0 */
    g0 = (G *)malloc(sizeof(G));
    if (!g0)
        runtime_throw("failed to allocate g0");

    memset(g0, 0, sizeof(G));
    g0->goid = 0;
    g0->atomicstatus = Grunning;
    g0->allgs_index = -1;

    tls_init(g0);
    allgs_add(g0);

    goroutine_count = 1;
    current_g = g0;
}

void scheduler_start(void)
{
    if (!runq_empty())
        schedule();
}

int schedule_with_budget(uint64_t budget_us)
{
    G *gp;
    int ran = 0;
    uint64_t deadline = timer_us_gettime64() + budget_us;

    setg(g0);
    cleanup_dead_goroutines();

    while ((gp = runq_get()) != NULL) {
        ran++;
        run_goroutine(gp);
        cleanup_dead_goroutines();

        if (timer_us_gettime64() >= deadline)
            break;
    }

    return ran;
}

void scheduler_run_loop(void)
{
    int64_t next_timer;

    for (;;) {
        schedule();
        cleanup_dead_goroutines();

        if (goroutine_count <= 1)
            return;

        next_timer = check_timers();

        if (runq_empty() && next_timer < 0)
            runtime_throw("deadlock - all goroutines asleep");

        if (next_timer > 1000)
            thd_sleep((int)(next_timer / 1000));
        else
            thd_pass();
    }
}
