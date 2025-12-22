# Limitations

This document describes the known limitations of libgodc. Understanding these
is essential for writing reliable Dreamcast Go programs.

## Memory

### 16MB Total

The Dreamcast has 16MB of RAM. No virtual memory, no swap, no second chance.

Budget your memory:
- KOS + drivers: ~1MB
- Your code: ~1-3MB  
- GC heap: 2MB active (4MB total, two semi-spaces)
- Goroutine stacks: 64KB each
- Everything else: KOS malloc

When you run out, you crash.

### Goroutine Memory Overhead

Dead goroutines retain approximately 160 bytes each (G struct only).
The stack memory and TLS are properly reclaimed, and the G struct is kept in a
free list for reuse by future goroutines.

**Why the free list?** Reusing G structs avoids repeated malloc/free overhead.
When you spawn a new goroutine, it reuses a G from the free list if available.

**Impact:** If you spawn 10,000 goroutines that all exit without spawning new
ones, you'll have ~1.6MB in the free list. This memory is reused when you spawn
new goroutines.
For a typical game session, this is rarely a problem if you design with
long-lived goroutines.

**Workaround:** Prefer long-lived goroutines or let the free list grow to a
stable size. If you spawn and exit many goroutines, the G structs accumulate
in the free list but are reused:

```go
// GOOD: Fixed set of long-lived goroutines
go audioHandler()      // Lives for entire game
go inputPoller()       // Lives for entire game
go gameLoop()          // Lives for entire game

// OK: Spawning goroutines per-event (G structs are reused)
for event := range events {
    go handleEvent(event)  // ~160B stays in free list for reuse
}
```

### GC Pause Times

The garbage collector stops the world during collection. Pause times depend
on live heap size:

| Live Heap | Pause    |
|-----------|----------|
| 100KB     | 1-2ms    |
| 500KB     | 5-10ms   |
| 1MB       | 10-20ms  |

At 60fps, you have 16.6ms per frame. A 10ms GC pause causes visible stutter.

**Workarounds:**

1. Keep the live heap small (<500KB)
2. Disable automatic GC for action sequences:
   ```go
   debug.SetGCPercent(-1)  // Disable automatic GC
   runtime.GC()            // Manual GC during loading screens
   ```
3. Use KOS malloc for large, long-lived data (textures, audio, levels)

### Fixed 64KB Stacks

Goroutine stacks do not grow. Each goroutine gets exactly 64KB.

This limits recursion depth:

| Frame Size | Safe Depth |
|------------|------------|
| 50 bytes   | ~300       |
| 100 bytes  | ~150       |
| 250 bytes  | ~60        |
| 500 bytes  | ~30        |

**Workarounds:**

1. Convert recursion to iteration
2. Use smaller local variables
3. Pass large data by pointer, not by value
4. Avoid deep call chains

```go
// BAD: Large local arrays
func processLevel(depth int) {
    var buffer [4096]byte  // 4KB per stack frame!
    // ... recursive call
}

// GOOD: Heap allocation for large buffers
func processLevel(depth int) {
    buffer := make([]byte, 4096)  // GC heap
    // ... recursive call
}
```

## Scheduling

### No Parallelism (M:1)

All goroutines run on a single thread. The `go` keyword provides concurrency
(interleaved execution), not parallelism (simultaneous execution).

There is no benefit from GOMAXPROCS—the Dreamcast has one CPU core.

### No Preemption

Goroutines yield only at explicit points:
- Channel operations
- `runtime.Gosched()`
- `time.Sleep()`
- Timer operations

A goroutine in a tight loop blocks all other goroutines:

```go
// BAD: Blocks entire system
for {
    calculateNextFrame()  // Never yields!
}

// GOOD: Explicit yield
for {
    calculateNextFrame()
    runtime.Gosched()  // Let others run
}
```

### Channel Lock Contention

Under high contention, channel locks use spin-yield loops. Many goroutines
racing for the same channel wastes CPU.

**Workaround:** Use buffered channels to reduce contention:

```go
// Unbuffered: every send/receive contends
events := make(chan Event)

// Buffered: reduced contention
events := make(chan Event, 16)
```

## Language Features

### Not Implemented

