/* libgodc/runtime/defer_dreamcast.c - defer, panic, recover */

#include "goroutine.h"
#include "runtime.h"
#include "panic_dreamcast.h"
#include "type_descriptors.h"
#include "gc_semispace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <kos.h>
#include <arch/stack.h>
#include <arch/arch.h>
#include <malloc.h>
#include <kos/dbglog.h>

static bool g_panic_initialized = false;

static inline G *must_getg(void)
{
    G *gp = getg();
    if (!gp)
        runtime_throw("defer/panic: no G");
    return gp;
}

static inline bool get_in_panic(void)
{
    G *gp = getg();
    return gp && (gp->gflags2 & G_FLAG2_IN_PANIC) != 0;
}

static inline void set_in_panic(bool val)
{
    G *gp = getg();
    if (!gp)
        return;
    if (val)
        gp->gflags2 |= G_FLAG2_IN_PANIC;
    else
        gp->gflags2 &= ~G_FLAG2_IN_PANIC;
}

#define g_gccgo_defer_chain (must_getg()->_defer)
#define g_panic_chain (must_getg()->_panic)
#define g_checkpoint_chain (must_getg()->checkpoint)
#define g_defer_depth (must_getg()->defer_depth)

/* Type descriptors */
static const uint8_t __gccgo_defer_gcdata[] = {0x2F};
DEFINE_GO_TYPE_DESC(__gccgo_defer_type, GccgoDefer, GO_STRUCT, 24, __gccgo_defer_gcdata);
DEFINE_GO_TYPE_DESC(__panic_record_type, PanicRecord, GO_STRUCT, 12, NULL);
DEFINE_GO_TYPE_DESC(__checkpoint_type, Checkpoint, GO_STRUCT, 4, NULL);
DEFINE_GO_TYPE_DESC(__gostring_header_type, GoString, GO_STRING, sizeof(void *), NULL);

static inline bool in_irq_context(void)
{
    uint32_t sr;
    __asm__ volatile("stc sr, %0" : "=r"(sr));
    return (sr & 0x10000000) != 0;
}

/* Walk two frames up to get caller's caller. Used by runtime_checkpoint(). */
static void *panic_get_caller_frame(void)
{
#ifdef FRAME_POINTERS
    uintptr_t fp = arch_get_fptr();
    fp = arch_fptr_next(fp);
    fp = arch_fptr_next(fp);
    return (void *)fp;
#else
    uintptr_t sp;
    __asm__ __volatile__("mov r15, %0" : "=r"(sp));
    return (void *)sp;
#endif
}

static void print_panic_value(struct __go_type_descriptor *type, void *data)
{
    if (!type) {
        dbglog(DBG_CRITICAL, "nil");
        return;
    }

    uint8_t kind = type->__code & 0x1F;

    switch (kind) {
    case GO_STRING: {
        GoString *s = (GoString *)data;
        if (s && s->str && s->len > 0)
            dbglog(DBG_CRITICAL, "%.*s", (int)s->len, (char *)s->str);
        else
            dbglog(DBG_CRITICAL, "(empty string)");
        break;
    }
    case GO_INT:
        dbglog(DBG_CRITICAL, "%d", *(int *)data);
        break;
    case GO_INT32:
        dbglog(DBG_CRITICAL, "%ld", (long)*(int32_t *)data);
        break;
    case GO_INT64:
        dbglog(DBG_CRITICAL, "%lld", (long long)*(int64_t *)data);
        break;
    case GO_UINT:
    case GO_UINT32:
        dbglog(DBG_CRITICAL, "%lu", (unsigned long)*(uint32_t *)data);
        break;
    case GO_FLOAT32:
        dbglog(DBG_CRITICAL, "%f", (double)*(float *)data);
        break;
    case GO_FLOAT64:
        dbglog(DBG_CRITICAL, "%f", *(double *)data);
        break;
    case GO_BOOL:
        dbglog(DBG_CRITICAL, "%s", *(bool *)data ? "true" : "false");
        break;
    default:
        dbglog(DBG_CRITICAL, "(value of kind %d at %p)", kind, data);
        break;
    }
}

