# Chapter 7: Channels and Select

This chapter explains how libgodc implements Go channels for the Dreamcast. The implementation differs significantly from the standard Go runtime due to our M:1 cooperative scheduling model.

---

## The hchan Structure

Every channel is an `hchan` structure allocated on the GC heap:

```c
typedef struct hchan {
    uint32_t qcount;      // Items currently in buffer
    uint32_t dataqsiz;    // Buffer capacity (0 = unbuffered)
    void *buf;            // Ring buffer (follows hchan in memory)
    uint16_t elemsize;    // Size of each element
    uint8_t closed;       // Channel closed flag
    uint8_t buf_mask_valid; // Power-of-2 optimization flag
    
    struct __go_type_descriptor *elemtype;
    
    uint32_t sendx;       // Send index into ring buffer
    uint32_t recvx;       // Receive index into ring buffer
    
    waitq recvq;          // Goroutines waiting to receive
    waitq sendq;          // Goroutines waiting to send
    
    uint8_t locked;       // Simple lock (no contention in M:1)
} hchan;
```

When you write `make(chan int, 3)`, libgodc allocates a single block containing both the `hchan` header and the buffer:

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   Memory Layout for make(chan int, 3)                       │
│                                                             │
│   ┌─────────────────────┬─────────────────────────────────┐ │
│   │      hchan (48B)    │     buffer (3 × 4B = 12B)       │ │
│   ├─────────────────────┼───────┬───────┬───────┬─────────┤ │
│   │ qcount, dataqsiz,   │ [0]   │ [1]   │ [2]   │         │ │
│   │ sendx, recvx,       │ int   │ int   │ int   │         │ │
│   │ waitqueues, ...     │       │       │       │         │ │
│   └─────────────────────┴───────┴───────┴───────┴─────────┘ │
│                                                             │
│   Total allocation: sizeof(hchan) + (cap × elemsize)        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Ring Buffer Indexing

The buffer is a circular queue. To find where to read/write:

```c
static inline void *chanbuf(hchan *c, uint32_t i) {
    uint32_t index = chan_index(c, i);
    return (void *)((uintptr_t)c->buf + (uintptr_t)index * c->elemsize);
}
```

For power-of-2 capacities, we use bitwise AND instead of modulo:

```c
static inline uint32_t chan_index(hchan *c, uint32_t i) {
    if (c->buf_mask_valid)
        return i & (c->dataqsiz - 1);  // Fast: i & 3 for cap=4
    return i % c->dataqsiz;            // Slow: division
}
```

**Tip:** Use power-of-2 buffer sizes (2, 4, 8, 16...) for faster indexing.

---

## The Send Algorithm

When you write `ch <- value`, this is `chansend()`:

```
┌─────────────────────────────────────────────────────────────┐
│   chansend(c, elem, block)                                  │
│                                                             │
│   1. nil channel?                                           │
│      └── block=true: gopark forever (deadlock)              │
│      └── block=false: return false                          │
│                                                             │
│   2. Channel closed?                                        │
│      └── runtime_throw("send on closed channel")            │
│                                                             │
│   3. Receiver waiting in recvq?                             │
│      └── YES: Copy data DIRECTLY to receiver's elem         │
│               Wake receiver with goready()                  │
│               Return true                                   │
│                                                             │
│   4. Buffer has space? (qcount < dataqsiz)                  │
│      └── YES: Copy to buf[sendx], increment sendx           │
│               Return true                                   │
│                                                             │
│   5. Non-blocking? (block=false)                            │
│      └── Return false                                       │
│                                                             │
│   6. Must block:                                            │
│      └── Create sudog, enqueue in sendq                     │
│          gopark() - yield to scheduler                      │
│          When woken: return success flag                    │
└─────────────────────────────────────────────────────────────┘
```

The key insight: **direct transfer**. If a receiver is already waiting, we copy data straight to their memory location, bypassing the buffer entirely. This is why unbuffered channels involve no buffer at all.

---

## The Receive Algorithm

When you write `value := <-ch`, this is `chanrecv()`:

```
┌─────────────────────────────────────────────────────────────┐
│   chanrecv(c, elem, block)                                  │
│                                                             │
│   1. nil channel?                                           │
│      └── block=true: gopark forever                         │
│      └── block=false: return false                          │
│                                                             │
│   2. Closed AND empty?                                      │
│      └── Zero out elem, return (true, received=false)       │
│                                                             │
│   3. Sender waiting in sendq?                               │
│      └── Unbuffered: Copy directly from sender's elem       │
│      └── Buffered: Take from buffer, move sender's data in  │
│          Wake sender with goready()                         │
│          Return (true, received=true)                       │
│                                                             │
│   4. Buffer has data? (qcount > 0)                          │
│      └── Copy from buf[recvx], zero slot, decrement qcount  │
│          Return (true, received=true)                       │
│                                                             │
│   5. Non-blocking?                                          │
│      └── Return false                                       │
│                                                             │
│   6. Must block:                                            │
│      └── Create sudog, enqueue in recvq                     │
│          gopark()                                           │
│          When woken: return success                         │
└─────────────────────────────────────────────────────────────┘
```

### The Buffered Receive with Waiting Sender

This case is subtle. When the buffer is full and a sender is waiting:

```c
if (c->dataqsiz > 0) {  // Buffered channel
    // 1. Take oldest item from buffer for receiver
    src = chanbuf(c, c->recvx);
    chan_copy(c, elem, src);
    
    // 2. Put sender's NEW item into the freed slot
    chan_copy(c, src, sg->elem);
    
    // 3. Advance indices (sendx follows recvx)
    c->recvx = chan_index(c, c->recvx + 1);
    c->sendx = c->recvx;
}
```

This maintains FIFO order: the receiver gets the *oldest* buffered value, not the sender's new value.

---

## Wait Queues and Sudogs

When a goroutine blocks on a channel, it creates a **sudog** (sender/receiver descriptor):

```c
typedef struct sudog {
    G *g;                // The blocked goroutine
    struct sudog *next;  // Next in wait queue
    struct sudog *prev;  // Previous in wait queue
    void *elem;          // Pointer to data being sent/received
    uint64_t ticket;     // Used by select for case index
    bool isSelect;       // Part of a select statement?
    bool success;        // Did operation succeed?
    struct sudog *waitlink;   // For select: links all sudogs
    struct sudog *releasetime; // Unused (Go runtime compat)
    struct hchan *c;     // Channel we're waiting on
} sudog;
```

### The Sudog Pool

Creating sudogs during gameplay would trigger `malloc()`. libgodc pre-allocates a pool at startup:

```c
void sudog_pool_init(void) {
    for (int i = 0; i < 16; i++) {
        sudog *s = (sudog *)malloc(sizeof(sudog));
        s->next = global_pool;
        global_pool = s;
    }
}
```

`acquireSudog()` pulls from the pool; `releaseSudog()` returns to it. If the pool is exhausted, we fall back to `malloc()`.

### Wait Queues

Each channel has two wait queues (doubly-linked lists):

```c
typedef struct waitq {
    struct sudog *first;
    struct sudog *last;
} waitq;
```

Operations:
- `waitq_enqueue()` - add blocked goroutine to end
- `waitq_dequeue()` - remove and return first goroutine
- `waitq_remove()` - remove specific sudog (for select cancellation)

---

## Blocking and Waking: gopark/goready

This is where libgodc's M:1 model shines.

### gopark() - Block Current Goroutine

```c
void gopark(bool (*unlockf)(void *), void *lock, WaitReason reason) {
    G *gp = getg();
    if (!gp || gp == g0)
        runtime_throw("gopark on g0 or nil");

    gp->atomicstatus = Gwaiting;
    gp->waitreason = reason;

    // Call unlock function - if it returns false, abort parking
    if (unlockf && !unlockf(lock)) {
        gp->atomicstatus = Grunnable;
        runq_put(gp);
        return;
    }

    // Context switch to scheduler
    __go_swapcontext(&gp->context, &sched_context);
}
```

The goroutine saves its context and swaps to the scheduler. The `unlockf` callback releases the channel lock atomically with parking - if it returns false, we abort and re-enqueue instead.

### goready() - Wake a Goroutine

