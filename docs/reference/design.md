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
│     bare-metal OS for Dreamcast                                │
│     provides malloc, threads, drivers                          │
├────────────────────────────────────────────────────────────────┤
│  Dreamcast Hardware                                            │
│     SH4 CPU, PowerVR2 GPU, AICA sound                          │
│     16MB main RAM, 8MB VRAM                                    │
└────────────────────────────────────────────────────────────────┘
```

We don't need the full Go runtime. We need enough to run games. Games have
different requirements than servers, cloud services, or desktop systems.
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
(`split-stack`), where additional stack segments were chained together on demand
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

The GC sees raw memory, not a high-level Go object graph. For each GC-managed
semispace object it encounters, it must answer two questions:

1. "how many bytes do I copy to the other semispace?" and 
2. "does this object contain pointers I need to follow?"

Without answers, the GC cannot tell where one object ends and the next begins.

Solution? Each GC-managed semispace object carries an 8-byte header right
before its data. The GC reaches it with `ptr - 8` - a single subtract, no
lookups, no hash tables. The cost is 8 extra bytes per semispace object.
External `malloc()`-backed allocations do not use this header.

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

For GC-managed semispace objects, the header has two 4-byte words:

**Word 1 - GC info (packed into 32 bits):**
- **Forwarded** (1 bit): Has this object already been copied to the other
  semispace during the current GC cycle? Prevents copying the same object
  twice.
- **NoScan** (1 bit): Does this object contain any pointers? If not, the GC
  can skip scanning its contents - it just copies the bytes without looking
  inside. This is the key performance flag.
- **Type tag** (6 bits): Compact Go kind metadata copied into the header. The
  collector primarily relies on the full type descriptor pointer, not this
  small tag, when it needs object layout information.
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
Objects larger than `GC_LARGE_OBJECT_THRESHOLD` (64KB by default) currently
bypass this moving heap and go through `malloc()` instead. That is convenient
for large raw buffers, but it is also a current runtime limitation for large
typed Go allocations that contain pointers; see
[#6](https://github.com/drpaneas/libgodc/issues/6).

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

GC pause time is the time spent inside a collection cycle while Go code is
waiting for the collector to finish. In libgodc, this is the elapsed time
inside `gc_collect()`: roots are scanned, reachable objects are copied, and
Cheney's scan completes before execution continues. More live data usually
means a longer pause because there is more memory to copy and scan.

Pause time matters because it directly consumes your frame budget. At 60fps,
one frame is only 16.6ms. A 2ms GC pause is noticeable but often acceptable; a
10ms pause consumes most of the frame; a pause longer than 16.6ms can cause a
missed frame or visible hitch.

Run `tests/bench_gc_pause.elf` on hardware for focused pause measurements, or
`tests/bench_architecture.elf` for a broader benchmark that also reports GC
pauses.

For 60fps gameplay, disable threshold-triggered GC during hot gameplay
sections:

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
semispace still forces a collection. So this reduces surprise GC pauses, but
it is not a hard guarantee of "no GC during gameplay" if you keep allocating or
your live data no longer fits in one semispace.

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

    // Scan all goroutine stacks (and their G structs)
    gc_scan_all_goroutine_stacks();
}
```

1. **Explicit roots**  Optional. If C code holds pointers to Go objects across
   a collection, it must register the pointer location with `gc_add_root(&ptr)`.
   During GC, the collector updates that location if the object moves.

2. **Compiler-registered roots**  These are not "all globals" scanned blindly.
   gccgo registers root lists with `registerGCRoots()`, and the collector scans
   those roots using compiler-provided pointer metadata when available.

3. **Current stack**  The stack of the goroutine that triggered GC is scanned
   conservatively from the current stack pointer upward.

4. **Other goroutine stacks**  Each live goroutine's saved stack is scanned
   conservatively using the saved stack pointer, so only the used portion of
   the stack is examined.

5. **Goroutine metadata (`G` structs)**  Each live goroutine's `G` struct is
   also scanned conservatively for runtime-held pointers such as `_panic`,
   `_defer`, `waiting`, and `checkpoint`.

`Conservative` here does not mean "every aligned word is automatically treated
as a pointer." The scanner first filters for values that look like valid heap
object pointers and only then treats them as references. There is no separate
explicit register scan; stack scanning catches spilled registers.

### DMA Hazard

