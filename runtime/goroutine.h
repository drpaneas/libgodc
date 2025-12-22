/* libgodc/runtime/goroutine.h - goroutine types and scheduling
 *
 * M:1 cooperative scheduling: all goroutines on a single KOS thread.
 * Context switches at explicit yield points only.
 */
#ifndef GOROUTINE_H
#define GOROUTINE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kos.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct __go_type_descriptor;
struct _GccgoDefer;
struct _PanicRecord;
struct _Checkpoint;
struct hchan;
struct sudog;

void runtime_throw(const char *s) __attribute__((noreturn));

/* SH-4 context (64 bytes) - callee-saved regs + FPU */
typedef struct sh4_context {
    uint32_t r8, r9, r10, r11, r12, r13, r14;
    uint32_t sp, pr, pc;
    uint32_t fr12, fr13, fr14, fr15, fpscr, fpul;
} sh4_context_t;

_Static_assert(sizeof(sh4_context_t) == 64, "sh4_context_t must be 64 bytes");

/* TLS block - minimal goroutine tracking */
typedef struct tls_block {
    void *stack_guard;
    struct G *current_g;
    void *stack_hi;
    void *stack_lo;
    void *reserved[4];
} tls_block_t;

_Static_assert(sizeof(tls_block_t) == 32, "tls_block_t must be 32 bytes");

/* Stack segment for goroutines */
typedef struct stack_segment {
    struct stack_segment *prev;
    struct stack_segment *pool_next;
    void *base;
    size_t size;
    void *sp_on_entry;
    void *guard;
    bool pooled;
    uint8_t _pad[3];
} stack_segment_t;

/* Stack size - fixed per goroutine */
#include "godc_config.h"

/* Goroutine status */
typedef enum {
    Gidle = 0,
    Grunnable = 1,
    Grunning = 2,
    Gsyscall = 3,
    Gwaiting = 4,
    Gdead = 6,
    Gcopystack = 8,
    Gpreempted = 9,
} Gstatus;

/* G flags */
#define G_FLAG2_GOEXITING (1 << 0)
#define G_FLAG2_IN_PANIC  (1 << 1)

/* Wait reasons */
typedef enum {
    waitReasonZero = 0,
    waitReasonChanReceive,
    waitReasonChanSend,
    waitReasonSelect,
    waitReasonSleep,
    waitReasonSemacquire,
    waitReasonIO,
    waitReasonGC,
    waitReasonPreempted,
} WaitReason;

/*
 * G - Goroutine structure
 *
 * ABI CRITICAL: _panic at offset 0, _defer at offset 4 (gccgo hardcoded).
 * All fields in one struct - no hot/cold split.
 */
typedef struct G {
    /* ABI-CRITICAL: DO NOT MOVE */
    struct _PanicRecord *_panic;  /* Offset 0 */
    struct _GccgoDefer *_defer;   /* Offset 4 */

    /* Scheduling */
    Gstatus atomicstatus;
    struct G *schedlink;
    void *param;

    /* Stack */
    void *stack_lo;
    void *stack_hi;
    stack_segment_t *stack;
    void *stack_guard;
    tls_block_t *tls;

    /* CPU context */
    sh4_context_t context;

    /* Metadata */
    int64_t goid;
    WaitReason waitreason;
    int32_t allgs_index;
    uint32_t death_generation;
    struct G *dead_link;
    uint8_t gflags2;

    /* Channel wait */
    struct sudog *waiting;

    /* Defer/panic */
    struct _Checkpoint *checkpoint;
    int defer_depth;

    /* Entry point */
    uintptr_t startpc;

    /* Free list */
    struct G *freeLink;
} G;

/* Verify ABI-critical offsets */
_Static_assert(offsetof(G, _panic) == 0, "G._panic MUST be at offset 0");
_Static_assert(offsetof(G, _defer) == 4, "G._defer MUST be at offset 4");

/* Sudog - goroutine waiting on channel */
typedef struct sudog {
    G *g;
    struct sudog *next;
    struct sudog *prev;
    void *elem;
    uint64_t ticket;
    bool isSelect;
    bool success;
    struct sudog *waitlink;
    struct sudog *releasetime;
    struct hchan *c;
} sudog;

/* Context switch functions (runtime_sh4_minimal.S) */
int __go_getcontext(sh4_context_t *ctx);
void __go_setcontext(const sh4_context_t *ctx) __attribute__((noreturn));
void __go_swapcontext(sh4_context_t *old_ctx, const sh4_context_t *new_ctx);
void __go_makecontext(sh4_context_t *ctx, void *stack, size_t stack_size,
                      void (*entry)(void *), void *arg);

/* TLS */
extern tls_block_t *current_tls;
extern G *current_g;

__attribute__((no_split_stack)) G *getg(void);
__attribute__((no_split_stack)) void setg(G *gp);
void set_stack_guard_tls(void *guard);
__attribute__((no_split_stack)) void switch_to_goroutine(G *gp);
void tls_init(G *main_g);
tls_block_t *tls_alloc(void) __attribute__((returns_nonnull, malloc, warn_unused_result));
void tls_free(tls_block_t *tls);

/* Scheduler */
void schedule(void);
void gopark(bool (*unlockf)(void *), void *lock, WaitReason reason);
void goready(G *gp);
void goroutine_yield_to_scheduler(void);
void scheduler_init(void);
void scheduler_start(void);
void scheduler_run_loop(void);
int schedule_with_budget(uint64_t budget_us);
void cleanup_dead_goroutines(void);
extern sh4_context_t sched_context;

/* Goroutine creation */
G *__go_go(void (*fn)(void *), void *arg);
void runtime_goexit(void) __attribute__((noreturn));
void runtime_goexit_internal(void) __attribute__((noreturn));
G *runtime_getg(void);
extern void runtime_checkdefer(bool *frame);

/* Stack management */
stack_segment_t *stack_alloc(size_t size);
void stack_free(stack_segment_t *seg);
stack_segment_t *stack_pool_get(size_t min_size);
void stack_pool_put(stack_segment_t *seg);
void stack_pool_preallocate(void);
bool goroutine_stack_init(G *gp, size_t stack_size);
void goroutine_stack_free(G *gp);

/* Split-stack ABI stubs */
void *__splitstack_getcontext(void *context[10]);
void __splitstack_setcontext(void *context[10]);
void *__splitstack_makecontext(size_t stack_size, void *context[10], size_t *size);
void __splitstack_releasecontext(void *context[10]);
void *__splitstack_find(void *segment_arg, void *sp, size_t *len,
                        void **next_segment, void **next_sp, void **initial_sp);
void __splitstack_block_signals(int *new_value, int *old_value);
void __splitstack_block_signals_context(void *context[10], int *new_value, int *old_value);

/* Global state */
extern G *g0;
extern G *freegs;
extern uint64_t next_goid;
extern uint32_t goroutine_count;

G *allgs_iterate(int index);
int allgs_get_count(void);

#ifdef __cplusplus
}
#endif

#endif /* GOROUTINE_H */