```c
void goready(G *gp) {
    if (!gp) return;

    // Don't wake dead/already-runnable/running goroutines
    Gstatus status = gp->atomicstatus;
    if (status == Gdead || status == Grunnable || status == Grunning)
        return;

    gp->atomicstatus = Grunnable;
    gp->waitreason = waitReasonZero;
    runq_put(gp);
}
```

The woken goroutine becomes runnable and will be scheduled on the next `schedule()` call.

### Why M:1 Simplifies Things

In standard Go, channels need atomic operations and memory barriers because multiple OS threads access them. libgodc runs all goroutines on one KOS thread:

- No atomics needed for `locked` flag (simple bool)
- No memory barriers
- No contention on wait queues
- Context switches are explicit (cooperative)

The `chan_lock()`/`chan_unlock()` functions just set a flag:

```c
void chan_lock(hchan *c) {
    if (!c)
        runtime_throw("chan: nil channel");
    if (c->locked)
        runtime_throw("chan: recursive lock");
    c->locked = 1;
}

void chan_unlock(hchan *c) {
    if (c) c->locked = 0;
}
```

This is safe because we never preempt a goroutine in the middle of a channel operation.

---

## Select Implementation

Select is the most complex part. Here's how `selectgo()` works:

### Phase 1: Setup

```c
SelectGoResult selectgo(scase *cas0, uint16_t *order0, 
                        int nsends, int nrecvs, bool block) {
    int ncases = nsends + nrecvs;
    
    // order0 provides space for two arrays:
    uint16_t *pollorder = order0;           // Random order to check cases
    uint16_t *lockorder = order0 + ncases;  // Order to lock channels
```

### Phase 2: Randomize Poll Order (Fairness)

```c
// Fisher-Yates shuffle
for (int i = ncases - 1; i > 0; i--) {
    int j = fastrand() % (i + 1);
    uint16_t tmp = pollorder[i];
    pollorder[i] = pollorder[j];
    pollorder[j] = tmp;
}
```

Why random? If we always checked cases in order, the first case would always win when multiple are ready. Randomization ensures fairness.

### Phase 3: Lock Channels (Deadlock Prevention)

```c
// Sort by channel address using heap sort
heapsort_lockorder(cas0, lockorder, ncases);

// Lock in address order
sellock(cas0, lockorder, ncases);
```

If goroutine A does `select { case <-ch1: case <-ch2: }` and goroutine B does `select { case <-ch2: case <-ch1: }`, they could deadlock if they lock in different orders. Sorting by address ensures everyone locks in the same global order.

### Phase 4: Check for Ready Cases

```c
for (int i = 0; i < ncases; i++) {
    int casi = pollorder[i];  // Check in random order
    scase *cas = &cas0[casi];
    hchan *c = cas->c;
    
    if (c == NULL)
        continue;
    
    if (casi < nsends) {
        // Send: closed channel will panic - select it
        if (c->closed) {
            selected = casi;
            break;
        }
        // Check for waiting receiver or buffer space
        if (!waitq_empty(&c->recvq) || c->qcount < c->dataqsiz) {
            selected = casi;
            break;
        }
    } else {
        // Receive: check for waiting sender, buffer data, or closed
        if (!waitq_empty(&c->sendq) || c->qcount > 0 || c->closed) {
            selected = casi;
            break;
        }
    }
}
```

If any case is ready, execute it immediately and return.

### Phase 5: Block on All Channels

If nothing is ready and `block=true`, we enqueue on ALL channels:

```c
sudog *sglist = NULL;

for (int i = 0; i < ncases; i++) {
    int casi = pollorder[i];
    scase *cas = &cas0[casi];
    hchan *c = cas->c;
    
    if (c == NULL)
        continue;
    
    sudog *sg = acquireSudog();
    sg->g = gp;
    sg->c = c;
    sg->elem = cas->elem;
    sg->isSelect = true;
    sg->success = false;
    sg->ticket = casi;  // Remember which case this is
    
    // Link for later cleanup
    sg->waitlink = sglist;
    sglist = sg;
    
    if (casi < nsends)
        waitq_enqueue(&c->sendq, sg);
    else
        waitq_enqueue(&c->recvq, sg);
}

gp->waiting = sglist;
gopark(selparkcommit, &unlock_arg, waitReasonSelect);
```

