# Chapter 9: Performance and Trade-offs

## Part 1: The Cache — Your Best Friend

### The Numbers That Matter

```text
┌─────────────────────────────────────────────────────────────┐
│   SH-4 MEMORY HIERARCHY                                     │
│                                                             │
│   Registers:     0 cycles (instant)                         │
│   L1 Cache:      1-2 cycles (~10 ns)                        │
│   Main RAM:      10-20 cycles (~100 ns)                     │
│   CD-ROM:        millions of cycles (200+ ms)               │
│                                                             │
│   Cache miss = 10-20× SLOWER than cache hit!                │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Cache Lines: The Free Lunch

When you read one byte from RAM, the CPU doesn't fetch just that byte. It fetches a whole **cache line** — 32 bytes on SH-4.

```text
You ask for array[0]:
┌────┬────┬────┬────┬────┬────┬────┬────┐
│ 0  │ 1  │ 2  │ 3  │ 4  │ 5  │ 6  │ 7  │  ← All 32 bytes loaded!
└────┴────┴────┴────┴────┴────┴────┴────┘
  ▲
  You wanted this one

Next 7 accesses are FREE! They're already in cache.
```

### Sequential Access: The Fast Path

```go
// FAST: Sequential access — 125 elements
sum := 0
for i := 0; i < 125; i++ {
    sum += array[i]
}
```

What happens:
```
Access array[0] → Cache miss, load 32 bytes
Access array[1] → Cache HIT (free!)
Access array[2] → Cache HIT (free!)
...
Access array[7] → Cache HIT (free!)
Access array[8] → Cache miss, load next 32 bytes
...

Total cache misses: 125 / 8 = ~16
```

### Strided Access: The Slow Path

```go
// SLOW: Strided access (every 8th element) — also 125 elements
sum := 0
for i := 0; i < 1000; i += 8 {
    sum += array[i]
}
```

What happens:
```
Access array[0]   → Cache miss
Access array[8]   → Cache miss (different cache line!)
Access array[16]  → Cache miss
Access array[24]  → Cache miss
...
Access array[992] → Cache miss

Total cache misses: 125 (EVERY access misses!)
```

Same number of additions (125), but strided is **~8× slower** because every access misses the cache.

### The Practical Lesson

```
┌─────────────────────────────────────────────────────────────┐
│   CACHE-FRIENDLY PATTERNS                                   │
│                                                             │
│   ✓ Process arrays left-to-right                            │
│   ✓ Keep related data together (struct of arrays)           │
│   ✓ Avoid pointer-chasing (linked lists are slow!)          │
│   ✓ Small, tight loops                                      │
│                                                             │
│   ✗ Random access patterns                                  │
│   ✗ Large structs with rarely-used fields                   │
│   ✗ Jumping around memory                                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Part 2: The Float64 Trap

### The Shocking Truth

Go defaults to `float64` for floating-point numbers:

```go
x := 3.14  // This is float64!
```

On a modern PC, float64 and float32 are about the same speed. On SH-4?

```
┌─────────────────────────────────────────────────────────────┐
│   FLOAT PERFORMANCE ON SH-4                                 │
│                                                             │
│   float32:  Hardware accelerated, FAST                      │
│             One instruction, one cycle                      │
│                                                             │
│   float64:  Software emulation, SLOW                        │
│             Multiple instructions, 10-20× slower!           │
│                                                             │
│   A physics simulation using float64 could run              │
│   at 6 FPS instead of 60 FPS. That's the difference.        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### The Fix

Be explicit about float32:

```go
// SLOW
x := 3.14           // float64 by default!
y := x * 2.0        // float64 math