DMA (Direct Memory Access) is a hardware feature. The CPU sets up the transfer
by telling a hardware block such as the graphics processor (PowerVR2) or sound
processor (AICA): use this source address, this destination address, and this
size, then start. After that, the hardware moves the bytes on its own while
the CPU goes on doing other work instead of copying the data byte by byte.

The hazard is timing. Once DMA starts, the hardware keeps using the raw memory
address the CPU programmed into it. But libgodc's GC moves small heap objects.
If the buffer lives in the GC heap and a collection happens before DMA
completes, the GC may move that buffer to a new address. Go code can be
updated to the new address, but the hardware is not aware of the move and keeps
reading from or writing to the old, now stale, address.

There are only a few ways to avoid this:

1. Use memory that the moving GC will never relocate. In libgodc, large
   allocations (`size > 64KB`) bypass the semispace GC and use `malloc()`
   instead. Textures and framebuffers can also live in VRAM via
   `kos.PvrMemMalloc()`.
2. Make sure no collection can happen while the DMA transfer is in flight. In
   practice that means: do not call `runtime.GC()`, avoid allocations that
   could fill the active semispace, and if needed disable threshold-triggered
   GC around that section with `debug.SetGCPercent(-1)`. This reduces the risk,
   but it is not a perfect guarantee: if you overflow the active semispace, GC
   still runs.
3. Handle cache coherency separately. A stable address is necessary, but not
   sufficient. Even if the buffer does not move, DMA code still needs cache
   flush/invalidate calls so hardware and CPU see the same bytes.

