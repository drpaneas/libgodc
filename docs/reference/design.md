# libgodc Design

libgodc is a Go runtime for the Sega Dreamcast. This document explains how it
works under the hood.

## The Problem

The Dreamcast is a fixed platform: 200MHz SH-4, 16MB RAM, no MMU, no swap.
The standard Go runtime assumes infinite memory, preemptive scheduling,
operating system threads, and virtual memory. None of these exist here.

libgodc replaces the Go runtime with one designed for this environment.

## Architecture

```go
┌────────────────────────────────────────────────────────────────┐
│  Your Go Code                                                  │
│     compiles with sh-elf-gccgo                                 │
│     produces .o files with Go runtime calls                    │
├────────────────────────────────────────────────────────────────┤
│  libgodc (this library)                                        │
│     implements Go runtime functions                            │
│     memory allocation, goroutines, channels, GC                │
├────────────────────────────────────────────────────────────────┤
│  KallistiOS (KOS)                                              │
│     baremetal OS for Dreamcast                                 │
│     provides malloc, threads, drivers                          │
├────────────────────────────────────────────────────────────────┤
│  Dreamcast Hardware                                            │
│     SH4 CPU, PowerVR2 GPU, AICA sound                          │
│     16MB main RAM, 8MB VRAM                                    │
└────────────────────────────────────────────────────────────────┘
```

We don't need the full Go runtime. We need enough to run games. Games have
different requirements than servers—short sessions, realtime deadlines, no
network services. This simplifies everything.

## Memory Model

### The Budget

```
16MB total RAM:
 KOS kernel + drivers:     ~1MB
 Your program text/data:   ~13MB
 GC heap (two semispaces): 4MB (2MB active at any time)
 Goroutine stacks:         ~640KB (10 goroutines × 64KB)
 Channel buffers:          Variable
 Available for KOS malloc: ~6-9MB (textures, audio, meshes)
```

The number are from the source code config:
 GC heap: `GC_SEMISPACE_SIZE_KB` in `godc_config.h` (default 2048 = 2MB × 2)
 Stack size: `GOROUTINE_STACK_SIZE` in `godc_config.h` (default 64KB)
 Run `bench_architecture.elf` to verify: prints actual config values

The 16MB limit is absolute. There is no virtual memory, no swap, no second
chance. Every byte matters.

### Allocation Strategy

libgodc uses three allocation paths:

**1. GC Heap (for Go objects)**

Small, frequentlyallocated objects go here. The semispace collector manages
them automatically. Implementation: `gc_heap.c`, `gc_copy.c`.

Implementation of the allocation in simple pseudocode:

```c
// Bump allocator: O(1) allocation (simplified)
void *gc_alloc(size_t size, type_descriptor *type) {
    size = ALIGN(size + HEADER_SIZE, 8);
    if (alloc_ptr + size > alloc_limit) {
        gc_collect();  // Cheney's algorithm
    }
    void *obj = alloc_ptr;
    alloc_ptr += size;
    return obj;
}
```go

This is simplified. The real code in `gc_heap.c` also handles large objects
(>64KB bypass the GC heap and go straight to malloc), alignment edge cases,
and gc_percent threshold checks. But the core is exactly this: bump a pointer.

The bump allocator is the fastest possible allocation strategy. Deallocation
happens during collectionlive objects are copied, dead objects are forgotten.

Usage example:

```go
// Go: allocate freely, GC handles cleanup
func spawnEnemy() *Enemy {
    return &Enemy{bullets: make([]Bullet, 100)}
}
// No kill function needed  when nothing references it, it's collected
```

**2. KOS Heap (for large objects)**

Objects larger than 64KB bypass the GC entirely. This is correct for game
assetstextures, audio buffers, and mesh data are typically loaded once and
never freed during gameplay.

```go
// This goes to KOS malloc, not GC:
texture := make([]byte, 256*256*2)  // 128KB texture
```c

Large objects use `malloc()` internally and are not tracked by the GC.
To free them, use `runtime.FreeExternal`:

```go
//go:linkname freeExternal runtime.FreeExternal
func freeExternal(ptr unsafe.Pointer)

// Allocate large texture
texture := make([]byte, 256*256*2)  // 128KB, bypasses GC

// When done with it:
freeExternal(unsafe.Pointer(&texture[0]))
texture = nil  // Don't use after freeing!
```

See `gc_external_free` in `gc_heap.c`. Run `test_free_external.elf` to verify.

Typical pattern  swap textures between levels:

```go
// Load level 1
bgTexture := make([]byte, 512*512*2)  // 512KB

// ... play level 1 ...

// Unload before level 2
freeExternal(unsafe.Pointer(&bgTexture[0]))
bgTexture = make([]byte, 512*512*2)  // reuses memory

// or you could use a helper function, like that:
func freeSlice(s []byte) {
    if len(s) > 0 {
        freeExternal(unsafe.Pointer(&s[0]))
    }
}

// Then just:
freeSlice(bgTexture)
```

**3. Stack (for goroutine execution)**

Each goroutine gets a fixed 64KB stack. No stack growth, no splitstack.
This is simpler and faster than growable stacks, but requires discipline.

Stack frames are freed automatically when functions return. Use the stack
for temporary buffers:

```go
func processAudio() {
    buffer := [4096]int16{}  // 8KB on stack, automatically freed
    // ...
}
```

### Object Header

Every GC object has an 8byte header. The GC needs to know each object's
size (to copy it) and whether it contains pointers (to scan them). Storing
this inline costs 8 bytes per object but makes lookup instant (`ptr  8`).

```
┌──────────────────────────────────────────────────────────┐
│  Bits 31: Forwarded (1 = copied during GC)               │
│  Bits 30: NoScan (1 = no pointers)                       │
│  Bits 29-24: Type tag (6 bits, Go type kind)             │
│  Bits 23-0: Size (24 bits, max 16MB)                     │
├──────────────────────────────────────────────────────────┤
│  Type pointer (32 bits, full type descriptor)            │
└──────────────────────────────────────────────────────────┘
```

Putting numbers on the paper, a `[4]byte` array actually uses _not 4_ but 12
bytes (4 data + 8 header). This is why many small allocations hurt more than
fewer large ones.

The NoScan bit is critical for performance. Objects containing only integers,
floats, or other nonpointer types skip GC scanning entirelythe collector
just copies them without inspecting their contents.

The practical takeaway: prefer value types over pointer types when possible.

```go
// Faster GC (NoScan), just a copy:
type Vertex struct { X, Y, Z float32 }
mesh := make([]Vertex, 1000)

// Slower GC (must scan), has pointers:
mesh := make([]*Vertex, 1000)
```

## Garbage Collection

### Algorithm: Cheney's SemiSpace Collector

The heap is divided into two semispaces of equal size. Only one is active
at any time. When the active space fills up:

1. Stop all goroutines (stoptheworld)
2. Copy all live objects to the other space
3. Update all pointers to point to new locations
4. Switch active space
5. Resume execution

```go
// Two semispaces
gc_heap.space[0] = memalign(32, GC_SEMISPACE_SIZE);
gc_heap.space[1] = memalign(32, GC_SEMISPACE_SIZE);

// Collection switches active space
int old_space = gc_heap.active_space;
int new_space = 1  old_space;
gc_heap.active_space = new_space;

// Copy to new space, scan roots, update pointers
gc_scan_roots();
// ... Cheney's forwarding loop ...
```

This algorithm is simple, has no fragmentation, and handles cycles naturally.
The cost is that only half the heap is usable at any time.

### Collection Trigger

GC runs when:
 Active space exceeds threshold (default: 75% when `gc_percent=100`)
 Allocation would exceed remaining space
 Explicit GC call

The threshold is controlled by `gc_percent`:
- `gc_percent = 100` (default): threshold = 75% of heap space
- `gc_percent = 50`: threshold = 50% of heap space  
- `gc_percent = -1`: disable automatic GC (only explicit `runtime.GC()` triggers collection)

To control GC from Go:

```go
//go:linkname setGCPercent debug.SetGCPercent
func setGCPercent(percent int32) int32

//go:linkname gc runtime.GC
func gc()

func init() {
    setGCPercent(50)   // Trigger at 50% instead of 75%
    setGCPercent(-1)   // Disable automatic GC entirely
    gc()               // Force collection now
}
```

Run `test_gc_percent.elf` to verify this works.

### Pause Times

GC pause time depends on live object count and layout. Run
`tests/bench_architecture.elf` on hardware to measure actual pauses.

For 60fps (16.6ms frames), disable automatic GC during gameplay:

```go
import _ "unsafe"

//go:linkname setGCPercent debug.SetGCPercent
func setGCPercent(percent int32) int32

//go:linkname forceGC runtime.GC
func forceGC()

func main() {
    setGCPercent(-1)  // Disable automatic GC
    
    // ... game runs with no GC pauses ...
    
    // GC during loading screens only:
    showLoadingScreen()
    forceGC()
    startGameplay()
}
```

### Root Scanning

The GC finds live objects by tracing from roots:

```c
static void gc_scan_roots(void)
{
    // Scan explicit roots (gc_add_root)
    for (int i = 0; i < gc_root_table.count; i++) { ... }

    // Scan compilerregistered roots (registerGCRoots)
    gc_scan_compiler_roots();

    // Scan current stack
    gc_scan_stack();

    // Scan all goroutine stacks
    gc_scan_all_goroutine_stacks();
}
```

1. **Global variables**  Registered by gccgogenerated code via
   `registerGCRoots()`. Each package contributes a root list.

2. **Goroutine stacks**  Scanned conservatively. Every aligned pointersized
   value that points into the heap is treated as a potential pointer.

3. **Explicit roots**  Optional. If you write C code that holds pointers to
   Go objects, call `gc_add_root(&ptr)` so the GC doesn't collect them.

### DMA Hazard

The GC moves objects. Any pointer held by hardware (PVR DMA, AICA) will become
stale after collection. Safe patterns:

```go
// DANGEROUS  GC might move buffer during DMA:
data := make([]byte, 4096)     // Small, in GC heap
startDMA(data)                  // Hardware holds pointer
runtime.Gosched()               // GC might run here!

// SAFE  Large allocations bypass GC:
data := make([]byte, 100*1024)  // >64KB, uses malloc
startDMA(data)                  // Won't move

// SAFE  VRAM for textures:
tex := kos.PvrMemMalloc(size)   // Allocates in VRAM
```

## Scheduler

### M:1 Cooperative Model

All goroutines run on a single KOS thread. One goroutine executes at a time.
Context switches happen only at explicit yield points:

 Channel operations (send, receive, select)
 `runtime.Gosched()`
 `time.Sleep()` and timer waits
 Blocking I/O

A goroutine in a tight CPU loop will monopolize the processor. There is no
preemption.

### Why M:1?

The Dreamcast has one CPU core. Preemptive scheduling adds complexity and
overhead for no parallelism benefit. Cooperative scheduling is simpler,
faster, and sufficient for games.

### Run Queue Structure

The scheduler maintains a simple FIFO run queue. Goroutines are added to
the tail and removed from the head. This is simpler than prioritybased
scheduling and sufficient for game workloads where you control when each
goroutine yields.

```c
// Goroutines execute in the order they become runnable
runq_put(gp);   // Add to tail
gp = runq_get(); // Remove from head
```

For realtime requirements, structure your code so timesensitive work
runs on the main goroutine or yields frequently.

### Context Switching

Each goroutine saves 64 bytes of CPU state when it yields:

```c
typedef struct sh4_context {
    uint32_t r8, r9, r10, r11, r12, r13, r14;  // Calleesaved
    uint32_t sp, pr, pc;                        // Special registers
    uint32_t fr12, fr13, fr14, fr15;           // FPU calleesaved
    uint32_t fpscr, fpul;                       // FPU control
} sh4_context_t;
```

Context switch is implemented in `runtime_sh4_minimal.S` (simplified for brevity):