// FAST
var x float32 = 3.14  // Explicit float32
y := x * 2.0          // float32 math
```go

For game physics, positions, velocities — always use float32.

---

## Part 3: What We Deliberately Left Out

> "Perfection is achieved not when there is nothing more to add, but when there is nothing left to take away." — Antoine de Saint-Exupéry

libgodc is not a complete Go implementation. That's intentional. Here's what we cut and why:

### Omission 1: Full Reflection

**Standard Go:** Every type carries metadata — field names, method signatures, struct tags. This enables `reflect` and fancy JSON marshaling.

**Cost:** Binary size can **double**.

**libgodc:** Basic reflection only. Enough for `println` to work.

**What you lose:**
```go
reflect.MakeFunc(...)     // NOT SUPPORTED
json.Marshal(myStruct)    // NOT SUPPORTED (would need full reflection)
```

**What you do instead:** Write explicit serialization. Use code generators.

### Omission 2: Finalizers

**Standard Go:**
```go
runtime.SetFinalizer(obj, func(o *MyType) {
    o.cleanup()  // Runs when GC collects obj
})
```

**The problem:** Finalizers are a nightmare for GC:
- Objects can be resurrected
- Run order is undefined
- Timing is unpredictable
- Complicate the GC significantly

**libgodc:** No finalizers.

**What you do instead:** Use `defer` for cleanup:
```go
func process() {
    resource := acquire()
    defer resource.Release()  // Always runs!
    // ... use resource ...
}
```

### Omission 3: Preemptive Scheduling

**Standard Go:** The runtime can interrupt a goroutine at almost any point.

**libgodc:** Goroutines must yield voluntarily.

```go
// THIS FREEZES THE SYSTEM
for {
    // Infinite loop, never yields
    // No other goroutine will EVER run
}

// THIS IS FINE
for {
    doWork()
    runtime.Gosched()  // "Let others run"
}
```

**Why we did this:** Preemption requires safe points, stack inspection, and signal handling. Complex for little benefit on single-CPU.

### Omission 4: Concurrent GC

**Standard Go:**
```
Your code:    ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
GC:                ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓
              Both run in parallel!
              Pause: < 1ms
```

**libgodc:**
```
Your code:    ░░░░░░░░░░████████████░░░░░░░░
GC:                     ▓▓▓▓▓▓▓▓▓▓▓▓
              EVERYTHING STOPS during GC
              Pause: 5-20ms
```

**Why we did this:** Concurrent GC requires write barriers, atomic operations, and careful synchronization. Stop-the-world is simpler and predictable.

**What you do:** Keep live data small. Trigger GC between frames or during loading.

### The Trade-off Table

| Feature | What We Chose | Why |
|---------|---------------|-----|
| GC | Semi-space, stop-the-world | Simple, no fragmentation |
| Scheduling | Cooperative, M:1 | No locks, predictable |
| Panic/Recover | setjmp/longjmp | No DWARF unwinding |
| Reflection | Minimal | Binary size |
| Preemption | None | Simplicity |
| C interop | Direct linking | No CGo complexity |

**Our philosophy:** Predictability over throughput. Simplicity over features.

---

## Part 4: When to Optimize

### The Golden Question

Before optimizing anything, ask:

> **"Have I measured this?"**

If the answer is no, stop. You're guessing. And programmers are notoriously bad at guessing where time is spent.

### The 90/10 Rule

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   90% of execution time is spent in 10% of the code         │
│                                                             │
│   That means:                                               │
│   • 90% of your code DOESN'T MATTER for performance         │
│   • Optimizing the wrong code = wasted effort               │
│   • Always measure first!                                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### DO Optimize

- Code that runs **every frame** (game loop, rendering)
- **Hot loops** with thousands of iterations
- Code that **measurements show** is slow

### DON'T Optimize

- Code that runs **once** (startup, level load)
- Code that runs **rarely** (menu navigation)
- Code you **haven't measured**
- At the cost of **readability**

### How to Measure

```go
//extern timer_us_gettime64
func timerUsGettime64() uint64

func measureGameLoop() {
    start := timerUsGettime64()
    
    updatePhysics()
    physicsTime := timerUsGettime64() - start
    
    renderStart := timerUsGettime64()
    renderFrame()
    renderTime := timerUsGettime64() - renderStart
    
    println("Physics:", physicsTime, "us")
    println("Render:", renderTime, "us")
}
```

Now you know where time actually goes!

---

## Part 5: The Debug Build System

### Production vs Debug

By default, libgodc is silent. Zero debug output, zero overhead.

```bash
# Production build (default)
make && make install

# Debug build - enables debug output and assertions
make DEBUG=3 && make install
```

### The Performance Tax of Debug Output

```
┌─────────────────────────────────────────────────────────────┐
│   OPERATION          Production     DEBUG=3                 │
│                                                             │
│   Goroutine spawn    50 μs          188,000 μs (188 ms!)    │
│   Channel send       19 μs          ~50,000 μs              │
│   GC pause           21 ms          ~500 ms                 │
│                                                             │
│   Debug output is EXTREMELY EXPENSIVE!                      │
│   Never benchmark with DEBUG enabled.                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Debug Macros

