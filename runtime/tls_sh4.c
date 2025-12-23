/* libgodc/runtime/tls_sh4.c - Thread-local storage for SH-4 */

#include "goroutine.h"
#include "gc_semispace.h"
#include "runtime.h"
#include <string.h>
#include <kos.h>

/* Global state - M:1 cooperative scheduling, no locks needed */
G *current_g = NULL;
tls_block_t *current_tls = NULL;

static tls_block_t main_tls_block __attribute__((aligned(8)));

G *getg(void) { return current_g; }

void setg(G *gp)
{
    current_g = gp;
    if (gp && gp->tls)
        gp->tls->current_g = gp;
}

void switch_to_goroutine(G *gp)
{
    if (!gp)
        return;

    tls_block_t *tls = gp->tls;
    if (tls) {
        tls->stack_guard = gp->stack_guard;
        tls->current_g = gp;
        tls->stack_hi = gp->stack_hi;
        tls->stack_lo = gp->stack_lo;
    }

    current_g = gp;
    current_tls = tls;
    __asm__ volatile("" ::: "memory");
}

void tls_init(G *main_g)
{
    memset(&main_tls_block, 0, sizeof(tls_block_t));
    main_g->tls = &main_tls_block;
    main_tls_block.current_g = main_g;

    kthread_t *cur_thd = thd_current;
    if (cur_thd && cur_thd->stack && cur_thd->stack_size > 0) {
        main_g->stack_lo = cur_thd->stack;
        main_g->stack_hi = (void *)((uintptr_t)cur_thd->stack + cur_thd->stack_size);
        main_g->stack_guard = main_g->stack_lo;
    } else {
        register uintptr_t sp asm("r15");
        main_g->stack_hi = (void *)((sp + 0x1000) & ~0xFFF);
        main_g->stack_lo = (void *)((uintptr_t)main_g->stack_hi - 32 * 1024);
        main_g->stack_guard = main_g->stack_lo;
    }

    main_tls_block.stack_hi = main_g->stack_hi;
    main_tls_block.stack_lo = main_g->stack_lo;
    main_tls_block.stack_guard = main_g->stack_guard;

    current_g = main_g;
    current_tls = &main_tls_block;
}

/* TLS pool */
#define TLS_POOL_MAX 64
static tls_block_t *tls_pool_head = NULL;
static int tls_pool_count = 0;

tls_block_t *tls_alloc(void)
{
    tls_block_t *tls;

    if (tls_pool_head) {
        tls = tls_pool_head;
        tls_pool_head = (tls_block_t *)tls->stack_guard;
        tls_pool_count--;
        memset(tls, 0, sizeof(tls_block_t));
        return tls;
    }

    tls = (tls_block_t *)malloc(sizeof(tls_block_t));
    if (!tls)
        runtime_throw("tls_alloc: out of memory");

    memset(tls, 0, sizeof(tls_block_t));
    return tls;
}

void tls_free(tls_block_t *tls)
{
    if (!tls || tls == &main_tls_block)
        return;

    if (tls_pool_count < TLS_POOL_MAX) {
        tls->stack_guard = tls_pool_head;
        tls_pool_head = tls;
        tls_pool_count++;
        return;
    }

    free(tls);
}

void set_stack_guard_tls(void *guard)
{
    G *gp = getg();
    if (gp && gp->tls)
        gp->tls->stack_guard = guard;
}

G *_g_(void)
{
    return getg();
}