- Race detector
- CPU/memory profiling
- Debugger support (delve, gdb)
- Plugin package
- cgo (use KOS C functions directly via `//extern`)

### Limited Implementation

- **reflect:** Basic type inspection only. No `reflect.MakeFunc`.
- **unsafe:** Works, but remember pointers are 4 bytes.
- **sync:** Mutexes work, but see M:1 scheduling caveat—no goroutine runs
  while you hold a lock, so deadlock is impossible but starvation is easy.

### Unrecoverable Runtime Panics

User `panic()` is recoverable via `recover()`. Runtime panics are not:

- Nil pointer dereference
- Array/slice bounds check
- Integer divide by zero
- Stack overflow

These crash immediately. There is no recovery.

**Why?** A bounds check failure means your program's invariants are violated.
Continuing would corrupt data. It's better to crash cleanly.

## Platform Constraints

### 32-bit Pointers

All pointers are 4 bytes. Code assuming 64-bit pointers will break:

```go
// BAD: Assumes 64-bit
type Header struct {
    flags uint32
    ptr   uintptr  // 4 bytes on Dreamcast, not 8!
    size  uint32
}
```

### Single-Precision FPU

The SH-4 FPU operates in single precision (`-m4-single`). Double precision
operations are emulated in software—extremely slow.

```go
// FAST: Single precision
var x float32 = 3.14

// SLOW: Software emulation
var y float64 = 3.14159265358979
```

Avoid `float64` in hot paths. The compiler flag `-m4-single` makes all FPU
operations single precision, but libraries may still use doubles.

### Cache Coherency

The SH-4 has separate instruction and data caches. DMA operations require
explicit cache management using KOS functions:

```c
// Before DMA write (CPU -> hardware):
dcache_flush_range((uintptr_t)ptr, size);   // Flush data cache

// After DMA read (hardware -> CPU):
dcache_inval_range((uintptr_t)ptr, size);  // Invalidate data cache
```

The GC handles cache management for semi-space flips via incremental
invalidation, but your DMA code must handle it explicitly using KOS cache
functions.

### No Signals

There are no Unix signals. `os.Signal`, `signal.Notify`, etc. don't work.
Use KOS's interrupt handlers or polling instead.

### No Networking (by default)

Networking requires a Broadband Adapter (BBA) or modem. Most Dreamcast units
don't have one. Design your game to work offline.

## Debugging

### Available

- Serial output via `println()` (routed to dc-tool)
- `LIBGODC_ERROR` / `LIBGODC_CRITICAL` macros (defined in runtime.h)
- GC statistics via the C function `gc_stats(&used, &total, &collections)`
- `runtime.NumGoroutine()` to count active goroutines
- KOS debug console (`dbglog()`)

### Not Available

- Stack traces on panic (limited)
- Core dumps
- Breakpoints
- Variable inspection
- Heap profiling

When something goes wrong, you have `println()` and your brain. Use them.

## Compatibility

### gccgo Only

This runtime is for gccgo (GCC's Go frontend), not the standard gc compiler.
Code compiled with `go build` will not work. Use `sh-elf-gccgo`.

### KallistiOS Required

libgodc requires KallistiOS. It won't work with other Dreamcast development
libraries.

### SH-4 Architecture Only

This code is specifically for the Hitachi SH-4 CPU. It won't run on other
architectures.

## Summary

| Limitation              | Impact                    | Workaround                    |
|-------------------------|---------------------------|-------------------------------|
| G struct pooling        | ~160B per dead goroutine  | Long-lived goroutines         |
| GC pauses               | 1-20ms depending on heap  | Small heap, manual GC timing  |
| M:1 scheduling          | No parallelism            | Explicit yields               |
| Fixed stacks            | Limited recursion         | Iteration, smaller frames     |
| No preemption           | Tight loops block all     | `runtime.Gosched()`           |
| Runtime panics          | Unrecoverable             | Defensive coding              |
| 16MB RAM                | Memory pressure           | Monitor usage, plan carefully |

For typical Dreamcast games—15-60 minute sessions with a fixed goroutine
architecture—these limitations are manageable. Design with constraints in
mind from the start, and you'll have a runtime that's simple, fast, and
reliable.
