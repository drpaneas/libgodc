# Chapter 4: Goroutines

## The Trade-off

Let me set expectations: **goroutines on Dreamcast work, but differently than on modern hardware.**

You get zero parallelism (single CPU), but you get everything else: clean concurrency primitives, channels, and code that feels like Go.

Here's the thing. Goroutines shine when you have multiple CPU cores:

```
Modern PC (8 cores):
────────────────────────────────────────────────────────────
Core 1: [──────goroutine A──────]
Core 2: [──────goroutine B──────]
Core 3: [──────goroutine C──────]
Core 4: [──────goroutine D──────]
...
        ↑
        All running SIMULTANEOUSLY
        4x faster than running them one-by-one!
```

But Dreamcast?

```
Dreamcast (1 core):
────────────────────────────────────────────────────────────
CPU:    [───A───][───B───][───A───][───C───][───B───]...
        ↑
        Only ONE runs at a time
        ZERO parallelism benefit
```

So why did libgodc implements them?

---

## Why Bother?

**Because Go without goroutines isn't Go.**

Imagine porting Python to a machine without lists. Or JavaScript without callbacks. You could do it, but would it feel like the same language?

I wanted Go on Dreamcast to feel like Go. You can write:

```go
go processEnemies()
go playBackgroundMusic()
go handleInput()
```

It works. It's correct. The code is cleaner. It's just not *faster* than calling them directly:

```go
processEnemies()
playBackgroundMusic()
handleInput()
```

There's overhead—but less than you might expect. Let's see the numbers.

---

## What Happens Under the Hood

When you create a goroutine, here's what actually happens:

```
┌─────────────────────────────────────────────────────────────┐
│   go doSomething()                                          │
│   ────────────────                                          │
│                                                             │
│   1. Allocate 64 KB stack (from pool or malloc)             │
│   2. Initialize G struct (~150 bytes)                       │
│   3. Save 16 CPU registers to context                       │
│   4. Set up context (sp, pc, pr)                            │
│   5. Add to run queue                                       │
│   6. Later: context switch to run (~6.6 μs)                 │
│   ─────────────────────────────────────────────────────     │
│   Total spawn + first run: ~32 μs                           │
│                                                             │
│   That's ~6,400 CPU cycles per goroutine spawn!             │
└─────────────────────────────────────────────────────────────┘
```

What do you get for this overhead? On a multi-core system: parallelism. On Dreamcast: proper Go semantics and working concurrency primitives. That's actually worth something!

### The Numbers

I ran benchmarks on real Dreamcast hardware (from `bench_architecture.elf`):

```
┌─────────────────────────────────────────────────────────────┐
│   OPERATION               TIME                              │
├─────────────────────────────────────────────────────────────┤
│   runtime.Gosched()       120 ns      ← very cheap!         │
│   Buffered channel op     ~1.5 μs                           │
│   Context switch          ~6.6 μs                           │
│   Channel round-trip      ~13 μs                            │
│   Goroutine spawn+run     ~34 μs                            │
└─────────────────────────────────────────────────────────────┘
```

At 200 MHz, you get about 200 million cycles per second. At 60 FPS you have 3.3 million cycles per frame. A 34 μs goroutine spawn is ~6,800 cycles—that's only 0.2% of your frame budget. You can afford a few goroutines per frame, just don't spawn hundreds!

