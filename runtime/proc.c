/* libgodc/runtime/proc.c - Goroutine creation and management */

#include "goroutine.h"
#include "gc_semispace.h"
#include "type_descriptors.h"
#include "panic_dreamcast.h"
#include "runtime.h"
#include <string.h>
#include <stdio.h>
#include <kos.h>

/* External declarations from scheduler.c */
extern G *g0;
extern G *freegs;
extern uint64_t next_goid;
extern uint32_t goroutine_count;
extern sh4_context_t sched_context;
extern void allgs_add(G *gp);
extern void allgs_remove(G *gp);

/* Dead goroutine queue */
static G *dead_queue_head = NULL;
static G *dead_queue_tail = NULL;
static uint32_t global_generation = 0;

void generation_tick(void)
{
    global_generation++;
}

static void enqueue_dead_g(G *gp)
{
    if (gp->atomicstatus != Gdead)
        return;

    gp->death_generation = global_generation;
    gp->dead_link = NULL;

    if (dead_queue_tail)
        dead_queue_tail->dead_link = gp;
    else
        dead_queue_head = gp;
    dead_queue_tail = gp;
}

static G *dequeue_reclaimable_dead_g(void)
{
    G *gp = dead_queue_head;
    if (!gp)
        return NULL;

    uint32_t age = global_generation - gp->death_generation;
    if (age < DEAD_G_GRACE_GENERATIONS)
        return NULL;

    dead_queue_head = gp->dead_link;
    if (!dead_queue_head)
        dead_queue_tail = NULL;

    gp->dead_link = NULL;
    return gp;
}

static void free_g(G *gp)
{
    if (!gp || gp == g0)
        return;

    gp->_defer = NULL;
    gp->_panic = NULL;
    gp->checkpoint = NULL;
    gp->waiting = NULL;

    /* Add to free list */
    gp->freeLink = freegs;
    freegs = gp;
}

void cleanup_dead_goroutines(void)
{
    G *gp;
    int cleaned = 0;

    if (getg() != g0)
        return;

    while (cleaned < MAX_CLEANUP_PER_CALL) {
        gp = dequeue_reclaimable_dead_g();
        if (!gp)
            break;

        allgs_remove(gp);

        if (gp->tls) {
            tls_free(gp->tls);
            gp->tls = NULL;
        }
        goroutine_stack_free(gp);
        free_g(gp);
        cleaned++;
    }
}

/* Allocate a G struct */
static G *alloc_g(void)
{
    G *gp;

    if (freegs) {
        gp = freegs;
        freegs = gp->freeLink;
        gp->freeLink = NULL;
    } else {
        gp = (G *)malloc(sizeof(G));
        if (!gp)
            runtime_throw("failed to allocate goroutine");
    }

    memset(gp, 0, sizeof(G));
    return gp;
}

/* Goroutine entry wrapper */
static void goroutine_entry_wrapper(void *unused)
{
    (void)unused;

    /* Re-enable interrupts */
    int sr;
    __asm__ volatile("stc sr, %0" : "=r"(sr));
    sr &= ~0xF0;
    __asm__ volatile("ldc %0, sr" : : "r"(sr));

    G *gp = getg();
    if (!gp) {
        runtime_goexit_internal();
        return;
    }

    void (*fn)(void *) = (void (*)(void *))gp->startpc;
    void *arg = gp->param;

    if (fn)
        fn(arg);

    runtime_goexit_internal();
}

/* Create new goroutine */
G *__go_go(void (*fn)(void *), void *arg)
{
    G *gp = alloc_g();

    gp->goid = next_goid++;
    gp->startpc = (uintptr_t)fn;
    gp->param = arg;

    if (!goroutine_stack_init(gp, GOROUTINE_STACK_SIZE)) {
        free_g(gp);
        runtime_throw("failed to allocate goroutine stack");
    }

    gp->tls = tls_alloc();
    gp->tls->current_g = gp;
    gp->tls->stack_hi = gp->stack_hi;
    gp->tls->stack_lo = gp->stack_lo;

    size_t usable_stack = (uintptr_t)gp->stack_hi - (uintptr_t)gp->stack_lo - 64;
    __go_makecontext(&gp->context, gp->stack_lo, usable_stack,
                     goroutine_entry_wrapper, NULL);

    gp->atomicstatus = Gidle;
    gp->allgs_index = -1;

    allgs_add(gp);
    goroutine_count++;

    goready(gp);

    return gp;
}