static void fatalpanic(PanicRecord *p) __attribute__((noreturn));
static void fatalpanic(PanicRecord *p)
{
    int old_irq = irq_disable();

    dbglog(DBG_CRITICAL, "\npanic: ");
    print_panic_value(p->arg_type, p->arg_data);
    dbglog(DBG_CRITICAL, "\n");

    dbglog(DBG_CRITICAL, "\ngoroutine 1 [running]:\n");
    arch_stk_trace(2);

    struct mallinfo mi = mallinfo();
    dbglog(DBG_CRITICAL, "\nMemory: arena=%d used=%d free=%d\n",
           mi.arena, mi.uordblks, mi.fordblks);

    dbgio_flush();
    irq_restore(old_irq);

    if (thd_current)
        thd_sleep(FATALPANIC_FLUSH_DELAY_MS);

    arch_exit();
    __builtin_unreachable();
}

void runtime_gopanic_impl(struct __go_type_descriptor *type, void *data)
{
    if (!g_panic_initialized) {
        int old_irq = irq_disable();
        dbglog(DBG_CRITICAL, "\nFATAL: panic before init\npanic: ");
        print_panic_value(type, data);
        dbglog(DBG_CRITICAL, "\n\n");
        arch_stk_trace(2);
        dbgio_flush();
        irq_restore(old_irq);
        arch_exit();
        __builtin_unreachable();
    }

    if (in_irq_context()) {
        int old_irq = irq_disable();
        dbglog(DBG_CRITICAL, "\nFATAL: panic in IRQ\npanic: ");
        print_panic_value(type, data);
        dbglog(DBG_CRITICAL, "\n");
        arch_stk_trace(2);
        dbgio_flush();
        irq_restore(old_irq);
        arch_exit();
        __builtin_unreachable();
    }

    int panic_depth = 0;
    for (PanicRecord *pp = g_panic_chain; pp; pp = pp->link) {
        panic_depth++;
        if (panic_depth > 32) {
            dbglog(DBG_CRITICAL, "panic: chain corrupted\n");
            arch_exit();
        }
    }

    if (panic_depth > MAX_RECURSIVE_PANICS) {
        dbglog(DBG_CRITICAL, "panic: too many nested panics\n");
        arch_exit();
    }

    PanicRecord *p = (PanicRecord *)gc_alloc_no_gc(sizeof(PanicRecord), &__panic_record_type);
    if (!p) {
        dbglog(DBG_CRITICAL, "panic: out of memory\n");
        print_panic_value(type, data);
        dbglog(DBG_CRITICAL, "\n");
        abort();
    }

    p->arg_type = type;
    p->arg_data = data;
    p->recovered = false;
    p->aborted = false;
    p->goexit = false;
    p->link = g_panic_chain;

    g_panic_chain = p;
    set_in_panic(true);

    G *gp = must_getg();
    while (gp->_defer) {
        GccgoDefer *d = gp->_defer;
        uintptr_t pfn = d->pfn;
        void *arg = d->arg;

        if (pfn == 0) {
            if (d->_panic)
                d->_panic->aborted = true;
            d->_panic = NULL;
            gp->_defer = d->link;
            gp->defer_depth--;
            continue;
        }

        d->pfn = 0;
        d->_panic = p;

        void (*fn)(void *) = (void (*)(void *))pfn;
        fn(arg);

        if (p->recovered) {
            gp->_panic = p->link;
            while (gp->_panic && gp->_panic->aborted)
                gp->_panic = gp->_panic->link;

            set_in_panic(gp->_panic != NULL);

            d->_panic = NULL;
            gp->_defer = d->link;
            gp->defer_depth--;

            Checkpoint *cp = gp->checkpoint;
            if (cp) {
                gp->checkpoint = cp->link;
                longjmp(cp->env, 1);
            }

            dbglog(DBG_CRITICAL, "\nFATAL: recover without checkpoint\n");
            arch_stk_trace(1);
            dbgio_flush();
            arch_exit();
            __builtin_unreachable();
        }

        d->_panic = NULL;
        gp->_defer = d->link;
        gp->defer_depth--;
    }

    fatalpanic(gp->_panic);
    __builtin_unreachable();
}

