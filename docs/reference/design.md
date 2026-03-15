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
different requirements than servers, cloud providers and kubernetes.
This simplifies everything.

## Memory Model

### The Budget

```
16MB total RAM:
 GC heap (two semispaces): 4MB total (2MB active at any time)
 Goroutine stack:          64KB per goroutine
 Large object threshold:   >64KB bypasses the GC heap
 Everything else:          KOS + program text/data + malloc-backed assets
```

These values come from the runtime configuration:
- GC heap: `GC_SEMISPACE_SIZE_KB` in `runtime/godc_config.h` (default 2048, so 2MB per semispace)
- Stack size: `GOROUTINE_STACK_SIZE` in `runtime/godc_config.h` (default 64KB)
- Large-object threshold: `GC_LARGE_OBJECT_THRESHOLD_KB` in `runtime/godc_config.h` (default 64KB)

Address bounds and load base follow the same conventions documented in
`memory-map.md`: main RAM is treated as the half-open interval
`[0x8C000000, 0x8D000000)`, and the program image is linked to start at
`0x8C010000` (KOS linker `LOAD_OFFSET`). This leaves the first 64KB below
`0x8C010000` for low-memory KOS/dcload use.

`tests/bench_architecture.go` reports the stack size and large-object threshold
at runtime. The semispace size comes directly from the compile-time config.

The 16MB limit is absolute. There is no virtual memory, no swap, no second
chance. Every byte matters.

### Allocation Strategy

libgodc uses three allocation paths:

**1. GC Heap (for Go objects)**

Small, frequently allocated objects go here. The semispace collector manages
them automatically. Implementation: `gc_heap.c`, `gc_copy.c`.

Implementation of the allocation in simple pseudocode:

```c
// Bump allocator: O(1) allocation (simplified)
void *gc_alloc(size_t size, type_descriptor *type) {
    size_t aligned = ALIGN(size, 8);
    size_t total = HEADER_SIZE + aligned;
    if (alloc_ptr + total > alloc_limit) {
        gc_collect();  // Cheney's algorithm
    }
    void *obj = alloc_ptr;
    alloc_ptr += total;
    return obj + HEADER_SIZE;
}
```

This is simplified. The real code in `gc_heap.c` also handles large objects
(`size > 64KB` bypasses the GC heap and goes straight to `malloc()` by
default), alignment edge cases, and gc_percent threshold checks. 
But the core is exactly this: bump a pointer.

The bump allocator is the fastest possible allocation strategy. Deallocation
happens during collection: live objects are copied, dead objects are forgotten.

Usage example:

```go
// Go: allocate freely, GC handles cleanup
func spawnEnemy() *Enemy {
    return &Enemy{bullets: make([]Bullet, 100)}
}
// No kill function needed  when nothing references it, it's collected
```

**2. KOS Heap (for large objects)**

Objects larger than 64KB bypass the moving GC heap. With the default config,
the threshold check is strict: a 64KB allocation still stays in the GC heap,
while 64KB + 1 byte goes to `malloc()`. This fits common game-asset usage:
textures, audio buffers, and mesh data are often large and long-lived.

```go
// This goes to KOS malloc, not GC:
texture := make([]byte, 256*256*2)  // 128KB texture
```

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

The `unsafe.Pointer(&texture[0])` syntax is intentional. A slice in Go is a
header (data pointer, length, capacity) - not the array itself. Passing
`&texture` would give a pointer to the slice header on the stack, not the
`malloc()`'d array. `&texture[0]` reaches through to the underlying data
pointer, which is what `free()` needs.

