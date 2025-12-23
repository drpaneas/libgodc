# Introduction to libgodc

## What Is This Book?

This book is about building a Go runtime for the Sega Dreamcast.

Wait, what?

```text
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   THE CRAZY PROJECT                                         │
│                                                             │
│   Go:                                                       │
│   • Designed for servers and cloud computing                │
│   • Expects gigabytes of RAM                                │
│   • Has a sophisticated garbage collector                   │
│   • Written for modern multi-core CPUs                      │
│                                                             │
│   Dreamcast:                                                │
│   • A game console from 1998                                │
│   • Has 16 MB of RAM (megabytes, not giga)                  │
│   • Single CPU core at 200 MHz                              │
│   • Was designed for arcade games                           │
│                                                             │
│   These shouldn't work together. But they do.               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

We call this project **libgodc**, a library that implements Go's runtime for the Dreamcast. By the end of this book, you'll understand how we built the Dreamcast Go runtime from scratch: memory allocation, garbage collection, goroutine scheduling, channels, and more.

---

## Who Is This Book For?

You should read this book if:

- You're curious how programming languages work "under the hood"
- You want to understand what a runtime actually does
- You enjoy systems programming and low-level details
- You think retro game consoles are cool

You'll need to know:

- Basic Go (variables, functions, structs, goroutines)
- Some C (pointers, memory, basic syntax)
- What a compiler does (turns source code into machine code, duh!)

You don't need to know:

- Assembly language (we'll explain what you need)
- How to program the Dreamcast (KallistiOS handles the hard parts)
- Anything about garbage collectors (we'll build one together)

---

## The Machine We're Programming

Let's meet our hardware. The Sega Dreamcast (1998) was ahead of its time—the first 128-bit console, they said! (Marketing math, but still impressive.)

```text
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   THE SEGA DREAMCAST                                        │
│                                                             │
│   ┌─────────────────────────────────────────────────────┐   │
│   │                                                     │   │
│   │   CPU:     Hitachi SH-4 @ 200 MHz                   │   │
│   │                                                     │   │
│   │   RAM:     16 MB (yes, that's megabytes, not giga)  │   │
│   │                                                     │   │
│   │   VRAM:    8 MB (for the GPU)                       │   │
│   │                                                     │   │
│   │   GPU:     PowerVR2 CLX2                            │   │
│   │                                                     │   │
│   │   Sound:   Yamaha AICA (has its own ARM7 + 2 MB)    │   │
│   │                                                     │   │
│   │   Storage: GD-ROM (or SD card adapter)              │   │
│   │                                                     │   │
│   └─────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

For comparison, your phone probably has:
- 4-8 CPU cores at 2+ GHz
- 4-8 GB of RAM
- Virtual memory, memory protection, multiple privilege levels

The Dreamcast has:
- 1 CPU core at 200 MHz
- 16 MB of RAM
- No virtual memory, no memory protection, no privilege levels

Different world.

---

## Why Can't We Just Use Standard Go?

Go has an official compiler called `gc`. It generates code for x86, ARM, and other modern architectures.

The Dreamcast uses a **SuperH SH-4** processor. Adding SH-4 support to `gc` would require rewriting significant portions of the compiler backend—months of work, requiring deep expertise in both Go internals and the SH-4 architecture. That's a project for a team of compiler engineers with sleepless nights, questionable caffeine consumption, and possibly mild insanity.

Instead, we use **gccgo**, an alternative Go compiler built on GCC. GCC already supports SH-4 (from decades of embedded development). So gccgo can compile Go to SH-4—we just need to provide the runtime.

```go
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   TWO PATHS TO GO ON DREAMCAST                              │
│                                                             │
│   Path A: Modify gc                                         │
│   ─────────────────────                                     │
│   - Write a new SH-4 backend                                │
│   - Write a new Dreamcast Operating System                  │
│   - Understand SSA, register allocation, etc.               │
│   - Result: "real" Go on Dreamcast                          │
│                                                             │
│   Path B: Use gccgo + write runtime (this book)             │
│   ────────────────────────────────────────────              │
│   - GCC already knows SH-4                                  │
│   - Write runtime in C                                      │
│   - Result: Go dialect for Dreamcast                        │
│                                                             │
│   We chose Path B. It's faster and teaches more.            │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## The 16 Megabyte Problem

Sixteen megabytes. That's it. Everything must fit:

```text
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   16 MB = 16,777,216 bytes                                  │
│                                                             │
│   That's shared between:                                    │
│                                                             │
│   ┌─────────────────────────────────────────────────┐       │
│   │  Your program's code           (0.5 - 2 MB)     │       │
│   ├─────────────────────────────────────────────────┤       │
│   │  KallistiOS overhead           (~0.5 MB)        │       │
│   ├─────────────────────────────────────────────────┤       │
│   │  Go runtime heap               (??? MB)         │       │
│   ├─────────────────────────────────────────────────┤       │
│   │  Goroutine stacks              (??? MB)         │       │
│   ├─────────────────────────────────────────────────┤       │
│   │  Game assets (textures, etc.)  (??? MB)         │       │
│   └─────────────────────────────────────────────────┘       │
│                                                             │
│   Everything fights for space.                              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

This is why our garbage collector choice matters so much. We use a **semi-space copying collector**, which needs two equally-sized spaces. libgodc allocates 2 MB per space = 4 MB total = **2 MB usable heap**.

```text
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   Semi-space GC memory usage (libgodc default):             │
│                                                             │
│   ┌─────────────────────┬─────────────────────┐             │
│   │    FROM-SPACE       │     TO-SPACE        │             │
│   │      2 MB           │       2 MB          │             │
│   │                     │                     │             │
│   │  (active heap)      │  (empty, waiting    │             │
│   │                     │   for next GC)      │             │
│   └─────────────────────┴─────────────────────┘             │
│                                                             │
│   Total: 4 MB for a 2 MB usable heap. That's 50% overhead!  │
│                                                             │
│   But: no fragmentation, simple, predictable.               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

> **Design decision:** We chose simplicity (semi-space GC) over memory efficiency. On a 16 MB machine, this hurts. But a more memory-efficient collector would be much more complex to implement and debug. The 2 MB usable heap is sufficient for most Dreamcast games—large assets like textures should use external allocation anyway. For games needing more RAM, compile with `-DGC_SEMISPACE_SIZE_KB=1024` to shrink the heap to 1 MB usable (2 MB total).

---

## Where Does Everything Live?

The Dreamcast has 16 MB of main RAM at addresses `0x8C000000` to `0x8CFFFFFF`. Here's how it's organized:

```text
    0x8C000000 ──────────────────────────────────────────────
                 │
                 │   KOS kernel + drivers (~1 MB)
                 │
                 ├──────────────────────────────────────────
                 │   .text (your compiled code)
                 │   .rodata (constants, type descriptors)
                 │   .data (initialized globals)
                 │   .bss (uninitialized globals)
                 ├──────────────────────────────────────────
                 │
                 │   KOS malloc heap (everything below):
                 │
                 │   ┌─────────────────────────────────────┐
                 │   │  GC semi-space 0 (2 MB)             │
                 │   ├─────────────────────────────────────┤
                 │   │  GC semi-space 1 (2 MB)             │
                 │   ├─────────────────────────────────────┤
                 │   │  Goroutine stacks (64 KB each)      │
                 │   ├─────────────────────────────────────┤
                 │   │  Textures, audio, game assets       │
                 │   └─────────────────────────────────────┘
                 │
                 ├──────────────────────────────────────────
                 │   Main thread stack (grows downward)
                 │
    0x8CFFFFFF ──────────────────────────────────────────────

                 Total: 16 MB (0x1000000 bytes)
```

KOS manages the heap via `malloc`. When you run out of memory, `malloc` returns NULL and your program crashes. There's no virtual memory, no swap file, no second chance. See our implementation friendly messages (lol):

```c
// runtime/gc_heap.c
if (gc_heap.alloc_ptr + total_size > gc_heap.alloc_limit)
    runtime_throw("out of memory");

// runtime/stack.c  
void *base = memalign(8, size);
if (!base)
    runtime_throw("stack_alloc: out of memory");

// runtime/chan.c
c = (hchan *)gc_alloc(totalSize, &__hchan_type);
if (!c)
    runtime_throw("makechan: out of memory");

// runtime/tls_sh4.c
tls = (tls_block_t *)malloc(sizeof(tls_block_t));
if (!tls)
    runtime_throw("tls_alloc: out of memory");
```

---

## The SH-4 Processor

Let's get to know the CPU that runs our code.

### The Alignment Rule

Here's something that will bite you if you forget it:

**The SH-4 requires natural alignment.**

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   Type          Size     Must be aligned to                 │
│   ────          ────     ──────────────────                 │
│   uint8         1 byte   Any address is fine                │
│   uint16        2 bytes  Address must be divisible by 2     │
│   uint32        4 bytes  Address must be divisible by 4     │
│   uint64        8 bytes  Address must be divisible by 8     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

On x86 (your laptop), unaligned access is just slow. On SH-4, **it crashes the CPU**.

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   x86 (your laptop):                                        │
│   Unaligned access?  → Works, but slower                    │
│                                                             │
│   SH-4 (Dreamcast):                                         │
│   Unaligned access?  → ADDRESS ERROR EXCEPTION              │
│                         System crashes. No recovery.        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

Our allocator must always return properly aligned addresses.

### The Floating Point Unit

The SH-4 has a powerful FPU with a twist:

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   Single-precision (float32):  FAST! ✓                      │
│   - Hardware accelerated                                    │
│   - Multiply-add in 1 cycle                                 │
│                                                             │
│   Double-precision (float64):  Slow ✗                       │
│   - Takes many more cycles                                  │
│   - Avoid in performance-critical code                      │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```go

Go defaults to `float64`. For games, consider using `float32` where precision isn't critical.
Sadly making `float32` the new default for our libgodc is not possible. Unless someone, is
crazy enough to recompile `gccgo` and change all the consts and all the standard library
to use `float32`, that is a massive work, especially around the math libraries and ones
that depend on it. So, just remember to use `float32` and never `float64`.

A better way to solve this, in the future, would be to create `float32` wrappers around common
math functions.

---

## The Cache Problem

The SH-4 has a 16 KB data cache with "write-back" behavior. When you write data, it might only go to the cache, not to main memory.

```text
THE PROBLEM:
════════════

  Your code writes to address 0x8C100000
          │
          ▼
  ┌───────────────┐
  │    CACHE      │  ← Data goes HERE
  │  (new value)  │
  └───────────────┘
          
  ┌───────────────┐
  │  MAIN MEMORY  │  ← But not HERE (yet)
  │  (old value)  │
  └───────────────┘
          │
          ▼
  GPU reads from 0x8C100000
  Gets the OLD value!  💥
```

We have to manually flush the cache before hardware reads from memory:

```c
dcache_flush_range(addr, len);  // Push cache → memory
```

On your laptop, the OS handles this. On the Dreamcast, it's our job.

---

## KallistiOS: The Foundation

We're not programming bare-metal. We build on **KallistiOS** (KOS), the standard SDK for Dreamcast homebrew.

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   ┌───────────────────────────────────────────────────┐     │
│   │              Your Go Program                      │     │
│   └───────────────────────────────────────────────────┘     │
│                          │                                  │
│                          ▼                                  │
│   ┌───────────────────────────────────────────────────┐     │
│   │                  libgodc                          │     │
│   │  (Go runtime: GC, scheduler, channels, etc.)      │     │
│   └───────────────────────────────────────────────────┘     │
│                          │                                  │
│                          ▼                                  │
│   ┌───────────────────────────────────────────────────┐     │
│   │               KallistiOS                          │     │
│   │  (hardware abstraction, malloc, timers)           │     │
│   └───────────────────────────────────────────────────┘     │
│                          │                                  │
│                          ▼                                  │
│   ┌───────────────────────────────────────────────────┐     │
│   │            Dreamcast Hardware                     │     │
│   └───────────────────────────────────────────────────┘     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

KOS is a **minimal embedded operating system** that gets statically linked into your program. There's no user/kernel mode separation, no process isolation, and no memory protection. Your code runs with full hardware access, alongside the KOS kernel.

---

## The Constraints That Shape Everything

These hardware limitations drive every decision in libgodc:

### Constraint 1: No Memory Protection

On your laptop, accessing invalid memory gives: `Segmentation fault (core dumped)`

On the Dreamcast: the program corrupts silently or crashes without explanation.

### Constraint 2: Real-Time Requirements

Games need consistent frame rates. At 60 FPS, you have **16.67 milliseconds** per frame:

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   One frame = 16.67 ms                                      │
│                                                             │
│   ┌─────────────────────────────────────────────────────┐   │
│   │░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │   │
│   └─────────────────────────────────────────────────────┘   │
│   Game logic  Rendering             GC pause                │
│   ░░░░░░░░░░  ░░░░░░░░░░░░░░░      ░░░░                     │
│                                      ▲                      │
│                                      │                      │
│                        If GC takes 20ms, you miss frames!   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Constraint 3: Single Core

The SH-4 is single-core CPU. Even if we wanted parallel GC, the SH-4 can't run threads
simultaneously. That said, when GC runs, everything stops.