Instead of raw `printf`, use these macros:

| Macro | Use For | Example |
|-------|---------|---------|
| `LIBGODC_TRACE()` | General tracing | Scheduler events |
| `LIBGODC_WARNING()` | Non-fatal issues | Large allocations |
| `LIBGODC_ERROR()` | Recoverable errors | Failed operations |
| `LIBGODC_CRITICAL()` | Fatal errors | Logged to crash dump |
| `GC_TRACE()` | GC-specific | Collection details |

**In production (DEBUG=0):** All macros compile to nothing. Zero cost.

**In debug (DEBUG=3):** Output includes labels:
```
[godc:main] Scheduling G 42 (status=1)
[godc:main] WARNING: Large allocation 256 KB
[GC] #3: 1024->512 (50% survived) in 21045 us
```

### Using Debug Macros

In C runtime code:

```c
#include "runtime.h"

void my_function(void) {
    LIBGODC_TRACE("Entering my_function");
    
    if (error_condition) {
        LIBGODC_WARNING("Something unexpected: %d", value);
    }
    
    LIBGODC_TRACE("my_function complete");
}
```

In Go code, use `println`:

```go
const DEBUG = false  // Set to true when debugging

func debugPrint(msg string) {
    if DEBUG {
        println(msg)
    }
}
```

### Debug Functions Available

When investigating issues, you can call these:

```c
gc_dump_stats();       // Print GC statistics
gc_verify_heap();      // Check heap integrity
gc_print_object(ptr);  // Print object details
gc_dump_heap(10);      // Dump first 10 heap objects
```

---

## Real Benchmark Results

We ran these benchmarks on actual Dreamcast hardware. These numbers should guide your optimization decisions.

### PVRMark: Go vs Native C

We ran the KOS pvrmark benchmark (flat-shaded triangles, no textures) on real Dreamcast hardware to measure Go runtime overhead:

| Metric | C Native | Go (default) | Go (GODC_FAST) |
|--------|----------|--------------|----------------|
| **Peak polys/frame** | 17,533 | 13,833 | **14,333** |
| **Peak pps** | ~1,054,097 | ~831,714 | **~860,532** |
| **vs C performance** | 100% | 79% | **82%** |
| **Binary size** | 314 KB | 614 KB | 614 KB |

```
┌─────────────────────────────────────────────────────────────┐
│   POLYGON THROUGHPUT (polys/frame @ 60fps)                  │
│                                                             │
│   C Native:      ████████████████████████████████████ 17,533│
│   Go Optimized:  ████████████████████████████        14,333 │
│   Go Default:    ██████████████████████████          13,833 │
│                                                             │
│   GODC_FAST=1 adds +500 polys/frame (+3.6%)                 │
│   Go achieves 82% of C polygon throughput                   │
└─────────────────────────────────────────────────────────────┘
```

**Analysis:**
- The 18% overhead comes from Go→C FFI calls and loop bounds checking
- `GODC_FAST=1` improves performance by ~3.6% via aggressive optimization
- For real games with textures, lighting, and game logic, this difference is negligible
- **14,333 flat-shaded triangles at 60fps is plenty for actual gameplay**

**What the extra 300KB binary size buys you:**
- Garbage collection
- Goroutines and channels
- Defer/panic/recover
- Type safety and bounds checking
- Full Go standard library support

### Compiler Optimization Flags

The `godc build` command uses these SH-4 specific optimizations:

| Flag | Effect | Default |
|------|--------|---------|
| `-O2` | Standard optimization | ✓ |
| `-m4-single` | Single-precision FPU mode | ✓ |
| `-mfsrra` | Hardware reciprocal sqrt (10× faster) | ✓ |
| `-mfsca` | Hardware sin/cos (10× faster) | ✓ |
| `-O3` | Aggressive optimization | GODC_FAST only |
| `-ffast-math` | Fast FP (breaks IEEE) | GODC_FAST only |
| `-funroll-loops` | Loop unrolling | GODC_FAST only |

**To enable aggressive optimizations:**

```bash
GODC_FAST=1 godc build
```

**Warning:** `-ffast-math` breaks IEEE floating point compliance. NaN and infinity handling may not work correctly. Use only for games where FP precision isn't critical.