```asm
__go_swapcontext:
    ! Save current context
    mov.l   r8, @r4         ! r4 = old_ctx
    mov.l   r9, @(4, r4)
    ...
    ! Restore new context
    mov.l   @r5, r8         ! r5 = new_ctx
    mov.l   @(4, r5), r9
    ...
    rts
```

### FPU Context

Every context switch saves floatingpoint registers, even if your goroutine
only uses integers. This costs ~50 extra cycles per switch.

```go
// Both goroutines pay FPU overhead, even though neither uses floats
go audioDecoder()   // Integer PCM math
go networkHandler() // Packet parsing
```

This is a tradeoff: always saving FPU is slower but correct. A goroutine
that unexpectedly uses a float won't corrupt another's FPU state.

## Goroutine Structure

```c
typedef struct G {
    // ABICRITICAL: gccgo expects these at specific offsets
    PanicRecord *_panic;      // Offset 0: innermost panic
    GccgoDefer *_defer;       // Offset 4: innermost defer

    // Scheduling
    Gstatus atomicstatus;
    G *schedlink;
    void *param;

    // Stack
    void *stack_lo;
    void *stack_hi;
    stack_segment_t *stack;
    void *stack_guard;
    tls_block_t *tls;

    // CPU context (64 bytes)
    sh4_context_t context;

    // Metadata
    int64_t goid;
    WaitReason waitreason;
    int32_t allgs_index;
    uint32_t death_generation;
    G *dead_link;
    uint8_t gflags2;

    // Channel wait
    sudog *waiting;

    // Defer/panic
    Checkpoint *checkpoint;
    int defer_depth;

    // Entry point
    uintptr_t startpc;
    G *freeLink;
} G;
```

See `goroutine.h` for the authoritative definition.

### Goroutine Lifecycle

1. **Creation**  `__go_go()` allocates G struct, stack, and TLS block
2. **Runnable**  Added to run queue
3. **Running**  Scheduler switches context to it
4. **Waiting**  Parked on channel, timer, or I/O
5. **Dead**  Function returned, queued for cleanup

Dead goroutines are reclaimed after a grace period (epochbased reclamation)
to ensure no dangling sudog references from channel wait queues.

## Channels

Channels are the primary synchronization primitive. Implementation follows
the Go runtime closely.

### Structure

```c
typedef struct hchan {
    uint32_t qcount;        // Current element count
    uint32_t dataqsiz;      // Buffer size (0 = unbuffered)
    void *buf;              // Circular buffer
    uint16_t elemsize;      // Element size
    uint8_t closed;         // Channel closed flag
    uint8_t buf_mask_valid; // Optimization: can use & instead of %
    struct __go_type_descriptor *elemtype;
    uint32_t sendx, recvx;  // Buffer indices
    waitq recvq, sendq;     // Wait queues (sudog linked lists)
    uint8_t locked;         // Simple lock flag
} hchan;
```

### Unbuffered Channels

Send blocks until a receiver arrives. Receive blocks until a sender arrives.
When both are ready, data transfers directlyno buffering.

This is the fundamental synchronization primitive: rendezvous.

### Buffered Channels

Send blocks only when buffer is full. Receive blocks only when buffer is
empty. The buffer is a simple circular array.

### Select

Select uses randomized ordering to prevent starvation:

```go
select {
case x := <ch1:  // These are checked in random order
case ch2 < y:
case <time.After(timeout):
}
```

Implementation: shuffle cases, check each for readiness, park on all
if none ready.


## Defer, Panic, Recover

### Defer

Defer uses a linked list per goroutine. Each `defer` statement pushes a
record; function exit pops and executes them in LIFO order.

```c
typedef struct GccgoDefer {
    struct GccgoDefer *link;    // Next entry in defer stack
    bool *frame;                // Pointer to caller's frame bool
    PanicRecord *panicStack;    // Panic stack when deferred
    PanicRecord *_panic;        // Panic that caused defer to run
    uintptr_t pfn;              // Function pointer to call
    void *arg;                  // Argument to pass to function
    uintptr_t retaddr;          // Return address for recover matching
    bool makefunccanrecover;    // MakeFunc recover permission
    bool heap;                  // Whether heap allocated
} GccgoDefer;  // 32 bytes total
```go