Eface runtime_gorecover_impl(void)
{
    Eface result = {NULL, NULL};

    if (!get_in_panic() || !g_panic_chain)
        return result;

    PanicRecord *p = g_panic_chain;
    if (p->recovered)
        return result;

    p->recovered = true;
    result.type = p->arg_type;
    result.data = p->arg_data;
    return result;
}

void runtime_panicstring(const char *s)
{
    size_t len = strlen(s);

    char *str_data = (char *)gc_alloc_no_gc(len + 1, NULL);
    if (!str_data) {
        dbglog(DBG_CRITICAL, "panic: %s\n", s);
        abort();
    }
    memcpy(str_data, s, len + 1);

    GoString *gs = (GoString *)gc_alloc_no_gc(sizeof(GoString), &__gostring_header_type);
    if (!gs) {
        dbglog(DBG_CRITICAL, "panic: %s\n", s);
        abort();
    }
    gs->str = (const uint8_t *)str_data;
    gs->len = (intptr_t)len;

    extern struct __go_type_descriptor __go_tdn_string;
    runtime_gopanic_impl(&__go_tdn_string, gs);
}

void runtime_throw(const char *s)
{
    int old_irq = irq_disable();
    dbglog(DBG_CRITICAL, "\nfatal error: %s\n\n", s);
    arch_stk_trace(1);
    struct mallinfo mi = mallinfo();
    dbglog(DBG_CRITICAL, "Memory: arena=%d used=%d free=%d\n",
           mi.arena, mi.uordblks, mi.fordblks);
    dbgio_flush();
    irq_restore(old_irq);
    arch_exit();
    __builtin_unreachable();
}

bool runtime_canrecover(uintptr_t frame_addr) __asm__("_runtime.canrecover");
bool runtime_canrecover(uintptr_t frame_addr)
{
    (void)frame_addr;
    return get_in_panic() && g_panic_chain && !g_panic_chain->recovered;
}

void panic_init(void)
{
    G *gp = getg();
    if (!gp)
        runtime_throw("panic_init: no G");

    gp->_defer = NULL;
    gp->_panic = NULL;
    gp->checkpoint = NULL;
    gp->defer_depth = 0;
    gp->gflags2 &= ~G_FLAG2_IN_PANIC;

    g_panic_initialized = true;
}

/**
 * runtime_checkpoint - Create a recovery point for panic/recover.
 *
 * Usage in Go code:
 *     if runtime_checkpoint() != 0 {
 *         // Recovered from panic - handle error
 *         return
 *     }
 *     defer func() {
 *         if r := recover(); r != nil {
 *             // This triggers longjmp back to checkpoint
 *         }
 *     }()
 *     // ... code that might panic ...
 *
 * Returns 0 on initial call, non-zero when longjmp'd back after recovery.
 */
int runtime_checkpoint(void)
{
    Checkpoint *cp = (Checkpoint *)gc_alloc(sizeof(Checkpoint), &__checkpoint_type);
    if (!cp)
        runtime_throw("failed to allocate checkpoint");

    cp->frame = panic_get_caller_frame();
    cp->link = g_checkpoint_chain;
    g_checkpoint_chain = cp;

    /* Use setjmp for checkpoint - returns 0 first time, 1 after longjmp */
    return setjmp(cp->env);
}

/**
 * runtime_uncheckpoint - Remove checkpoint if it belongs to caller's frame.
 *
 * Call this to clean up a checkpoint when the function returns normally
 * (without panicking).
 */
void runtime_uncheckpoint(void)
{
    if (g_checkpoint_chain) {
        void *caller_frame = panic_get_caller_frame();
        if (g_checkpoint_chain->frame == caller_frame)
            g_checkpoint_chain = g_checkpoint_chain->link;
    }
}

void runtime_gopanic(struct __go_type_descriptor *type, void *data) __asm__("_runtime.gopanic");
void runtime_gopanic(struct __go_type_descriptor *type, void *data)
{
    runtime_gopanic_impl(type, data);
}

Eface runtime_gorecover(void) __asm__("_runtime.gorecover");
Eface runtime_gorecover(void)
{
    return runtime_gorecover_impl();
}

Eface runtime_deferredrecover(void) __asm__("_runtime.deferredrecover");
Eface runtime_deferredrecover(void)
{
    return runtime_gorecover_impl();
}

