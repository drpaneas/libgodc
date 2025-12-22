/* gen-offsets.c - Generate asm-offsets.h for assembly code */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Minimal type definitions matching goroutine.h */

typedef struct sh4_context {
    uint32_t r8, r9, r10, r11, r12, r13, r14;
    uint32_t sp, pr, pc;
    uint32_t fr12, fr13, fr14, fr15, fpscr, fpul;
} sh4_context_t;

typedef struct tls_block {
    void *stack_guard;
    struct G *current_g;
    void *stack_hi;
    void *stack_lo;
    void *reserved[4];
} tls_block_t;

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

typedef enum { Gidle = 0, Grunnable = 1, Grunning = 2, Gwaiting = 4, Gdead = 6 } Gstatus;
typedef enum { waitReasonZero = 0 } WaitReason;

struct _PanicRecord;
struct _GccgoDefer;
struct _Checkpoint;
struct sudog;

typedef struct G {
    struct _PanicRecord *_panic;
    struct _GccgoDefer *_defer;
    Gstatus atomicstatus;
    struct G *schedlink;
    void *param;
    void *stack_lo;
    void *stack_hi;
    stack_segment_t *stack;
    void *stack_guard;
    tls_block_t *tls;
    sh4_context_t context;
    int64_t goid;
    WaitReason waitreason;
    int32_t allgs_index;
    uint32_t death_generation;
    struct G *dead_link;
    uint8_t gflags2;
    struct sudog *waiting;
    struct _Checkpoint *checkpoint;
    int defer_depth;
    uintptr_t startpc;
    struct G *freeLink;
} G;

#define OFFSET(name, type, field) \
    int __offset_##name[(offsetof(type, field) + 1)] __attribute__((section(".offsets")))

#define SIZE(name, type) \
    int __size_##name[(sizeof(type) + 1)] __attribute__((section(".sizes")))

/* Generate offsets */
SIZE(G_SIZE, G);
OFFSET(G_PANIC, G, _panic);
OFFSET(G_DEFER, G, _defer);
OFFSET(G_ATOMICSTATUS, G, atomicstatus);
OFFSET(G_SCHEDLINK, G, schedlink);
OFFSET(G_PARAM, G, param);
OFFSET(G_STACK_LO, G, stack_lo);
OFFSET(G_STACK_HI, G, stack_hi);
OFFSET(G_STACK, G, stack);
OFFSET(G_STACK_GUARD, G, stack_guard);
OFFSET(G_TLS, G, tls);
OFFSET(G_CONTEXT, G, context);
OFFSET(G_GOID, G, goid);
OFFSET(G_WAITREASON, G, waitreason);
OFFSET(G_WAITING, G, waiting);
OFFSET(G_CHECKPOINT, G, checkpoint);
OFFSET(G_STARTPC, G, startpc);

SIZE(CONTEXT_SIZE, sh4_context_t);
OFFSET(CONTEXT_R8, sh4_context_t, r8);
OFFSET(CONTEXT_SP, sh4_context_t, sp);
OFFSET(CONTEXT_PR, sh4_context_t, pr);
OFFSET(CONTEXT_PC, sh4_context_t, pc);
OFFSET(CONTEXT_FR12, sh4_context_t, fr12);
OFFSET(CONTEXT_FPSCR, sh4_context_t, fpscr);
OFFSET(CONTEXT_FPUL, sh4_context_t, fpul);

SIZE(TLS_SIZE, tls_block_t);
OFFSET(TLS_STACK_GUARD, tls_block_t, stack_guard);
OFFSET(TLS_CURRENT_G, tls_block_t, current_g);
OFFSET(TLS_STACK_HI, tls_block_t, stack_hi);
OFFSET(TLS_STACK_LO, tls_block_t, stack_lo);
