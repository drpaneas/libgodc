# Limitations

This document describes the known limitations of libgodc. Understanding these
is essential for writing reliable Dreamcast Go programs.

## Memory

### 16MB Total

The Dreamcast has 16MB of RAM. No virtual memory, no swap, no second chance.

Budget your memory:
- KOS + drivers: ~1MB
- Your code: build-dependent
- GC heap: 2MB active by default (4MB total, two semi-spaces)
- Spawned goroutine stacks: 64KB each by default
- Main goroutine stack: KOS main-thread stack (128KB by default)
- Everything else: KOS malloc

When you run out, you crash.

### Goroutine Memory Overhead

The runtime contains a dead-goroutine queue and a `freegs` reuse path for `G`
structs, but in the current source exited goroutines do not age into
reclaimable state because `global_generation` is never advanced.

That means exited goroutines do not currently reach the cleanup path that would
reclaim their stack and TLS and then recycle the `G` struct.

**Workaround:** Prefer long-lived goroutines and avoid high-churn
spawn/exit patterns:

```go
// GOOD: Fixed set of long-lived goroutines
go audioHandler()      // Lives for entire game
go inputPoller()       // Lives for entire game
go gameLoop()          // Lives for entire game

// Risky today: spawning goroutines per-event can accumulate unreclaimed state
for event := range events {
    go handleEvent(event)
}
```

### GC Pause Times

The garbage collector effectively stops the world for Go goroutines during
collection. Pause times depend primarily on live heap size and object layout:

| Live Heap | Pause    |
|-----------|----------|
| 100KB     | 1-2ms    |
| 500KB     | 5-10ms   |
| 1MB       | 10-20ms  |

At 60fps, you have 16.6ms per frame. A 10ms GC pause consumes most of that
budget and can cause visible stutter.

**Workarounds:**

1. Keep the live heap small (<500KB)
2. Disable threshold-triggered GC for action sequences:
   ```go
   debug.SetGCPercent(-1)  // Disable threshold-triggered GC
   runtime.GC()            // Manual GC during loading screens
   ```
   This reduces surprise GC pauses, but it is not a hard guarantee: if an
   allocation would overflow the active semispace, GC still runs.
3. Use non-moving memory for large raw buffers (textures, audio, levels). In
   the current runtime, allocations larger than 64KB bypass the semispace heap
   and use `malloc()`. This is useful for raw buffers, but large typed Go
   allocations that contain pointers are a known limitation; see
   [#6](https://github.com/drpaneas/libgodc/issues/6).

### Fixed Spawned-Goroutine Stacks

Spawned goroutine stacks do not grow. By default each spawned goroutine gets
64KB, while the main goroutine uses the KOS main-thread stack.

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
- Blocking channel operations
- `runtime.Gosched()`
- `time.Sleep()`
- Timer operations
- Non-blocking `select`/`default` when no case is ready

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

Under high contention, many goroutines contending for the same channel spend
time parking and waking through the wait queues. Channel locking is still a
serialization point, but it is not implemented as a spin-yield loop.

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
- **sync:** Mutexes work, but M:1 scheduling does not make deadlocks
  impossible. Avoid blocking or sleeping while holding locks, and keep
  critical sections short.

### Panic Recovery Is Limited

`panic()` enters the panic/recover machinery, and helper paths for nil
dereference, bounds failures, and divide-by-zero currently go through
`runtime_panicstring()` as well.

To resume execution after recovery, the runtime expects a checkpoint to have
been established earlier. A recovered panic without a checkpoint becomes fatal
(`recover without checkpoint`).

Some failures still abort immediately, including fatal `runtime_throw()` paths
and interface type-assertion panic helpers that call `abort()` directly.

For gameplay code, the practical rule is still the same: do not rely on panic
recovery for ordinary control flow.

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

The GC handles cache management for semispace flips via incremental
invalidation, but your DMA code must handle cache coherency explicitly using
KOS cache functions.

This is only part of DMA safety. Cache management makes CPU and hardware agree
about the bytes at a given address, but it does not stop the GC from moving a
small heap buffer to a different address mid-transfer. DMA code therefore needs
both:

1. Correct cache flush/invalidate calls.
2. A stable, non-moving buffer for the lifetime of the transfer.

Longer-term API work to make DMA-safe memory explicit is tracked in
[#11](https://github.com/drpaneas/libgodc/issues/11).

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
| Exited goroutine cleanup | High spawn/exit churn retains stack/TLS state | Long-lived goroutines |
| GC pauses               | 1-20ms depending on heap  | Small heap, manual GC timing  |
| M:1 scheduling          | No parallelism            | Explicit yields               |
| Fixed stacks            | Limited recursion         | Iteration, smaller frames     |
| No preemption           | Tight loops block all     | `runtime.Gosched()`           |
| Panic recovery          | Checkpoint-based and limited | Avoid panic-driven control flow |
| 16MB RAM                | Memory pressure           | Monitor usage, plan carefully |

For typical Dreamcast games—15-60 minute sessions with a fixed goroutine
architecture—these limitations are manageable. Design with constraints in
mind from the start, and you'll have a runtime that's simple, fast, and
reliable.