void runtime_deferprocStack(GccgoDefer *d, bool *frame, uintptr_t pfn, void *arg) __asm__("_runtime.deferprocStack");
void runtime_deferprocStack(GccgoDefer *d, bool *frame, uintptr_t pfn, void *arg)
{
    if (g_defer_depth >= MAX_DEFER_DEPTH)
        runtime_throw("defer overflow");
    if (!d)
        runtime_throw("deferprocStack: nil");

    d->pfn = pfn;
    d->arg = arg;
    d->frame = frame;
    d->retaddr = 0;
    d->makefunccanrecover = false;
    d->heap = false;
    d->panicStack = g_panic_chain;
    d->_panic = NULL;
    d->link = g_gccgo_defer_chain;
    g_gccgo_defer_chain = d;
    g_defer_depth++;
}

void runtime_deferproc_gccgo(bool *frame, uintptr_t pfn, void *arg) __asm__("_runtime.deferproc");
void runtime_deferproc_gccgo(bool *frame, uintptr_t pfn, void *arg)
{
    if (g_defer_depth >= MAX_DEFER_DEPTH)
        runtime_throw("defer overflow");

    GccgoDefer *d = (GccgoDefer *)gc_alloc(sizeof(GccgoDefer), &__gccgo_defer_type);
    if (!d)
        runtime_throw("defer alloc failed");

    d->pfn = pfn;
    d->arg = arg;
    d->frame = frame;
    d->retaddr = 0;
    d->makefunccanrecover = false;
    d->heap = true;
    d->panicStack = g_panic_chain;
    d->_panic = NULL;
    d->link = g_gccgo_defer_chain;
    g_gccgo_defer_chain = d;
    g_defer_depth++;
}

void runtime_deferreturn_gccgo(bool *frame) __asm__("_runtime.deferreturn");
void runtime_deferreturn_gccgo(bool *frame)
{
    G *gp = must_getg();

    while (gp->_defer && gp->_defer->frame == frame) {
        GccgoDefer *d = gp->_defer;
        uintptr_t pfn = d->pfn;
        void *arg = d->arg;

        if (pfn == 0) {
            gp->_defer = d->link;
            gp->defer_depth--;
            continue;
        }

        d->pfn = 0;
        void (*fn)(void *) = (void (*)(void *))pfn;
        fn(arg);

        gp->_defer = d->link;
        gp->defer_depth--;
    }
}

static void runtime_checkdefer_impl(bool *frame);

void runtime_checkdefer_asm(bool *frame) __asm__("_runtime.checkdefer");
void runtime_checkdefer_asm(bool *frame)
{
    runtime_checkdefer_impl(frame);
}

void runtime_checkdefer(bool *frame)
{
    runtime_checkdefer_impl(frame);
}

static void runtime_checkdefer_impl(bool *frame)
{
    G *gp = must_getg();

    if (frame) {
        while (gp->_defer && gp->_defer->frame == frame) {
            GccgoDefer *d = gp->_defer;
            uintptr_t pfn = d->pfn;
            void *arg = d->arg;

            if (pfn == 0) {
                gp->_defer = d->link;
                gp->defer_depth--;
                continue;
            }

            d->pfn = 0;
            void (*fn)(void *) = (void (*)(void *))pfn;
            fn(arg);
            gp->_defer = d->link;
            gp->defer_depth--;
        }
    } else {
        while (gp->_defer) {
            GccgoDefer *d = gp->_defer;
            uintptr_t pfn = d->pfn;
            void *arg = d->arg;

            if (pfn == 0) {
                gp->_defer = d->link;
                gp->defer_depth--;
                continue;
            }

            d->pfn = 0;
            void (*fn)(void *) = (void (*)(void *))pfn;
            fn(arg);
            gp->_defer = d->link;
            gp->defer_depth--;
        }
    }
}

bool runtime_setdeferretaddr(void *retaddr) __asm__("_runtime.setdeferretaddr");
bool runtime_setdeferretaddr(void *retaddr)
{
    (void)retaddr;
    return get_in_panic() && g_panic_chain && g_panic_chain->recovered;
}