### Phase 6: Woken - Find Winner

When woken, one sudog has `success=true`. Find it and dequeue from all other channels:

```c
// Pass 3: Find winner and dequeue losers
for (sudog *sg = sglist; sg != NULL; sg = sgnext) {
    sgnext = sg->waitlink;  // Save before we might release
    int casi = (int)sg->ticket;
    
    if (sg->success) {
        selected = casi;
        if (casi >= nsends)
            recvOK = true;  // Received actual data
    } else {
        // Remove from wait queue (we won't use this case)
        if (casi < nsends)
            waitq_remove(&sg->c->sendq, sg);
        else
            waitq_remove(&sg->c->recvq, sg);
    }
}

// Release all sudogs in separate pass
for (sudog *sg = sglist; sg != NULL; sg = sgnext) {
    sgnext = sg->waitlink;
    releaseSudog(sg);
}
```

### The Default Case

When `block=false` and nothing is ready, `selectgo()` returns `selected=-1`:

```c
if (!block) {
    selunlock(cas0, lockorder, ncases);
    go_yield();  // Give other goroutines a chance
    return (SelectGoResult){-1, false};
}
```

The `go_yield()` prevents tight polling loops from starving other goroutines.

---

## Closing Channels

`closechan()` marks the channel closed and wakes ALL waiting goroutines:

```c
void closechan(hchan *c) {
    G *wake_list = NULL;
    G *wake_tail = NULL;
    
    chan_lock(c);
    
    if (c->closed) {
        chan_unlock(c);
        runtime_throw("close of closed channel");
    }
    
    c->closed = 1;
    
    // Collect all receivers (they'll get zero values)
    while ((sg = waitq_dequeue(&c->recvq)) != NULL) {
        sg->success = false;  // Indicates closed, not real data
        gp = sg->g;
        if (!gp || gp->atomicstatus == Gdead)
            continue;
        if (sg->elem && c->elemsize > 0)
            memset(sg->elem, 0, c->elemsize);
        // Add gp to wake_list via schedlink...
    }
    
    // Collect all senders (they'll panic when they wake)
    while ((sg = waitq_dequeue(&c->sendq)) != NULL) {
        sg->success = false;
        gp = sg->g;
        if (!gp || gp->atomicstatus == Gdead)
            continue;
        // Add gp to wake_list via schedlink...
    }
    
    chan_unlock(c);
    
    // Wake everyone outside the lock
    while (wake_list) {
        gp = wake_list;
        wake_list = gp->schedlink;
        goready(gp);
    }
}
```

Senders check `success` when they wake and throw "send on closed channel" if false.

---

## Performance

For benchmark numbers, see the [Performance section in Design](../reference/design.md#performance). You can run the benchmarks yourself with `tests/bench_architecture.elf` on hardware.

### Why Unbuffered is Slower

Unbuffered channels always require a context switch:

```
Sender                          Receiver
──────                          ────────
ch <- 42                        
  │                             
  └── gopark() ─────────────────► scheduler picks receiver
                                       │
                                  x := <-ch
                                       │
  ◄── goready() ────────────────── wakes sender
  │
continues
```

Buffered channels avoid this when buffer has space/data.

### Optimization Tips

1. **Use buffered channels** for producer/consumer patterns
2. **Power-of-2 buffer sizes** for faster indexing (uses bitwise AND instead of modulo)
3. **Batch data** - send structs with multiple values instead of multiple sends
4. **select with default** for non-blocking checks in game loops
5. **Pre-warm channels** - send/receive once during init to allocate sudogs

---

## Limitations

libgodc channels have some constraints:

| Limit | Value | Reason |
|-------|-------|--------|
| Max buffer size | 65536 elements | Sanity check in `makechan()` |
| Max element size | 65536 bytes | 16-bit `elemsize` field in hchan |
| Sudog pool | 16 pre-allocated, 128 max | Defined in `godc_config.h` |

For game code, these limits are rarely hit. If you need larger queues, consider using slices with your own synchronization.