/* gccgo entry point */
G *runtime_newproc(void (*fn)(void *), void *arg) __asm__("_runtime.newproc");
G *runtime_newproc(void (*fn)(void *), void *arg)
{
    return __go_go(fn, arg);
}

/* Internal exit */
void runtime_goexit_internal(void)
{
    G *gp = getg();

    if (!gp)
        runtime_throw("goexit: current goroutine is nil");
    if (gp == g0)
        runtime_throw("goexit on g0");
    if (gp->gflags2 & G_FLAG2_GOEXITING)
        runtime_throw("recursive goexit");

    gp->gflags2 |= G_FLAG2_GOEXITING;

    /* Run defers */
    if (gp->_defer)
        runtime_checkdefer(NULL);

    /* Clear state */
    gp->_defer = NULL;
    gp->_panic = NULL;
    gp->gflags2 = 0;
    gp->waiting = NULL;
    gp->param = NULL;
    gp->waitreason = waitReasonZero;
    gp->startpc = 0;

    /* Validate g0 */
    if (!g0 || !g0->tls)
        runtime_throw("goexit: g0 or g0->tls is NULL");
    if (sched_context.sp == 0 || sched_context.pc == 0)
        runtime_throw("goexit: sched_context not initialized");

    /* Mark dead and queue for cleanup */
    gp->atomicstatus = Gdead;
    enqueue_dead_g(gp);
    goroutine_count--;

    __asm__ volatile("" ::: "memory");

    /* Switch TLS to g0 */
    current_tls = g0->tls;
    current_tls->current_g = g0;

    __asm__ volatile("" ::: "memory");

    uint32_t old_irq = irq_disable();
    (void)old_irq;

    __go_setcontext(&sched_context);
    __builtin_unreachable();
}

/* Public goexit */
void runtime_goexit(void)
{
    G *gp = getg();
    if (!gp || gp == g0)
        runtime_throw("runtime.Goexit on g0 or nil g");

    if (gp->_panic)
        gp->_panic->goexit = true;

    runtime_goexit_internal();
}

void runtime_Goexit(void) __asm__("_runtime.Goexit");
void runtime_Goexit(void)
{
    runtime_goexit();
}

G *runtime_getg_exported(void) __asm__("_runtime.getg");
G *runtime_getg_exported(void)
{
    return getg();
}

uint64_t runtime_goid(void)
{
    G *gp = getg();
    return gp ? gp->goid : 0;
}

uint64_t runtime_getgoid(void) __asm__("_runtime.getgoid");
uint64_t runtime_getgoid(void)
{
    return runtime_goid();
}

int32_t runtime_NumGoroutine(void) __asm__("_runtime.NumGoroutine");
int32_t runtime_NumGoroutine(void)
{
    return (int32_t)goroutine_count;
}

/* Initialize proc system and main goroutine */
void proc_init(void)
{
    scheduler_init();

    G *main_g = (G *)malloc(sizeof(G));
    if (!main_g)
        runtime_throw("failed to allocate main goroutine");

    memset(main_g, 0, sizeof(G));
    main_g->goid = next_goid++;
    main_g->atomicstatus = Grunning;

    kthread_t *cur_thd = thd_current;
    if (cur_thd && cur_thd->stack && cur_thd->stack_size > 0) {
        main_g->stack_lo = cur_thd->stack;
        main_g->stack_hi = (void *)((uintptr_t)cur_thd->stack + cur_thd->stack_size);
    } else {
        register uintptr_t sp_reg asm("r15");
        uintptr_t sp = sp_reg;
        main_g->stack_hi = (void *)((sp + 0x1000) & ~0xFFF);
        main_g->stack_lo = (void *)((uintptr_t)main_g->stack_hi - 32 * 1024);
    }

    main_g->tls = tls_alloc();
    main_g->tls->current_g = main_g;
    main_g->tls->stack_hi = main_g->stack_hi;
    main_g->tls->stack_lo = main_g->stack_lo;
    main_g->allgs_index = -1;

    allgs_add(main_g);
    /* Don't increment goroutine_count - main_g is the scheduler thread,
     * not a user goroutine. Count stays at 1 (g0 only). */

    setg(main_g);
}