A typed wrapper like `FreeSlice(s *[]byte)` would be cleaner for callers, but
it would only work for `[]byte`. Game code also allocates large `[]uint16`
(pixel buffers), `[]float32` (vertex data), and others. Without generics
support, you would need a separate wrapper per slice type. Using
`interface{}` with `reflect` is not an option either - the reflect package is
too heavy for a 16MB console, and interface boxing itself allocates memory,
which defeats the purpose of a function meant to free it. The raw
`unsafe.Pointer` version is ugly but it works: zero-cost, type-agnostic, and
no heavy dependencies. Tracked in [#2](https://github.com/drpaneas/libgodc/issues/2)
for a future typed `FreeSlice` wrapper once generics support is available.

The `texture = nil` after freeing is optional but strongly recommended. After
`freeExternal`, the slice header still holds the old data pointer, length, and
capacity - it looks valid to Go code. If you accidentally access it, that's a
use-after-free. On a desktop OS, the MMU (Memory Management Unit) would catch
this: the hardware marks freed pages as inaccessible, and the next access
triggers a segfault that crashes the process immediately with a clear error.
The Dreamcast's SH4 has an MMU, but KallistiOS runs with it disabled for
performance. All 16MB of RAM is flat and directly accessible. A use-after-free
silently reads or writes whatever now lives at that address - another
allocation, GC metadata, the stack. The bug might show up as a wrong pixel, a
corrupted sound, or a crash hundreds of frames later in unrelated code.
Setting the slice to nil turns that silent corruption into a Go panic, which
is immediately visible. The function itself cannot nil the caller's variable
because it only receives a raw `unsafe.Pointer` with no knowledge of the
slice it came from. Enabling the SH4 MMU in debug builds to catch these at
the hardware level is explored in
[#3](https://github.com/drpaneas/libgodc/issues/3).

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

**3. Stack (for program execution)**

Every function call in libgodc uses a stack - local variables, return
addresses, function arguments all live here. The stack is not specific to
goroutines; even your `main()` function runs on one. What differs is where
each stack comes from and how big it is.

**Main goroutine (g0):** Runs on the KOS main thread stack. KOS defaults to
32KB, but libgodc overrides this to 128KB in `kos_startup.c`
(`KOS_MAIN_STACK_SIZE`). This larger size is needed for deep call chains
during GC scanning, printf formatting with large buffers, and test harnesses.
You can override it at compile time with `-DKOS_MAIN_STACK_SIZE=N`.

**Spawned goroutines (`go func()`):** Each gets a fixed 64KB stack allocated
via `goroutine_stack_init()` in `proc.c`. The size comes from
`GOROUTINE_STACK_SIZE` in `godc_config.h`.

In standard Go, goroutines start with a small stack (a few KB) that grows
automatically when needed - the runtime detects when a function call would
overflow the current stack, allocates a larger one, copies everything over,
and updates all pointers. Earlier Go versions used "segmented stacks"
(splitstack), where additional stack segments were chained together on demand
instead of copying. Both approaches let goroutines use only as much stack as
they actually need.

libgodc uses neither. Stacks are fixed-size, allocated once, never resized.
This is simpler and faster (no growth checks, no copying, no pointer updates),
but requires discipline - if a call chain goes deeper than the stack size, it
overflows silently.

The infrastructure for overflow detection exists: every goroutine has a
`stack_guard` field set to the bottom of its stack, and the TLS block stores
it at offset 0 where the SH4 split-stack prologue would read it via
`@(0, GBR)`. However, the compiler flag `-fno-split-stack` disables the
overflow-checking prologues, and `__morestack` has been removed from the
assembly. The reason: the GBR register, which the split-stack prologue reads,
conflicts with KOS's `_Thread_local` storage. Without the prologues, no check
happens, and overflow corrupts whatever sits below the stack in memory - with
no fault, no panic, and no warning (the Dreamcast has no MMU protection, see
[#3](https://github.com/drpaneas/libgodc/issues/3)). Alternative detection
strategies (stack canaries, SP checks at yield points, MMU guard pages) are
tracked in [#4](https://github.com/drpaneas/libgodc/issues/4).

Stack frames are freed automatically when functions return. Use the stack
for temporary buffers:

```go
func processAudio() {
    buffer := [4096]int16{}  // 8KB on stack, automatically freed
    // ...
}
```

Be careful with fixed-size arrays. The compiler places non-escaping fixed-size
arrays entirely on the stack. A `var buf [100000]byte` inside a goroutine
would put 100KB on a 64KB stack, silently overflowing it. This does not apply
to `make()` - `make([]byte, 100000)` always heap-allocates the backing array
through `gc_alloc`, which routes large objects (>64KB) to `malloc()`. Only the
12-byte slice header (data pointer, length, capacity) stays on the stack. Rule
of thumb: use `make` for large buffers, keep fixed-size arrays well under the
stack size. Compile-time detection of oversized stack frames is tracked in
[#5](https://github.com/drpaneas/libgodc/issues/5).

## Garbage Collection

### Object Header

Τhe GC has hammer, and everything in the memory looks like a nail. Sees no 
type system, but just raw memory. For each object it encounters, it must answer
two questions: 

1. "how many bytes do I copy to the other semispace?" and 
2. "does this object contain pointers I need to follow?"

Without answers, the GC cannot tell where one object ends and the next begins.

Solution? Every object solves this by carrying an 8-byte header
right before its data. The GC reaches it with `ptr - 8` - a single subtract,
no lookups, no hash tables. The cost is 8 extra bytes per object.

```
              8-byte header                    your data
        ┌──────────┬──────────┐          ┌─────────────────┐
        │  word 1  │  word 2  │          │                 │
        │ (4 bytes)│ (4 bytes)│          │  object payload │
        └──────────┴──────────┘          └─────────────────┘
             ▲           ▲                       ▲
             │           │                       │
          GC info    type pointer          what your Go code sees
```

The header has two 4-byte words:

**Word 1 - GC info (packed into 32 bits):**
- **Forwarded** (1 bit): Has this object already been copied to the other
  semispace during the current GC cycle? Prevents copying the same object
  twice.
- **NoScan** (1 bit): Does this object contain any pointers? If not, the GC
  can skip scanning its contents - it just copies the bytes without looking
  inside. This is the key performance flag.
- **Type tag** (6 bits): What Go type kind this is (int, string, struct,
  slice, etc.). Used for type-safe operations.
- **Size** (24 bits): The total object size in bytes, including the 8-byte
  header and any alignment padding. The GC needs this to know how many bytes
  to copy. 24 bits allows sizes up to 16MB, which covers the entire Dreamcast
  RAM.

**Word 2 - Type pointer (32 bits):**
A pointer to the full type descriptor, which contains detailed information
about the object's layout (field offsets, which fields are pointers, etc.).
The GC uses this when scanning objects that contain pointers.

For example, a `[4]byte` array in memory looks like this:

```
        header (8 bytes)         data (4 bytes)    padding (4 bytes)
┌───────────────────────────┬──────────────────┬──────────────────┐
│ NoScan=1, Size=16, type=..│  0x41 0x42 ...   │   (alignment)   │
└───────────────────────────┴──────────────────┴──────────────────┘
                                                 total: 16 bytes
```

The `[4]byte` holds 4 bytes of useful data but costs 16 bytes in total
(8 header + 4 data + 4 alignment padding). This is why many small allocations
hurt more than fewer large ones.

The NoScan bit is critical for performance. Objects containing only integers,
floats, or other non-pointer types skip GC scanning entirely: the collector
just copies them without inspecting their contents.

The practical takeaway: prefer value types over pointer types when possible.

```go
type Vertex struct { X, Y, Z float32 }

// No pointers inside, GC copies the bytes and moves on (NoScan):
mesh := make([]Vertex, 1000)

// Every element is a pointer, GC must inspect each one to find
// and copy the Vertex it points to (scan):
mesh := make([]*Vertex, 1000)
```

### Algorithm: Cheney's Semispace Collector

The GC-managed heap is divided into two equally sized regions called
semispaces. Think of them as "space A" and "space B". Small and medium
allocations happen in one of them (the active space) using the bump allocator.
The other semispace is reserved as the destination for the next collection.
Objects larger than `GC_LARGE_OBJECT_THRESHOLD` (64KB by default) bypass this
moving heap and go through `malloc()` instead.

GC does not wait until the active semispace is literally full. By default it
can trigger once usage crosses the configured threshold (75% when
`gc_percent=100`), and an allocation that would overflow the active semispace
also forces a collection.

When collection runs, this runtime does the following:

1. **Effectively stop-the-world for Go goroutines.** libgodc runs one
   goroutine at a time on a single KOS thread, and goroutines switch only when
   they explicitly yield. The goroutine that triggers GC enters `gc_collect()`
   directly, while all other goroutines are already inactive as saved
   contexts. So for Go code this has the same effect as stop-the-world GC, but
   the runtime does not need a separate mechanism to pause parallel mutators.
2. **Flip to the other semispace.** The collector chooses the inactive
   semispace as the new active space and resets the three pointers that drive
   collection: `alloc_ptr` becomes the next free byte for new allocations,
   `alloc_limit` marks the end of that semispace, and `scan_ptr` starts at the
   beginning of the copied-object queue that Cheney's algorithm will walk.
3. **Scan the roots first.** Roots are the references the collector knows how
   to find without already knowing which heap objects are live. In libgodc,
   those roots are explicit roots (`gc_add_root`), compiler-registered global
   roots (`registerGCRoots`), the current stack, all goroutine stacks, and the
   `G` structs that hold runtime metadata. Starting with roots is essential:
   they define where reachability begins. Only after scanning them can the
   collector follow pointers into GC-managed memory, copy the first reachable
   objects into to-space, and seed Cheney's work queue. Global roots use
   compiler-provided pointer metadata when available; stacks and `G` structs
   are scanned conservatively. There is no separate explicit register scan.
4. **Copy reachable objects.** When a root points into from-space, the object
   it references is copied to to-space and the old header is rewritten with a
   forwarding pointer. If another root or object reaches it again, the
   forwarding pointer is reused instead of copying a second time.
5. **Perform Cheney's scan.** Objects already copied into to-space are scanned
   in allocation order. Their pointer fields are updated, and any referenced
   objects still in from-space are copied. The new semispace acts as the work
   queue: `scan_ptr` advances through copied objects until it catches
   `alloc_ptr`.
6. **Finish the cycle.** When `scan_ptr == alloc_ptr`, all reachable semispace
   objects have been moved. `bytes_allocated` becomes the live size, the old
   space is no longer used for allocation, and cache invalidation of that old
   space is deferred and processed incrementally after the GC pause.

```
Before GC:                          After GC:
  Space A (active, live + dead)       Space A (old from-space)
  ┌────────────────────┐              ┌────────────────────┐
  │ obj1 obj2 ... objN │              │ no longer used for │
  │ (reachable and     │              │ allocation; cache  │
  │  unreachable mixed)│              │ invalidated later  │
  └────────────────────┘              └────────────────────┘

  Space B (inactive)                  Space B (new to-space / active)
  ┌────────────────────┐              ┌────────────────────┐
  │                    │              │ obj1 obj3 obj7 ... │
  │                    │              │ (only reachable    │
  │                    │              │  objects)    ░░░░░ │
  └────────────────────┘              └────────────────────┘
                                             ▲ alloc_ptr after GC
```

Before GC, Space A is the active semispace holding all allocated objects -
reachable and unreachable intermixed. Space B sits empty. During GC, the
collector copies only reachable objects from A (now called "from-space") into B
(now called "to-space"). Unreachable objects like obj2 are never copied - they
are reclaimed implicitly by not being moved. After GC, Space B becomes the
active semispace with only live objects packed together, and Space A is
abandoned. The next GC cycle flips them again: B becomes from-space, A becomes
to-space.

```c
// Two semispaces, allocated at startup
gc_heap.space[0] = memalign(32, GC_SEMISPACE_SIZE);
gc_heap.space[1] = memalign(32, GC_SEMISPACE_SIZE);

// Collection flips to the other semispace
int old_space = gc_heap.active_space;
int new_space = 1 - old_space;
gc_heap.active_space = new_space;
gc_heap.alloc_ptr = gc_heap.space[new_space];
gc_heap.alloc_limit = gc_heap.space[new_space] + gc_heap.space_size;
gc_heap.scan_ptr = gc_heap.space[new_space];

// Scan roots, copy reachable objects, then scan copied objects
gc_scan_roots();
while (gc_heap.scan_ptr < gc_heap.alloc_ptr) {
    gc_header_t *header = (gc_header_t *)gc_heap.scan_ptr;
    void *obj = gc_get_user_ptr(header);
    gc_scan_object(obj);
    gc_heap.scan_ptr += GC_HEADER_GET_SIZE(header);
}
```

Why this algorithm? For the semispace heap it is simple to implement,
allocation is just a bump pointer, surviving objects are compacted
automatically, and reference cycles are handled naturally through forwarding
pointers. The tradeoff is that only one semispace is available for GC-managed
allocation at a time, so the other half must stay reserved as the copy
destination for the next collection.

### Collection Trigger

GC runs when:
 Active space exceeds threshold (default: 75% when `gc_percent=100`)
 Allocation would exceed remaining space
 Explicit GC call

The threshold is controlled by `gc_percent`:
- `gc_percent = 100` (default): threshold = 75% of heap space
- `gc_percent = 50`: threshold = 50% of heap space  
- `gc_percent = -1`: disable threshold-triggered GC; allocations that would overflow the active semispace still force collection

To control GC from Go:

```go
import _ "unsafe"

//go:linkname setGCPercent debug.SetGCPercent
func setGCPercent(percent int32) int32

//go:linkname gc runtime.GC
func gc()

func init() {
    setGCPercent(50)   // Trigger at 50% instead of 75%
    setGCPercent(-1)   // Disable threshold-triggered GC
    gc()               // Force collection now
}
```

Build and run `tests/test_gc_percent.elf` to verify this works.

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
    setGCPercent(-1)  // Disable threshold-triggered GC

    // ... gameplay avoids threshold-triggered GC pauses ...
    
    // GC during loading screens only:
    showLoadingScreen()
    forceGC()
    startGameplay()
}
```

Even with `gc_percent = -1`, an allocation that would overflow the active
semispace still forces a collection.

### Root Scanning

The GC finds live objects by tracing from roots:

```c
static void gc_scan_roots(void)
{
    // Scan explicit roots (gc_add_root)
    for (int i = 0; i < gc_root_table.count; i++) { ... }

    // Scan compiler-registered roots (registerGCRoots)
    gc_scan_compiler_roots();

    // Scan current stack
    gc_scan_stack();

    // Scan all goroutine stacks
    gc_scan_all_goroutine_stacks();
}
```

1. **Global variables**  Registered by gccgo-generated code via
   `registerGCRoots()`. Each package contributes a root list.

2. **Goroutine stacks**  Scanned conservatively. Every aligned pointer-sized
   value that points into the heap is treated as a potential pointer.

3. **Goroutine metadata (`G` structs)**  Scanned conservatively for pointers
   such as `_panic`, `_defer`, `waiting`, and `checkpoint`.

4. **Explicit roots**  Optional. If you write C code that holds pointers to
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

- Channel operations (send, receive, select)
- `runtime.Gosched()`
- `time.Sleep()` and timer waits

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
only uses integers. Compared with the no-FPU path, this adds about 25 extra
cycles inside each low-level `__go_swapcontext` call.

```go
// Both goroutines pay FPU overhead, even though neither uses floats
go audioDecoder()   // Integer PCM math
go networkHandler() // Packet parsing
```

This is a tradeoff: always saving FPU is slower but correct. A goroutine
that unexpectedly uses a float won't corrupt another's FPU state.

`runtime_sh4_minimal.S` also contains `__go_swapcontext_lazy` and
`__go_swapcontext_nofpu`, but the scheduler currently calls only the full
`__go_swapcontext` path. The current runtime does not track per-goroutine FPU
usage in `G` or pass `fpu_flags` from the scheduler, so the conservative
always-save path is the one that is actually used.

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
4. **Waiting**  Parked on channel, `select`, or timer waits
5. **Dead**  Function returned, queued for cleanup

`proc.c` includes a grace-period dead queue using `death_generation` and
`dead_link`, but in the current source `global_generation` is never advanced.
So the reclamation path exists, but exited goroutines do not currently age into
reclaimable state.

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
case x := <-ch1:  // These are checked in random order
case ch2 <- y:
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
```

### Panic and Recover

User-initiated panic (`panic()`) is recoverable via `recover()` in a deferred
function. Implementation uses `setjmp`/`longjmp` with checkpoints.

Current implementation detail: helper functions for nil dereference, bounds
checks, and divide-by-zero also call `runtime_panicstring()`, so they enter the
same panic/recover machinery instead of going straight to `runtime_throw()`.
Fatal runtime failures still use `runtime_throw()` and abort immediately.

## Type System

### Type Descriptors

gccgo generates type descriptors for every Go type. libgodc uses these for:

 GC pointer scanning (which fields contain pointers?)
 Interface method dispatch (which methods does this type implement?)
 Reflection (what is this type's name and structure?)

```c
typedef struct __go_type_descriptor {
    uintptr_t __size;                               // Size in bytes
    uintptr_t __ptrdata;                            // Prefix containing pointers
    uint32_t __hash;                                // Type hash
    uint8_t __tflag;                                // Extra type flags
    uint8_t __align;                                // Variable alignment
    uint8_t __field_align;                          // Struct-field alignment
    uint8_t __code;                                 // Kind (bool, int, slice, etc.)
    void *__equalfn;                                // Equality helper
    const uint8_t *__gcdata;                        // GC bitmap/program
    const struct __go_string *__reflection;         // Reflection string form
    const struct __go_uncommon_type *__uncommon;    // Method metadata
    struct __go_type_descriptor *__pointer_to_this; // Descriptor for *T
} __go_type_descriptor;
```

On the current 32-bit SH-4 build this base descriptor is 36 bytes. See
`runtime/type_descriptors.h` for the authoritative layout and offset checks.

### Interface Tables

Interface dispatch uses runtime-built method tables with the layout gccgo
expects. When you write:

```go
var w io.Writer = os.Stdout
w.Write(data)
```

libgodc builds the itab on demand in `get_itab()`, fills the method slots at
runtime, and caches the result for reuse.

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

libgodc is built with GCC's `-m4-single` mode. The SH4 FPU is fast in
single-precision but slow in double-precision. All `float64` operations
generate software-emulation calls; avoid them in hot paths.

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

Selected files in `runtime/` (not exhaustive):

```
runtime/
├── go-main.c              # Executable entry point
├── kos_startup.c          # KOS startup integration
├── dreamcast_support.c    # Platform support glue
├── runtime.h              # Shared runtime declarations
├── runtime_stubs.c        # GCC/gccgo runtime entry points and helpers
├── runtime_c_stubs.c      # C wrappers for runtime symbols
├── gc_heap.c              # Heap initialization, allocation
├── gc_copy.c              # Cheney's copying collector
├── gc_runtime.c           # Go runtime GC control hooks
├── gc_semispace.h         # GC data structures, header layout, constants
├── writebarrier_dreamcast.c # Write barrier support
├── scheduler.c            # Run queue, schedule(), goready()
├── proc.c                 # Goroutine creation, lifecycle
├── stack.c                # Fixed-size stack allocation and pooling
├── splitstack.c           # gccgo split-stack compatibility stubs
├── chan.c                 # Channel implementation
├── chan.h                 # Channel data structures
├── select.c               # Select statement
├── sudog.c                # Wait queue entries
├── defer_dreamcast.c      # Defer/panic/recover
├── panic_dreamcast.h      # Panic/checkpoint data structures
├── go-panic.c             # Compiler/runtime panic helpers
├── tls_sh4.c              # Goroutine TLS and current_g tracking
├── timer.c                # Time.Sleep, timers
├── interface_dreamcast.c  # Interface conversions and runtime itab creation
├── type_registry.c        # Runtime type registration helpers
├── type_descriptors.h     # Type descriptor layouts
├── map_dreamcast.c        # Map implementation
├── map_dreamcast.h        # Map type definitions
├── map_fast_internal.h    # Map internals
├── string_dreamcast.c     # String runtime helpers
├── go-unsafe-pointer.c    # unsafe helpers
├── go-print.c             # print/println support
├── go-memmove.c           # memmove builtin
├── go-memequal.c          # equality builtin
├── go-memclr.c            # memclr builtin
├── go-construct-map.c     # Map construction helpers
├── go-callers.c           # Caller stack helpers
├── go-caller.c            # Caller lookup
├── go-assert.c            # Assert helpers
├── goroutine.h            # Core data structures
├── godc_config.h          # Runtime configuration
├── dc_platform.h          # Platform constants
├── runtime_sh4_minimal.S  # Context switching assembly
├── gen-offsets.c          # Offset-check source for assembly layout
└── asm-offsets.h          # Currently generated placeholder header
```

## AssemblyC ABI Synchronization

### The Problem

Context switching is implemented in assembly (`runtime_sh4_minimal.S`). The assembly
code accesses G struct fields by hardcoded byte offsets:

```asm
mov.l   @(32, r4), r0    ! Load G->context at offset 32
```

If someone changes the G struct in C (adds/removes/reorders fields), the assembly
breaks silently: it reads garbage from the wrong offsets. This is a classic embedded
systems bug: C struct layout changes invisibly break handwritten assembly.

### Current State

The current protection story is more limited than a fully generated include
workflow:

1. `runtime/runtime_sh4_minimal.S` still hardcodes the offsets it uses via
   local `.equ` constants such as `G_CONTEXT`.
2. `runtime/gen-offsets.c`, `runtime/asm-offsets.h`, and `make check-offsets`
   still exist, but the current header-generation pipeline greps `#define`
   lines that `gen-offsets.c` does not emit.
3. As checked in today, `runtime/asm-offsets.h` is therefore just a placeholder
   header with include guards and no actual offset definitions.
4. `make check-offsets` currently validates only that placeholder output, not
   the `.equ` constants in the assembly file.
5. `runtime/scheduler.c` does not currently perform a runtime `offsetof()`
   assertion before scheduling starts.

In other words, the assembly file is still the effective source of truth for
the offsets it consumes.

### Workflow for Changing G Struct

1. Modify `runtime/goroutine.h` (the authoritative definition)
2. Update `runtime/gen-offsets.c` to mirror the layout being checked
3. Update the `.equ` values in `runtime/runtime_sh4_minimal.S` if any offsets changed
4. Run `make runtime/asm-offsets.h` to refresh the placeholder generated header
5. Run `make check-offsets` to confirm that placeholder output still matches
6. Commit the layout-related files together

### Why This Matters

In games, struct layout bugs cause symptoms like:

- Goroutines resume with corrupted registers
- Context switches overwrite random memory
- FPU state leaks between goroutines
- Panics with nonsensical stack traces

These are nearly impossible to debug. Even with the current partial
verification, keeping the C layout, placeholder generated header, and assembly
constants in sync is critical.

## Performance

The source tree includes `tests/bench_architecture.go`, which reports these
metrics when run on hardware:

| Benchmark | Reported by the current source | Notes |
|-----------|--------------------------------|-------|
| `gosched` | ns per yield | `runtime.Gosched()` in a tight loop |
| Baseline comparison | ns per inline-loop iteration | Rough baseline only; not a direct function call |
| Buffered channel | ns per operation | Sends and receives on a buffered channel |
| Context switch | ns per switch | Derived from ping-pong goroutines |
| Unbuffered channel | ns per roundtrip | Send + receive over an unbuffered channel |
| Goroutine spawn | ns per spawn | Create, schedule, run, and receive |
| GC pause | us per forced collection | Retained sizes from 32KB to 1MB |
| Memory layout | stack size, context size, header size, and large-object threshold | Reports runtime configuration |

Run `tests/bench_architecture.elf` on your hardware for current numbers.

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