> See the [Glossary](../appendix/glossary.md#performance-numbers) for a complete reference of all benchmark numbers.

---

## How It Works

The implementation is pretty elegant for a 200 MHz machine. Let's see how we create the illusion of concurrency.

### The G Struct

Every goroutine is a `G` structure (see `runtime/goroutine.h`):

```
┌─────────────────────────────────────────────────────────────┐
│   Goroutine (G)                                             │
│                                                             │
│   _panic:     nil         (current panic - offset 0)        │
│   _defer:     nil         (deferred functions - offset 4)   │
│   atomicstatus: Grunning  (or Gwaiting, Grunnable, etc.)    │
│   schedlink:  next G      (run queue linkage)               │
│   stack_lo:   0x8c100000  (bottom of stack)                 │
│   stack_hi:   0x8c110000  (top of stack, 64 KB above)       │
│   context:    saved CPU registers (64 bytes)                │
│                           ├── r8-r14 (callee-saved GPRs)    │
│                           ├── sp, pc, pr (special)          │
│                           └── fr12-fr15, fpscr, fpul (FPU)  │
│   goid:       42          (unique ID - 8 bytes)             │
│   waiting:    sudog*      (channel wait queue entry)        │
│   checkpoint: ptr         (for panic/recover)               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

The key is `context`, _aka_ the saved CPU registers. This lets us pause mid-function and resume later.

### The Run Queue

Runnable goroutines wait in line:

```
     head                                    tail
       ↓                                       ↓
    ┌────┐   ┌────┐   ┌────┐   ┌────┐
    │ G3 │──▶│ G7 │──▶│ G2 │──▶│ G9 │──▶ NULL
    └────┘   └────┘   └────┘   └────┘
      ↑
   "I'm next!"
```

The scheduler is simple:

```c
while (true) {
    G *gp = runq_get();       // Get next goroutine
    if (gp) {
        switch_to(gp);        // Run it
    }
    // When it yields, we come back here
}
```

### Context Switching

This is where the magic happens. We're running goroutine A, and we need to switch to B:

```
STEP 1: Save A's registers to A's context
────────────────────────────────────────────────────────
        CPU                         A's Context
    ┌─────────┐                   ┌─────────┐
    │ r8 = 42 │ ────────────────▶ │ r8 = 42 │
    │ r9 = 17 │                   │ r9 = 17 │
    │ sp = X  │                   │ sp = X  │
    │ pc = Y  │                   │ pc = Y  │
    └─────────┘                   └─────────┘


STEP 2: Load B's registers from B's context  
────────────────────────────────────────────────────────
    B's Context                       CPU
    ┌─────────┐                   ┌─────────┐
    │ r8 = 99 │ ────────────────▶ │ r8 = 99 │
    │ r9 = 55 │                   │ r9 = 55 │
    │ sp = P  │                   │ sp = P  │
    │ pc = Q  │                   │ pc = Q  │
    └─────────┘                   └─────────┘


STEP 3: Return (now running B!)
────────────────────────────────────────────────────────
CPU continues from B's saved PC with B's saved registers.
To B, it's like it never stopped running!
```

On SH-4, we save/restore 16 registers (64 bytes). The full context switch with FPU takes ~88 cycles. With lazy FPU optimization (skipping FPU for integer-only goroutines), it drops to ~38 cycles. At 200 MHz, that's under 0.5 microseconds—the total yield path including scheduler overhead is ~6.6 μs as shown in the benchmarks.

---

## Cooperative Scheduling: The Gotcha

Our scheduler is **cooperative**, not preemptive. This is different from official Go!

**Preemptive** (official Go since 1.14): The runtime can forcibly pause a goroutine at any time using timer interrupts or signals. Even an infinite loop gets interrupted so other goroutines can run.

**Cooperative** (libgodc): Goroutines must **volunteer** to give up the CPU. The runtime never forces a switch. If a goroutine doesn't yield, nothing else runs.

Why the difference? Preemptive scheduling requires:
- Signal handlers or timer interrupts to interrupt running code
- Complex stack inspection to find safe preemption points
- More saved state per context switch

On Dreamcast, we keep it simple. The cost is that you must be careful:

```go
// This freezes your Dreamcast (but works fine in official Go!):
func badGoroutine() {
    for {
        x++  // Infinite loop, never yields
    }
}
```

### Where Goroutines Yield

```
┌─────────────────────────────────────────────────────────────┐
│   YIELDS (lets others run)         DOESN'T YIELD            │
├─────────────────────────────────────────────────────────────┤
│   ✓ Channel send: ch <- x          ✗ Math: x + y * z        │
│   ✓ Channel receive: <-ch          ✗ Memory: array[i]       │
│   ✓ time.Sleep()                   ✗ Loops: for i := ...    │
│   ✓ runtime.Gosched()                                       │
│   ✓ select {}                                               │
└─────────────────────────────────────────────────────────────┘
```

### The Fix for Long Computations

```go
// Bad: No yields for 10 million iterations
for i := 0; i < 10000000; i++ {
    result += compute(i)
}

// Good: Yield periodically
for i := 0; i < 10000000; i++ {
    result += compute(i)
    if i % 10000 == 0 {
        runtime.Gosched()  // Let others run
    }
}
```

Note: if you have a single long computation with no natural yield points, a direct function call is simpler. Goroutines shine when you have *multiple* things that can interleave.

---

## When Goroutines Shine

Goroutines work well for several patterns. Here's real benchmark data from `bench_goroutine_usecase.elf`:

```
┌─────────────────────────────────────────────────────────────┐
│   USE CASE                    OVERHEAD    VERDICT           │
├─────────────────────────────────────────────────────────────┤
│   Multiple independent tasks  10-38%      ✓ Acceptable      │
│   Producer-consumer pattern   ~163%       ⚠ Use carefully   │
│   Channel ping-pong           ~13 μs/op   Know the cost     │
└─────────────────────────────────────────────────────────────┘
```

The key insight: **independent tasks** (each goroutine does its own work, minimal channel communication) have reasonable overhead (typically ~25%, varies with scheduling). **Heavy channel use** (producer-consumer with many sends) costs ~163%.

### Porting Existing Go Code

If you're porting Go code that uses goroutines, it works without modification:

```go
// This Go code just works:
func fetch(urls []string) []Result {
    ch := make(chan Result, len(urls))
    for _, url := range urls {
        go func(u string) {
            ch <- download(u)
        }(url)
    }
    // ... collect results
}
```

## Patterns to Avoid

Some patterns don't make sense on a single-core system:

### Don't: Spawn Per-Item

```go
// Inefficient: 1000 spawns = 32 ms overhead
for i := 0; i < 1000; i++ {
    go process(items[i])
}

// Better: Process directly, or use one goroutine
for i := 0; i < 1000; i++ {
    process(items[i])
}
```

### Don't: Force Sequential With Channels

```go
// Overcomplicated: These are sequential anyway
go step1()
<-done1
go step2()
<-done2

// Simpler:
step1()
step2()
```

### Be Careful: Heavy Channel Traffic

```go
// Each channel op is ~13 μs
// High-volume producer-consumer shows ~163% overhead
for item := range items {
    workChan <- item
}
```

For high-throughput paths, batch items or use direct calls.