Longer-term API work to make these memory domains explicit and steer callers
toward DMA-safe buffers is tracked in
[#11](https://github.com/drpaneas/libgodc/issues/11).

Movement-safe patterns:

```go
// DANGEROUS: small buffer in moving GC heap
data := make([]byte, 4096)      // Small, in GC heap
startDMA(data)                  // Hardware keeps this address until DMA completes
runtime.Gosched()               // Another goroutine may run
// Any later allocation or explicit runtime.GC() before DMA completes can move `data`

// SAFE: large allocations bypass the moving GC heap
data := make([]byte, 100*1024)  // >64KB, uses malloc
startDMA(data)                  // Address will not move during GC

// SAFE: VRAM for textures
tex := kos.PvrMemMalloc(size)   // Allocates in VRAM
```

These patterns only avoid movement by the GC. DMA code still needs explicit
cache flush/invalidate calls; see Cache Coherency below.

## Scheduler

The scheduler is the part of the runtime that decides which goroutine runs
next. In libgodc, it keeps a queue of goroutines that are ready to run. When
the current goroutine cannot continue yet, such as waiting on a channel or
timer, or when it voluntarily lets another goroutine run, the scheduler saves
its state and restores another goroutine's state so execution can continue
there.

### M:1 Cooperative Model

All goroutines run on top of a single underlying KallistiOS (KOS) OS thread.
Only one goroutine executes at a time. The scheduler gets a chance to run only
when the current goroutine
voluntarily gives up control or blocks waiting for something:

- Channel operations that need to wait, such as send, receive, or `select`
- `runtime.Gosched()`, which means "let another goroutine run now"
- `time.Sleep()` or timer waits, which park the goroutine until a timer expires

A goroutine in a tight CPU loop will monopolize the processor. The runtime
cannot forcibly interrupt it and switch to another goroutine.

### Why M:1?

`M:1` means many goroutines (`M`) are multiplexed onto one underlying OS thread
(`1`). On Dreamcast, that OS thread runs on the console's single CPU core, so
only one goroutine can execute at a time.

`Preemptive scheduling` means the runtime or OS can interrupt a running
goroutine at arbitrary points, usually on a timer, and switch to another one.
That improves fairness, but it also needs more machinery: timer-driven
interrupts, more bookkeeping, and more forced context switches.

`Cooperative scheduling` means the current goroutine keeps running until it
blocks or explicitly yields. That is simpler because switches happen only at
known points, and it is usually faster on this target because the runtime
avoids timer-driven forced switches and their bookkeeping cost.

Since the Dreamcast has one CPU core, this extra preemptive machinery would not
let Go goroutines run in parallel. The tradeoff is fairness: a goroutine that
never yields can starve others. For libgodc's game-oriented workloads, the
simpler M:1 cooperative design is a good fit.

Possible future work in this area is tracked in
[#12](https://github.com/drpaneas/libgodc/issues/12), which explores enforced
safepoints and better scheduler fairness without abandoning the single-threaded
design. The related question of making the single KallistiOS-thread contract
explicit is tracked in [#9](https://github.com/drpaneas/libgodc/issues/9).

### Run Queue Structure

The run queue is the scheduler's waiting room for goroutines that are ready to
run right now. Some goroutines are blocked waiting on a channel or timer; those
cannot run yet. Others are ready to run as soon as the CPU becomes available.
The scheduler needs a place to remember that second group, and the run queue is
that place.

Nothing maintains this queue in parallel or in the background. The scheduler
updates it directly in the same single-threaded runtime code:

- when a goroutine becomes runnable again, it is appended to the queue
- when the current goroutine yields, it is appended to the queue
- when the scheduler wants the next goroutine, it removes one from the front

libgodc uses a simple FIFO queue: goroutines are added to the tail and removed
from the head. This is easy to implement, predictable, and sufficient for
game-oriented workloads where the program mostly controls fairness by deciding
when goroutines yield.

```c
// Goroutines execute in the order they become runnable
runq_put(gp);   // Add to tail
gp = runq_get(); // Remove from head
```

For real-time requirements, structure your code so time-sensitive work runs on
the main goroutine or yields frequently.

One subtlety: it is first ready, first run, not first created, first run forever.
A goroutine can run, block, wake up later, and then re-enter the queue at a later time.

### Context Switching

Context switching is the low-level mechanism the scheduler uses to pause one
goroutine and resume another. A `context` is the CPU state needed to continue
execution later as if nothing happened: register values, stack pointer, return
address/program counter, and floating-point state.

This solves a basic problem: the Dreamcast has one CPU core, but libgodc wants
many goroutines to take turns running on it. When goroutine A yields, the
runtime must remember exactly where A stopped so it can later resume A and run
goroutine B in the meantime. Without context switching, the runtime would lose
the CPU state for the paused goroutine.

In libgodc, each goroutine saves 64 bytes of CPU state when it yields:

```c
typedef struct sh4_context {
    uint32_t r8, r9, r10, r11, r12, r13, r14;  // Callee-saved
    uint32_t sp, pr, pc;                        // Special registers
    uint32_t fr12, fr13, fr14, fr15;           // FPU callee-saved
    uint32_t fpscr, fpul;                       // FPU control
} sh4_context_t;
```

The actual save/restore operation is implemented in
`runtime_sh4_minimal.S` (simplified for brevity).

This code is written in assembly rather than C or Go because it must directly
manipulate CPU registers and stack state with exact control. A normal C or Go
function would already be using registers, following the calling convention,
and touching the stack while it runs, which could overwrite the very machine
state the runtime is trying to preserve. The SH-4 return register (`pr`) and
stack pointer (`sp`) also need precise save/restore handling that is awkward or
impossible to express safely in ordinary Go or C.

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

This snippet shows the core idea:

- `r4` points to the context structure where the current goroutine's CPU state
  should be saved
- `r5` points to the context structure for the goroutine we want to resume
- the first group of instructions copies the current register values into
  `old_ctx`
- the second group loads previously saved register values from `new_ctx`
- `rts` returns into the restored execution state, so the resumed goroutine
  continues as if it had never stopped

The omitted lines do the same for the stack pointer, return/program location,
and floating-point state.

### FPU Context

Every context switch saves floating-point registers, even if your goroutine
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

Possible future work to use the lazy/no-FPU paths safely is tracked in
[#13](https://github.com/drpaneas/libgodc/issues/13).

## Goroutine Structure

A goroutine is not just a function call. The runtime needs a record that keeps
track of everything required to pause it, resume it, and manage its state over
time. In libgodc, that record is the `G` struct.

The important takeaway is not every field, but the categories of information
the runtime keeps for each goroutine:

- panic/defer state, with ABI-critical fields expected by gccgo
- scheduling state, such as whether the goroutine is runnable or waiting
- stack bounds and TLS
- saved CPU context, so the goroutine can be resumed later
- wait-state metadata for channels, timers, and cleanup

Here is a reduced view of the structure:

```c
typedef struct G {
    // ABI-critical: gccgo expects these at fixed offsets
    PanicRecord *_panic;      // Offset 0: innermost panic
    GccgoDefer *_defer;       // Offset 4: innermost defer

    // Scheduling
    Gstatus atomicstatus;

    // Stack
    void *stack_lo;
    void *stack_hi;

    // Saved CPU state for context switching
    sh4_context_t context;

    // Metadata
    int64_t goid;
    WaitReason waitreason;

    // Waiting / panic bookkeeping
    sudog *waiting;
    Checkpoint *checkpoint;
} G;
```

`goroutine.h` contains the authoritative full definition. The reason this
structure matters is simple: without it, the runtime would have nowhere to
store the goroutine's saved CPU state, stack information, wait status, and
panic/defer bookkeeping between scheduler decisions.

### Goroutine Lifecycle

A goroutine moves through a small runtime state machine:

1. **Created**  `__go_go()` allocates the `G` struct, a stack, and a TLS block.
2. **Ready to run**  The goroutine is placed on the run queue, meaning it could
   run as soon as the scheduler picks it.
3. **Running**  The scheduler context-switches into that goroutine, so it now
   owns the CPU.
4. **Waiting**  If it cannot continue yet, such as waiting on a channel,
   `select`, or timer, the runtime parks it and runs something else.
5. **Finished**  When the goroutine function returns, the runtime marks it dead
   and queues it for cleanup.

`proc.c` includes a grace-period dead queue using `death_generation` and
`dead_link`, but in the current source `global_generation` is never advanced.
So the cleanup path exists, but exited goroutines do not currently become
eligible for actual cleanup and reuse.

## Channels

Channels are the primary synchronization primitive. Implementation follows
the Go runtime closely.

They matter to libgodc's design because channel operations are one of the main
ways goroutines block, wake up, and hand work to each other. So channels are
not just a language feature here; they are tightly connected to the scheduler,
wait queues, and overall runtime behavior.

### Structure

A channel is represented at runtime by an `hchan` structure. The runtime needs
this structure to solve three problems:

- store buffered elements for buffered channels
- remember which goroutines are waiting to send or receive
- track channel state, such as element size and whether the channel is closed

Here is a reduced view of the structure:

```c
typedef struct hchan {
    uint32_t qcount;       // How many elements are buffered right now
    uint32_t dataqsiz;     // Buffer capacity (0 means unbuffered)
    void *buf;             // Circular buffer storage
    uint16_t elemsize;     // Size of each element
    uint8_t closed;        // Whether close(ch) has happened
    uint32_t sendx, recvx; // Circular-buffer indices
    waitq recvq, sendq;    // Goroutines waiting to receive or send
    uint8_t locked;        // Internal lock for channel operations
} hchan;
```

The important fields fall into three groups:

- `qcount`, `dataqsiz`, `buf`, `sendx`, and `recvx` describe the buffered data
- `recvq` and `sendq` remember blocked receivers and senders
- `elemsize` and `closed` describe the channel's element layout and state

`chan.h` contains the authoritative full definition, including extra metadata
such as `elemtype` and internal optimizations.

### Unbuffered Channels

An unbuffered channel has no storage for queued elements. A send cannot finish
until a receiver is ready, and a receive cannot finish until a sender is ready.
When both are ready, the value transfers directly from sender to receiver with
no intermediate buffer.

This makes an unbuffered channel more than a transport mechanism: it is also a
synchronization point. The sender and receiver rendezvous at the channel
operation, so neither side continues past that point until the other side is
ready.

### Buffered Channels

A buffered channel adds storage between sender and receiver. The sender can
place values into the channel without waiting, as long as there is still room
in the buffer. The receiver can remove values without waiting, as long as the
buffer is not empty.

So a buffered channel trades strict rendezvous for decoupling: sender and
receiver do not have to meet at exactly the same moment. The runtime implements
the buffer as a simple circular array.

### Select

`select` lets one goroutine wait on several channel operations at once and
continue with whichever one becomes possible first. Instead of committing to a
single send or receive, the goroutine asks the runtime to choose among multiple
cases.

When more than one case is ready, the runtime randomizes the polling order so
the same case does not always win first:

```go
select {
case x := <-ch1:  // These are checked in random order
case ch2 <- y:
case <time.After(timeout):
}
```

Implementation summary: shuffle the case order, check for any ready case, and
if none are ready, park the goroutine on all relevant wait queues until one
case wakes it up.


## Defer, Panic, Recover

These features matter to libgodc's design because they require per-goroutine
runtime state and controlled stack unwinding. Deferred calls live on each
goroutine, panic state lives on each goroutine, and recovery depends on
checkpoint machinery that can jump execution back to a known safe point.

### Defer

Each goroutine keeps a linked list of deferred calls. Every `defer` statement
pushes a record onto that list. When the function returns normally, or when the
runtime unwinds the stack during panic, those records are popped and executed
in LIFO order.

The runtime needs each defer record to remember at least:

- which deferred call comes next in the chain
- which function to invoke
- what argument to pass
- whether the defer is currently being executed as part of panic unwinding

Here is a reduced view of the structure:

```c
typedef struct GccgoDefer {
    struct GccgoDefer *link; // Next deferred call in the goroutine's chain
    PanicRecord *_panic;     // Panic currently being unwound, if any
    uintptr_t pfn;           // Function pointer to call
    void *arg;               // Argument to pass
    uintptr_t retaddr;       // Return address used for recover matching
} GccgoDefer;
```

`panic_dreamcast.h` contains the authoritative full definition, including the
ABI-sensitive fields gccgo expects.

### Panic and Recover

A panic is the runtime's stack-unwinding path. When `panic()` starts, libgodc
records panic state on the current goroutine and walks that goroutine's defer
chain. Deferred calls run one by one, and one of them may call `recover()`.

To resume execution after a successful `recover()`, the runtime jumps back to a
previous checkpoint using `setjmp`/`longjmp`. This is why panic recovery is
more constrained here than the simple Go-level story suggests: a recovered
panic without a checkpoint becomes fatal.

Current implementation detail: helper functions for nil dereference, bounds
checks, and divide-by-zero also call `runtime_panicstring()`, so they enter the
same panic/recover machinery instead of going straight to `runtime_throw()`.

Fatal runtime failures still use `runtime_throw()` and abort immediately.

## Type System

### Type Descriptors

Type descriptors are the bridge between Go's compile-time type system and the
runtime's behavior. gccgo generates a descriptor for every Go type, and libgodc
uses that metadata for:

- precise GC pointer scanning (which parts of an object may contain pointers?)
- interface method dispatch (which methods does this type implement?)
- limited reflection and type-name metadata

Here is a reduced view of the base descriptor:

```c
typedef struct __go_type_descriptor {
    uintptr_t __size;                            // Size of values of this type
    uintptr_t __ptrdata;                         // Prefix of the value that may contain pointers
    uint8_t __code;                              // Kind (bool, int, slice, struct, etc.)
    const uint8_t *__gcdata;                     // GC bitmap or GC program
    const struct __go_string *__reflection;      // String form used by limited reflection/reporting
    const struct __go_uncommon_type *__uncommon; // Method metadata for named/method-bearing types
} __go_type_descriptor;
```

The full structure also contains equality, alignment, hashing, and pointer-type
metadata. On the current 32-bit SH-4 build this base descriptor is 36 bytes.
See `runtime/type_descriptors.h` for the authoritative layout and offset
checks.

### Interface Tables

Interface method calls need another runtime structure: an interface table
(`itab`). When you write:

```go
var w io.Writer = os.Stdout
w.Write(data)
```

the runtime must answer two questions before `w.Write(data)` can run:

- what concrete type is actually stored inside the interface?
- which concrete function implements the interface method slot being called?

libgodc answers that by building an `itab` on demand in `get_itab()`. The
layout follows what gccgo expects:

- `Iface.itab` points at the `methods[0]` entry of the table
- `methods[0]` stores the concrete type descriptor
- `methods[1+]` store function pointers for the interface methods

Once built, the table is cached and reused for later interface calls with the
same interface type and concrete type pair.

## SH4 Specifics

These hardware and ABI constraints shape how libgodc is implemented.

### Calling Convention Basics

When one function calls another, the SH-4 ABI defines which registers may be
overwritten and which must be preserved. This matters directly to libgodc's
assembly stubs and context-switching code, because the runtime must save and
restore the right machine state.

- `r0-r3`: argument, result, and scratch registers
- `r4-r7`: additional argument registers
- `r8-r13`: callee-saved general-purpose registers
- `r14`: frame pointer
- `r15`: stack pointer
- `pr`: procedure return register (return address)
- `GBR`: global base register, reserved by KOS for `_Thread_local`

The terms `caller-saved` and `callee-saved` simply describe who is responsible
for preserving a register across a function call. Caller-saved registers may be
overwritten by the callee, so the caller must save them first if it still needs
their values. Callee-saved registers must be restored by the callee before it
returns.

libgodc does not use `GBR` for goroutine TLS. Instead, it uses global
`current_g` / `current_tls` pointers. This avoids conflicts with KOS and keeps
context switching simpler.

### FPU Mode

The SH-4 has a hardware floating-point unit (FPU), but libgodc is built with
GCC's `-m4-single` mode. That makes single-precision (`float32`) arithmetic the
fast path. Double-precision (`float64`) operations are much slower because they
fall back to software-emulation helpers instead of running efficiently in
hardware.

This matters both for application code and for the runtime. In hot numeric
paths, prefer `float32`. In the scheduler, FPU state also affects context-switch
cost, because floating-point registers must be preserved when goroutines switch.

### Cache Coherency

The SH-4 has 32-byte cache lines. A saved goroutine context is 64 bytes, so a
context switch touches two cache lines of CPU state.

More importantly, DMA-capable hardware does not automatically see dirty CPU
cache lines. The GC handles cache management for its own semispace flip, but
user code doing DMA must use KOS cache functions explicitly:

```c
#include <arch/cache.h>

dcache_flush_range((uintptr_t)ptr, size);  // Flush before DMA write
dcache_inval_range((uintptr_t)ptr, size);  // Invalidate after DMA read
```

## AssemblyC ABI Synchronization

### The Problem

Context switching is implemented partly in assembly (`runtime_sh4_minimal.S`).
That assembly code must know the exact byte offsets of fields inside C
structures such as `G` and `sh4_context_t`.

Unlike C, assembly has no type system and no `offsetof()` operator. It only
sees numbers. A typical pattern looks like this:

```asm
.equ G_CONTEXT, ...
...
add     r1, r0           ! r0 = &G->context
```

If someone changes the `G` struct in C by adding, removing, or reordering
fields, the assembly can silently start reading or writing the wrong memory.
This is a classic low-level systems bug: the C layout changed, but the assembly
constants did not.

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
the offsets it consumes, even though that is exactly the thing we would like to
avoid.

This gap is tracked in
[#14](https://github.com/drpaneas/libgodc/issues/14).

### Practical Rule

Until that synchronization path is repaired, treat any layout change to
`runtime/goroutine.h` or related context structures as an assembly change too:
update the mirrored definitions, update the `.equ` constants in
`runtime/runtime_sh4_minimal.S`, and verify the offset-check workflow.

### Why This Matters

In games, struct layout bugs cause symptoms like:

- Goroutines resume with corrupted registers
- Context switches overwrite random memory
- FPU state leaks between goroutines
- Panics with nonsensical stack traces

These are nearly impossible to debug. Even with the current partial
verification, keeping the C layout, placeholder generated header, and assembly
constants in sync is critical.

## Design Decisions

**Why gccgo instead of gc?**

The standard Go compiler (`gc`) targets a very different runtime and does not
provide a practical SH-4 backend path for this project. `gccgo` already uses
GCC's backend, which supports SH-4 targets, so libgodc can replace `libgo`
with a Dreamcast-specific runtime without having to build a new compiler
backend first.

**Why semispace instead of marksweep?**

Semispace allocation is simple, fast, and has no fragmentation. On a 16MB
console, fragmentation could eventually make large allocations fail even when
total free memory still exists. The tradeoff is that only half of the
GC-managed semispace heap is usable at once, which is acceptable for the
intended game workloads. The current large-allocation bypass has additional
tradeoffs and limitations; see
[#6](https://github.com/drpaneas/libgodc/issues/6).

**Why cooperative instead of preemptive?**

Preemptive scheduling can improve fairness and timer responsiveness, but it
also needs more machinery: timer-driven interruption, safepoints, and extra
bookkeeping. On a single-core Dreamcast, that machinery still would not make Go
goroutines run in parallel, so libgodc currently favors a simpler cooperative
model. Follow-up work on fairness without abandoning the single-threaded design
is tracked in [#12](https://github.com/drpaneas/libgodc/issues/12).

**Why fixed stacks instead of growable?**

Growable stacks require both compiler support and runtime machinery to detect
overflow, allocate a larger stack, copy active frames, and resume execution on
the new stack. Fixed stacks remove that machinery and fit the current
`-fno-split-stack` model, which keeps the runtime much simpler. The tradeoff is
that stack size becomes a real limit rather than something the runtime can
expand dynamically. The current default is 64KB for spawned goroutines, but
that is not a universal guarantee. Related follow-up work is tracked in
[#4](https://github.com/drpaneas/libgodc/issues/4),
[#5](https://github.com/drpaneas/libgodc/issues/5), and
[#10](https://github.com/drpaneas/libgodc/issues/10).

## References

 Cheney, C.J. "A Nonrecursive List Compacting Algorithm." CACM, 1970.
 Jones & Lins. "Garbage Collection." Wiley, 1996.
 The Go Programming Language Specification.
 KallistiOS Documentation.
 SH4 Software Manual, Renesas.