### Panic and Recover

Userinitiated panic (`panic()`) is recoverable via `recover()` in a deferred
function. Implementation uses `setjmp`/`longjmp` with checkpoints.

Runtime panics (nil dereference, bounds check, divide by zero) are not
recoverablethey crash immediately with a diagnostic.

Why? Recovering from a bounds check failure would leave the program in an
undefined state. It's better to crash clearly than corrupt silently.

## Type System

### Type Descriptors

gccgo generates type descriptors for every Go type. libgodc uses these for:

 GC pointer scanning (which fields contain pointers?)
 Interface method dispatch (which methods does this type implement?)
 Reflection (what is this type's name and structure?)

```c
typedef struct __go_type_descriptor {
    uint8_t __code;              // Kind (bool, int, slice, etc.)
    uint8_t __align, __field_align;
    uintptr_t __size;
    uint32_t __hash;
    uintptr_t __ptrdata;         // Bytes containing pointers
    const void *__gcdata;        // Pointer bitmap
    // ...
} __go_type_descriptor;
```

### Interface Tables

Interface dispatch uses precomputed method tables. When you write:

```go
var w io.Writer = os.Stdout
w.Write(data)
```

The compiler generates an itab linking `*os.File` to `io.Writer`, containing
function pointers for all interface methods.

## SH4 Specifics

### Register Allocation

 **r0r7**: Callersaved (arguments, scratch)
 **r8r14**: Calleesaved (preserved across calls)
 **r15**: Stack pointer
 **pr**: Procedure return (return address)
 **GBR**: Reserved for KOS `_Thread_local`

We do not use GBR for goroutine TLS. Instead, we use a global `current_g`
pointer. This avoids conflicts with KOS and simplifies context switching.

### FPU Mode

libgodc uses singleprecision mode (`m4single`). The SH4 FPU is fast in
singleprecision but slow in doubleprecision. All `float64` operations
generate software emulation callsavoid them in hot paths.

### Cache Considerations

The SH4 has 32byte cache lines. Context switching saves/restores 64 bytes
of CPU state (2 cache lines).

DMA operations require explicit cache management. The GC handles this for
its semispace flip, but user code doing DMA must use KOS cache functions:

```c
#include <arch/cache.h>

dcache_flush_range((uintptr_t)ptr, size);  // Flush before DMA write
dcache_inval_range((uintptr_t)ptr, size);  // Invalidate after DMA read
```

## File Organization

```
runtime/
├── gc_heap.c           # Heap initialization, allocation
├── gc_copy.c           # Cheney's copying collector
├── gc_runtime.c        # Go runtime interface (newobject, etc.)
├── scheduler.c         # Run queue, schedule(), goready()
├── proc.c              # Goroutine creation, lifecycle
├── chan.c              # Channel implementation
├── select.c            # Select statement
├── sudog.c             # Wait queue entries
├── defer_dreamcast.c   # Defer/panic/recover
├── timer.c             # Time.Sleep, timers
├── tls_sh4.c           # TLS management
├── runtime_sh4_minimal.S  # Context switching assembly
├── interface_dreamcast.c  # Interface dispatch
├── map_dreamcast.c     # Map implementation
├── goroutine.h         # Core data structures
├── gen-offsets.c       # Generates struct offset definitions
└── asm-offsets.h       # Auto-generated struct offsets for assembly
```

## AssemblyC ABI Synchronization

### The Problem

Context switching is implemented in assembly (`runtime_sh4_minimal.S`). The assembly
code accesses G struct fields by hardcoded byte offsets:

```asm
mov.l   @(32, r4), r0    ! Load G>context at offset 32
```

If someone changes the G struct in C (adds/removes/reorders fields), the assembly
breaks silentlyit reads garbage from wrong offsets. This is a classic embedded
systems bug: C struct layout changes invisibly break handwritten assembly.

### The Solution

We use a threelayer defense:

**1. Generated Header (`asm-offsets.h`)**

`gen-offsets.c` uses `offsetof()` to emit the actual struct offsets:

```c
// genoffsets.c
OFFSET(G_CONTEXT, G, context);  // Emits: #define G_CONTEXT 32
```

The Makefile compiles this to assembly, extracts the `#define` lines, and writes
`asm-offsets.h`. This header is committed to git.

**2. Build-Time Verification (`make check-offsets`)**

Before release, run:

```bash
make check-offsets
```

This regenerates the offsets from the current struct and diffs against the
committed header. If they don't match, the build fails with a clear error.

**3. Runtime Verification (`scheduler.c`)**

At startup, the scheduler verifies critical offsets:

```c
if (offsetof(G, context) != G_CONTEXT) {
    runtime_throw("G struct layout mismatch - update asm-offsets.h");
}
```

If somehow a mismatched binary runs, it crashes immediately with a diagnostic
instead of silently corrupting goroutine state.

### Workflow for Changing G Struct

1. Modify `runtime/goroutine.h` (the authoritative definition)
2. Update `runtime/gen-offsets.c` to match
3. Run `make check-offsets` — it will fail if out of sync
4. Run `make runtime/asm-offsets.h` to regenerate
5. Update `runtime/runtime_sh4_minimal.S` if `G_CONTEXT` changed
6. Run `make check-offsets` again — should pass now
7. Commit all changed files together

### Why This Matters

In games, struct layout bugs cause symptoms like:

 Goroutines resume with corrupted registers
 Context switches overwrite random memory
 FPU state leaks between goroutines
 Panics with nonsensical stack traces

These are nearly impossible to debug. The offset verification catches them at
build time (or worst case, at startup) instead of during the final boss fight.

## Performance

Measured on real Dreamcast hardware (SH4 @ 200MHz), verified December 2025:

| Operation              | Time      | Notes                          |
|                        |           |                                |
| Gosched yield          | 120 ns    | Minimal scheduler roundtrip    |
| Direct call            | 140 ns    | Baseline comparison            |
| Buffered channel op    | ~1.5 μs   | Send to ready receiver         |
| Context switch         | ~6.6 μs   | Full goroutine switch          |
| Unbuffered channel     | ~13 μs    | Send + receive roundtrip       |
| Goroutine spawn        | ~34 μs    | Create + schedule + run        |
| GC pause (bypass)      | ~73 μs    | Objects ≥64KB bypass GC        |
| GC pause (64KB live)   | ~2.2 ms   | Medium live set                |
| GC pause (32KB live)   | ~6.2 ms   | Many small objects             |

Run `tests/bench_architecture.elf` to measure on your hardware.

> **Note:** For a complete reference of performance numbers, see the [Glossary](../appendix/glossary.md#performance-numbers).

## Design Decisions

**Why gccgo instead of gc?**

The standard Go compiler (gc) generates code for a completely different
runtime. gccgo uses GCC's backend, which already supports SH4 targets.
We replace libgo with libgodc; the compiler doesn't need modification.

**Why semispace instead of marksweep?**

Semispace has no fragmentation. In a 16MB system, fragmentation would
eventually make large allocations impossible even with free memory.
The 50% space overhead is acceptable for games.

**Why cooperative instead of preemptive?**

Preemptive scheduling requires timer interrupts, signal handling, and
safepoint insertion. All of this complexity gains nothing on a singlecore
CPU. Cooperative scheduling is simpler, faster, and sufficient.

**Why fixed stacks instead of growable?**

Growable stacks require compiler support (stack probes) and runtime support
(morestack). Fixed stacks work with any compiler flags and simplify the
runtime. 64KB is enough for typical game code.

## References

 Cheney, C.J. "A Nonrecursive List Compacting Algorithm." CACM, 1970.
 Jones & Lins. "Garbage Collection." Wiley, 1996.
 The Go Programming Language Specification.
 KallistiOS Documentation.
 SH4 Software Manual, Renesas.
