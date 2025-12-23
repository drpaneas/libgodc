#include "goroutine.h"
#include "chan.h"
#include "gc_semispace.h"
#include "panic_dreamcast.h"
#include "copy.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include <arch/timer.h>

/* from scheduler.c */
extern void go_yield(void);

/**
 * SelectGoResult - Return type for selectgo.
 *
 * gccgo expects selectgo to return (int, bool) where:
 *   - selected: index of the selected case (-1 if non-blocking and nothing ready)
 *   - recvOK: for receive operations, true if actual value received,
 *             false if zero value from closed channel
 *
 * On SH-4, small structs are returned in r0/r1.
 */
typedef struct
{
    int selected; // Selected case index
    bool recvOK;  // true if receive got actual data, false if closed channel
} SelectGoResult;

/* chanbuf() is now in chan.h (shared with chan.c) */

/*
 * xorshift32 PRNG - fast, good statistical properties.
 * Period 2^32-1 (excludes zero state).
 */
static inline uint32_t xorshift32(uint32_t x)
{
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

/*
 * Per-goroutine random number generator.
 *
 * Lazy-initialized from timer + goid on first use.
 */
static uint32_t fastrand_state = 0;

static uint32_t fastrand(void)
{
    G *gp = getg();

    /* Lazy init - seed from timer and goroutine ID */
    if (fastrand_state == 0)
    {
        uint64_t now = timer_us_gettime64();
        fastrand_state = (uint32_t)(now ^ (now >> 32));
        if (gp != NULL)
            fastrand_state ^= (uint32_t)gp->goid;
        if (fastrand_state == 0)
            fastrand_state = 1;  /* xorshift32 cannot have zero state */
    }

    fastrand_state = xorshift32(fastrand_state);
    return fastrand_state;
}

/**
 * Lock channels in address order to prevent deadlock.
 *
 * @param cases     Array of select cases
 * @param lockorder Array of indices sorted by channel address
 * @param ncases    Number of cases
 */
static void sellock(scase *cases, uint16_t *lockorder, int ncases)
{
    hchan *lastc = NULL;

    for (int i = 0; i < ncases; i++)
    {
        int idx = lockorder[i];
        hchan *c = cases[idx].c;

        if (c == NULL)
            continue;

        // Skip if same as previous (already locked)
        if (c == lastc)
            continue;

        chan_lock(c);
        lastc = c;
    }
}

/**
 * Unlock channels in reverse address order.
 */
static void selunlock(scase *cases, uint16_t *lockorder, int ncases)
{
    for (int i = ncases - 1; i >= 0; i--)
    {
        int idx = lockorder[i];
        hchan *c = cases[idx].c;

        if (c == NULL)
            continue;

        // Only unlock if different from next (don't double unlock)
        if (i < ncases - 1)
        {
            int next_idx = lockorder[i + 1];
            if (c == cases[next_idx].c)
                continue;
        }

        chan_unlock(c);
    }
}

/**
 * Heap sort for lock ordering.
 * Sort by channel address (as uintptr_t).
 */
static void heapsort_lockorder(scase *cases, uint16_t *order, int n)
{
    // Build heap
    for (int i = n / 2 - 1; i >= 0; i--)
    {
        // Heapify
        int parent = i;
        while (true)
        {
            int largest = parent;
            int left = 2 * parent + 1;
            int right = 2 * parent + 2;

            if (left < n &&
                (uintptr_t)cases[order[left]].c > (uintptr_t)cases[order[largest]].c)
            {
                largest = left;
            }
            if (right < n &&
                (uintptr_t)cases[order[right]].c > (uintptr_t)cases[order[largest]].c)
            {
                largest = right;
            }

            if (largest == parent)
                break;

            // Swap
            uint16_t tmp = order[parent];
            order[parent] = order[largest];
            order[largest] = tmp;
            parent = largest;
        }
    }

    // Extract elements
    for (int i = n - 1; i > 0; i--)
    {
        uint16_t tmp = order[0];
        order[0] = order[i];
        order[i] = tmp;

        // Heapify root
        int parent = 0;
        int size = i;
        while (true)
        {
            int largest = parent;
            int left = 2 * parent + 1;
            int right = 2 * parent + 2;

            if (left < size &&
                (uintptr_t)cases[order[left]].c > (uintptr_t)cases[order[largest]].c)
            {
                largest = left;
            }
            if (right < size &&
                (uintptr_t)cases[order[right]].c > (uintptr_t)cases[order[largest]].c)
            {
                largest = right;
            }

            if (largest == parent)
                break;

            tmp = order[parent];
            order[parent] = order[largest];
            order[largest] = tmp;
            parent = largest;
        }
    }
}

/**
 * Park callback argument for select - passed through gopark's lock parameter.
 *
 * Previously this was a global static, which was unsafe if multiple goroutines
 * were in selectgo simultaneously (even with M:1, one could set up state then
 * another could clobber it before the first parked). Now we allocate on stack
 * and pass through the lock parameter.
 */
typedef struct
{
    scase *cases;
    uint16_t *lockorder;
    int ncases;
} selunlock_arg_t;

static bool selparkcommit(void *lock)
{
    selunlock_arg_t *arg = (selunlock_arg_t *)lock;
    selunlock(arg->cases, arg->lockorder, arg->ncases);
    return true;
}

/**
 * selectgo - Main select implementation.
 *
 * @param cas0      Array of select cases
 * @param order0    Array of uint16 for ordering (2*ncases elements)
 * @param nsends    Number of send cases (first nsends in cas0)
 * @param nrecvs    Number of receive cases (after sends)
 * @param block     Whether to block if no case is ready
 * @return          SelectGoResult with:
 *                  - selected: case index, -1 if non-blocking and nothing ready
 *                  - recvOK: for receives, true if actual value, false if closed channel
 */
SelectGoResult selectgo(scase *cas0, uint16_t *order0, int nsends, int nrecvs, bool block)
{
    SelectGoResult result = {-1, false};
    int ncases = nsends + nrecvs;

    if (ncases == 0)
    {
        if (!block)
            return result;
        // Empty select blocks forever
        gopark(NULL, NULL, waitReasonSelect);
        runtime_throw("unreachable");
    }

    // Allocate poll order and lock order arrays
    // order0 has space for 2*ncases: first half is pollorder, second is lockorder
    uint16_t *pollorder = order0;
    uint16_t *lockorder = order0 + ncases;

    // Initialize poll order with random shuffle
    for (int i = 0; i < ncases; i++)
    {
        pollorder[i] = (uint16_t)i;
    }

    // Fisher-Yates shuffle for random poll order
    for (int i = ncases - 1; i > 0; i--)
    {
        int j = fastrand() % (i + 1);
        uint16_t tmp = pollorder[i];
        pollorder[i] = pollorder[j];
        pollorder[j] = tmp;
    }

    // Initialize lock order
    for (int i = 0; i < ncases; i++)
    {
        lockorder[i] = (uint16_t)i;
    }

    // Sort lock order by channel address
    heapsort_lockorder(cas0, lockorder, ncases);

    // Lock all channels
    sellock(cas0, lockorder, ncases);

    G *gp = getg();
    int selected = -1;

    // Pass 1: Check for ready cases
    for (int i = 0; i < ncases; i++)
    {
        int casi = pollorder[i];
        scase *cas = &cas0[casi];
        hchan *c = cas->c;

        if (c == NULL)
            continue;

        if (casi < nsends)
        {
            // Send case
            if (c->closed)
            {
                // Will panic - select this case
                selected = casi;
                break;
            }

            // Check for waiting receiver or buffer space
            if (!waitq_empty(&c->recvq) || c->qcount < c->dataqsiz)
            {
                selected = casi;
                break;
            }
        }
        else
        {
            // Receive case
            // Check for waiting sender, buffer data, or closed
            if (!waitq_empty(&c->sendq) || c->qcount > 0 || c->closed)
            {
                selected = casi;
                break;
            }
        }
    }

    // If we found a ready case, execute it
    if (selected >= 0)
    {
        scase *cas = &cas0[selected];
        hchan *c = cas->c;

        if (selected < nsends)
        {
            // Execute send - recvOK not applicable for sends
            if (c->closed)
            {
                selunlock(cas0, lockorder, ncases);
                runtime_throw("send on closed channel");
            }

            sudog *sg = waitq_dequeue(&c->recvq);
            if (sg != NULL)
            {
                // Direct send to receiver
                if (cas->elem != NULL && c->elemsize > 0)
                {
                    fast_copy(sg->elem, cas->elem, c->elemsize);
                }
                sg->success = true;
                selunlock(cas0, lockorder, ncases);
                goready(sg->g);
                result.selected = selected;
                result.recvOK = false; // Not a receive
                return result;
            }

            // Send to buffer
            void *dst = chanbuf(c, c->sendx);
            if (cas->elem != NULL && c->elemsize > 0)
            {
                fast_copy(dst, cas->elem, c->elemsize);
            }
            c->sendx = (c->sendx + 1) % c->dataqsiz;
            c->qcount++;

            selunlock(cas0, lockorder, ncases);
            result.selected = selected;
            result.recvOK = false; // Not a receive
            return result;
        }
        else
        {
            // Execute receive
            sudog *sg = waitq_dequeue(&c->sendq);
            if (sg != NULL)
            {
                if (c->dataqsiz == 0)
                {
                    // Unbuffered receive from sender
                    if (cas->elem != NULL && c->elemsize > 0)
                    {
                        fast_copy(cas->elem, sg->elem, c->elemsize);
                    }
                }
                else
                {
                    // Buffered: get from buffer, put sender's data in buffer
                    // Queue is full. Take the item at the head of the queue.
                    // Make the sender enqueue its item at the tail of the queue.
                    // Since the queue is full, those are both the same slot.
                    void *src = chanbuf(c, c->recvx);
                    if (cas->elem != NULL && c->elemsize > 0)
                    {
                        fast_copy(cas->elem, src, c->elemsize);
                    }
                    // Copy sender's data to the freed slot
                    fast_copy(src, sg->elem, c->elemsize);
                    c->recvx = (c->recvx + 1) % c->dataqsiz;
                    c->sendx = c->recvx; // CRITICAL: keep sendx in sync
                }
                sg->success = true;
                selunlock(cas0, lockorder, ncases);
                goready(sg->g);
                result.selected = selected;
                result.recvOK = true; // Received actual data from sender
                return result;
            }

            if (c->qcount > 0)
            {
                // Receive from buffer - actual data
                void *src = chanbuf(c, c->recvx);
                if (cas->elem != NULL && c->elemsize > 0)
                {
                    fast_copy(cas->elem, src, c->elemsize);
                }
                memset(src, 0, c->elemsize);
                c->recvx = (c->recvx + 1) % c->dataqsiz;
                c->qcount--;

                selunlock(cas0, lockorder, ncases);
                result.selected = selected;
                result.recvOK = true; // Received actual data from buffer
                return result;
            }

            if (c->closed)
            {
                // Receive zero value from closed channel - NOT actual data
                if (cas->elem != NULL && c->elemsize > 0)
                {
                    memset(cas->elem, 0, c->elemsize);
                }
                selunlock(cas0, lockorder, ncases);
                result.selected = selected;
                result.recvOK = false; // Closed channel, no actual data
                return result;
            }
        }
    }

    // No case ready
    if (!block)
    {
        selunlock(cas0, lockorder, ncases);
        // Yield to allow other goroutines to run.
        // This prevents tight loops with select/default from starving
        // other goroutines in our cooperative scheduler.
        go_yield();
        return result; // selected = -1, recvOK = false
    }

    // Pass 2: Enqueue on all channel wait queues
    sudog *sgnext;
    sudog *sglist = NULL;

    /* selectDone tracking removed - not needed for M:1 scheduling */

    for (int i = 0; i < ncases; i++)
    {
        int casi = pollorder[i];
        scase *cas = &cas0[casi];
        hchan *c = cas->c;

        if (c == NULL)
            continue;

        sudog *sg = acquireSudog();
        if (sg == NULL)
        {
            // Failed to allocate - unwind and return error
            // (Simplified: just unlock and panic)
            selunlock(cas0, lockorder, ncases);
            runtime_throw("select: failed to acquire sudog");
        }

        sg->g = gp;
        sg->c = c;
        sg->elem = cas->elem;
        sg->isSelect = true;
        sg->success = false;

        // Store case index in ticket field (reuse)
        sg->ticket = casi;

        // Link sudogs together for later cleanup
        sg->waitlink = sglist;
        sglist = sg;

        if (casi < nsends)
        {
            waitq_enqueue(&c->sendq, sg);
        }
        else
        {
            waitq_enqueue(&c->recvq, sg);
        }
    }

    // Set up for parking - allocate unlock args on stack and pass through lock param
    gp->waiting = sglist;
    selunlock_arg_t unlock_arg = {
        .cases = cas0,
        .lockorder = lockorder,
        .ncases = ncases
    };

    // Park - pass unlock_arg through the lock parameter
    gopark(selparkcommit, &unlock_arg, waitReasonSelect);

    // Woken up - find which case succeeded
    gp->waiting = NULL;

    // Re-lock all channels
    sellock(cas0, lockorder, ncases);

    selected = -1;
    bool recvOK = false;

    // Pass 3: Find the winning case and dequeue from others
    for (sudog *sg = sglist; sg != NULL; sg = sgnext)
    {
        sgnext = sg->waitlink;

        hchan *c = sg->c;
        int casi = (int)sg->ticket;

        if (sg->success)
        {
            // This case was selected
            selected = casi;
            // For receives, success=true means actual data was received
            // (sender woke us up with data)
            if (casi >= nsends)
            {
                recvOK = true;
            }
        }
        else
        {
            // Dequeue from wait queue
            if (casi < nsends)
            {
                waitq_remove(&c->sendq, sg);
            }
            else
            {
                waitq_remove(&c->recvq, sg);
            }
        }
    }

    // Handle case where we were woken by channel close
    // In this case, sg->success would be false for the receive
    // but we still need to return the selected case
    if (selected >= 0 && selected >= nsends && !recvOK)
    {
        // We were selected for a receive but success=false means
        // channel was closed. Zero the element.
        scase *cas = &cas0[selected];
        hchan *c = cas->c;
        if (c->closed && cas->elem != NULL && c->elemsize > 0)
        {
            memset(cas->elem, 0, c->elemsize);
        }
    }

    // Unlock all channels
    selunlock(cas0, lockorder, ncases);

    // Release all sudogs
    for (sudog *sg = sglist; sg != NULL; sg = sgnext)
    {
        sgnext = sg->waitlink;
        releaseSudog(sg);
    }

    result.selected = selected;
    result.recvOK = recvOK;
    return result;
}

// gccgo entry point
// Returns (int, bool) as a struct - gccgo expects both selected index and recvOK
SelectGoResult runtime_selectgo(scase *cas0, uint16_t *order0, int nsends, int nrecvs, bool block) __asm__("_runtime.selectgo");
SelectGoResult runtime_selectgo(scase *cas0, uint16_t *order0, int nsends, int nrecvs, bool block)
{
    return selectgo(cas0, order0, nsends, nrecvs, block);
}

/**
 * block - Empty select (select {}).
 * Blocks forever.
 */
void block(void)
{
    gopark(NULL, NULL, waitReasonSelect);
    runtime_throw("unreachable");
}

// gccgo entry point
void runtime_block(void) __asm__("_runtime.block");
void runtime_block(void)
{
    block();
}

// NOTE: selectnbrecv is now implemented in chan.c with proper gccgo asm names

// NOTE: selectnbsend is now implemented in chan.c with proper gccgo asm names
