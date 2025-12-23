# The Toolchain

## In this chapter

- You learn why we use `gccgo` instead of the standard Go compiler
- You see how Go code becomes Dreamcast machine code
- You understand the "holes" in compiled code and how we fill them
- You discover the dark arts: making C pretend to be Go
- You learn about calling conventions and type descriptors

---

## Why gccgo?

A compiler is just a program that writes programs. Most Go developers use `gc`, the standard Go compiler. It's fast, produces excellent code, and has a fantastic runtime.

But `gc` only speaks certain architectures:

```text
┌─────────────────────────────────────────┐
│                                         │
│     gc compiler's architecture list     │
│                                         │
│     ✓ x86-64   laptops, desktops        │
│     ✓ ARM64    phones, Raspberry Pi     │
│     ✓ RISC-V   new trend                │
│                                         │
│     ✗ SH-4     "never heard of this"    │
│                                         │
└─────────────────────────────────────────┘
```

The Dreamcast uses a Hitachi **SuperH SH-4** processor. Adding support to `gc` would require modifying the compiler backend—months of work, lots of caffeine, and at least three existential crises.

But here's the thing: **GCC has supported the SH-4 for over two decades.**

```go
┌─────────────────┐         ┌─────────────────┐
│   gc compiler   │         │   GCC compiler  │
│                 │         │                 │
│  Knows Go ✓     │         │  Knows Go ✗     │
│  Knows SH-4 ✗   │         │  Knows SH-4 ✓   │
└─────────────────┘         └─────────────────┘
        │                           │
        └─────── combine? ──────────┘
                    │
                    ▼
          ┌─────────────────┐
          │     gccgo       │
          │                 │
          │  Knows Go ✓     │
          │  Knows SH-4 ✓   │
          └─────────────────┘
```

**gccgo** is a Go frontend for GCC. It reads Go source code, performs type checking, then hands everything to GCC's backend. GCC handles the hard part—generating SH-4 machine code.

We get Go compilation for the Dreamcast "for free." Our job is to provide the **runtime library**.

### What is a Runtime?

A **runtime** is a library of functions that a compiled program calls during execution. It handles things the compiler can't (or shouldn't) generate inline: memory allocation, garbage collection, goroutine scheduling, panic handling, and more.

Why do languages use this pattern? **Portability.** The compiler translates your source code into machine instructions, but those instructions need to interact with the operating system or hardware. By separating "language translation" from "platform interaction," you can:

1. **Reuse the compiler** — gccgo already knows Go. We don't touch it.
2. **Swap the runtime** — We write a Dreamcast-specific runtime. The same compiler now works on a new platform.

This is how Go supports Linux, Windows, macOS, and now Dreamcast—same language, same compiler frontend, different runtimes.

Other languages use similar patterns:
- **C** has startup code (`crt0`) and `libc` for system calls
- **C++** adds exception handling (`libgcc`) and the standard library (`libstdc++`)
- **Rust** has a minimal runtime embedded in `libstd`
- **Java** has the JVM—a full runtime with GC, JIT, and class loading
- **Python** has `libpython`—the interpreter itself

The difference is scope. C's runtime is small—just system call wrappers. Go's runtime is large—it includes a garbage collector, scheduler, and channel implementation. That's why porting Go is harder than porting C, but the principle is identical.

---

## Code with Holes

Here's the key insight of this entire book. When you compile Go code, **the compiler doesn't include everything**.

```go
func main() {
    s := make([]int, 10)
    m := make(map[string]int)
    go doSomething()
}
```

What does `make([]int, 10)` actually do? It needs to allocate memory, initialize the slice header, and return it. Does the compiler generate all that code inline?

**No.** It generates function calls instead:

```
Your Go code              What the compiler emits
─────────────             ──────────────────────

make([]int, 10)       →   CALL runtime.makeslice
make(map[string]int)  →   CALL runtime.makemap  
go doSomething()      →   CALL runtime.newproc
```

The compiled object file is full of these calls. But the *implementations* aren't there:

```
┌─────────────────────────────────────────────────────┐
│                                                     │
│                  main.o (your compiled code)        │
│                                                     │
│    ┌─────┐  ┌─────┐  ┌─────┐  ┌─────┐  ┌─────┐      │
│    │HOLE │  │HOLE │  │HOLE │  │HOLE │  │HOLE │      │
│    └─────┘  └─────┘  └─────┘  └─────┘  └─────┘      │
│    runtime  runtime  runtime  runtime  runtime      │
│    .make    .make    .make    .new     .defer       │
│    slice    map      chan     proc     proc         │
│                                                     │
└─────────────────────────────────────────────────────┘
```

These are **unresolved symbols**. The object file knows it needs to call `runtime.makeslice`, but doesn't know where that function is.

**Who fills in the holes? That's us. That's `libgodc`.**

---

## Filling the Holes

Our job is to provide implementations. When the linker combines your code with our library, every hole gets filled:

```
BEFORE LINKING:
═══════════════

┌──────────────────┐          ┌──────────────────┐
│    main.o        │          │   libgodc.a      │
│                  │          │                  │
│  HOLE: runtime.  │          │  runtime.        │
│        makeslice │          │  makeslice ──────┼──→ actual code!
│                  │          │                  │
│  HOLE: runtime.  │          │  runtime.        │
│        newproc   │          │  newproc ────────┼──→ actual code!
└──────────────────┘          └──────────────────┘


AFTER LINKING:
══════════════

┌─────────────────────────────────────────────────────┐
│                    game.elf                         │
│                                                     │
│    call runtime.makeslice ───→ [makeslice code]     │
│    call runtime.newproc ─────→ [newproc code]       │
│                                                     │
│    No more holes! Ready to run.                     │
└─────────────────────────────────────────────────────┘
```

---

## The Symbol Problem

There's a wrinkle. Go uses dots in names: `runtime.makeslice`.

But dots are illegal in C identifiers:

```c
void runtime.makeslice() { }  // SYNTAX ERROR!
```

How do we write a C function with a dot in its name?

### The `__asm__` Trick

GCC lets you specify the symbol name separately:

```c
// C identifier uses underscore, but symbol has a dot
void *runtime_makeslice(void *type, int len, int cap)
    __asm__("runtime.makeslice");

void *runtime_makeslice(void *type, int len, int cap) {
    // implementation
}
```

```
┌────────────────────────────────────────────────────────┐
│                                                        │
│   In C code:         →    In object file:              │
│                                                        │
│   runtime_makeslice()     runtime.makeslice            │
│   (underscore)            (dot)                        │
│                                                        │
│   Go calls runtime.makeslice, linker finds it,         │
│   Go never knows it was written in C.                  │
│                                                        │
└────────────────────────────────────────────────────────┘
```

Every runtime function in `libgodc` uses this pattern.

---

## Symbols vs. Signatures

Two things must match between caller and callee:

**1. The Symbol (the name)**: Get it wrong, the linker complains loudly.

**2. The Signature (the shape)**: What arguments, what order, what return values.

The compiler has already decided how to call `runtime.makeslice`:

```
Register r4:  pointer to type descriptor
Register r5:  length
Register r6:  capacity

Return value in r0
```

If our implementation expects arguments in different registers:

```
What compiler sends:        What our code expects:
────────────────────        ──────────────────────

  r4 = type pointer           r4 = length        ← WRONG!
  r5 = length                 r5 = capacity      ← WRONG!
```

**The linker won't catch this.** Symbol names match, so it happily connects them. The mismatch only shows up at runtime as mysterious crashes.

```
┌─────────────────────────────────────────────────────┐
│                                                     │
│   Symbol mismatch:        Signature mismatch:       │
│   ───────────────         ──────────────────        │
│   Linker error            Linker succeeds           │
│   Clear message           Runtime crash             │
│   Easy to fix             Hard to debug             │
│                                                     │
└─────────────────────────────────────────────────────┘
```

---

## The Calling Convention

When a function calls another function, they need to agree on how to pass data. This is the **calling convention**.

### SH-4 Register Usage

```
┌─────────────────────────────────────────────────────────────┐
│   SH-4 Register Usage                                       │
│                                                             │
│   r0      Return value / scratch                            │
│   r1      Return value (64-bit) / scratch                   │
│   r2-r3   Scratch                                           │
│   ─────────────────────────────────────────────             │
│   r4      1st argument                                      │
│   r5      2nd argument                                      │
│   r6      3rd argument                                      │
│   r7      4th argument                                      │
│   ─────────────────────────────────────────────             │
│   r8-r13  Callee-saved (must preserve)                      │
│   r14     Frame pointer                                     │
│   r15     Stack pointer                                     │
└─────────────────────────────────────────────────────────────┘
```


**Why does this matter?** Most of the time, it doesn't—the compiler handles it. But understanding the calling convention helps when:

- **Debugging crashes**: Register dumps make sense when you know r4-r7 hold arguments
- **Writing `//extern` bindings**: You need to match what C functions expect
- **Reading the runtime assembly**: Context switching must save/restore the right registers (r8-r14 are callee-saved, so the *callee* must preserve them)

### Multiple Return Values

Go functions can return multiple values. C can't. `gccgo` handles this by returning a struct:

```c
struct result {
    int quotient;
    int remainder;
};

struct result divmod(int a, int b) {
    return (struct result){ a / b, a % b };
}
```

Small structs fit in r0-r1. When implementing runtime functions that return multiple values, we must match exactly what `gccgo` expects.

---

## Reading CPU Registers

Sometimes we need to know register values directly:

```c
// This variable IS register r15
register uintptr_t sp asm("r15");

printf("Stack pointer: 0x%08x\n", sp);
```

This isn't a copy—`sp` *is* the register. We use this for:

- Stack bounds checking
- Context switching (saving/restoring goroutine state)
- Debugging (dump registers on crash)

---

## Inline Assembly

Sometimes C can't express what we need. Here are real examples from libgodc:

```c
// Prefetch - hint CPU to load cache line (gc_copy.c)
#define GC_PREFETCH(addr) __asm__ volatile("pref @%0" : : "r"(addr))

// Read the stack pointer (gc_copy.c)
void *sp;
__asm__ volatile("mov r15, %0" : "=r"(sp));

// Read/write status register (scheduler.c)
__asm__ volatile("stc sr, %0" : "=r"(sr));  // read
__asm__ volatile("ldc %0, sr" : : "r"(sr)); // write

// Memory barrier - prevent compiler reordering (runtime.h)
#define CONTEXT_SWITCH_BARRIER() __asm__ volatile("" ::: "memory")
```go

We use assembly for:
- **Prefetching** (hint cache to load data we'll need soon)
- **Context switching** (save/restore all registers—see `runtime_sh4_minimal.S`)
- **Reading special registers** (stack pointer, status register)
- **Memory barriers** (ensure memory operations complete before continuing)

Don't use it for anything you can do in C. KOS handles cache flush/invalidate via `dcache_flush_range()`.

---

## Type Descriptors

When you define a Go type, the compiler generates a **type descriptor**. Here are the key fields (the full struct has 12 fields, 36 bytes):

```c
struct __go_type_descriptor {
    uintptr_t __size;        // Size of an instance
    uintptr_t __ptrdata;     // Bytes containing pointers
    uint32_t  __hash;        // Hash for type comparison
    uint8_t   __code;        // Kind (int, string, struct...)
    const uint8_t *__gcdata; // GC bitmap: which words are pointers
    // ... plus alignment, equality function, reflection string, etc.
};
```

For this Go type:

```go
type Point struct {
    X, Y int
    Name *string
}
```

The compiler generates:

```
┌─────────────────────────────────────────────────────────────┐
│   Type descriptor for Point:                                │
│                                                             │
│   __size:    12 bytes  (int + int + pointer)                │
│   __ptrdata: 12 bytes  (all 3 words may contain pointers)   │
│   __code:    STRUCT                                         │
│   __gcdata:  bit-packed bitmap (1 bit per word)             │
│                                                             │
│   Word 0 (X):    int, not a pointer  → bit 0 = 0            │
│   Word 1 (Y):    int, not a pointer  → bit 1 = 0            │
│   Word 2 (Name): pointer             → bit 2 = 1            │
│                                                             │
│   __gcdata[0] = 0b00000100 = 0x04                           │
│                                                             │
│   GC reads: gcdata[word/8] & (1 << (word%8))                │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

The garbage collector uses `__gcdata` to know which fields to scan. The bitmap is bit-packed: one bit per pointer-sized word. Without it, the GC would have to guess which values are pointers.

---

## The Build Process

```
══════════════════════════════════════════════════════════════
                    THE BUILD PIPELINE
══════════════════════════════════════════════════════════════

ONCE (building libgodc):
────────────────────────

  gc_runtime.c ─┐
  chan.c ───────┼──→ sh-elf-gcc ──→ *.o ──→ ar ──→ libgodc.a
  scheduler.c ──┤
  map.c ────────┘


EVERY TIME (building your game):
────────────────────────────────

  main.go ──→ sh-elf-gccgo ──→ main.o (with holes)
                                   │
                                   ▼
  main.o + libgodc.a + libkallisti.a ──→ sh-elf-ld ──→ game.elf

══════════════════════════════════════════════════════════════
```

The linker doesn't care what language produced the code. It just matches symbol names.

---

## Why C, Not Go?

`libgodc` is written in C (specifically, C11 with GNU extensions).

**The Bootstrap Problem**: To compile Go, you need a Go runtime. To get a Go runtime, you need to compile Go. Chicken, meet egg.

By writing the runtime in C, we sidestep the problem. The C compiler doesn't need anything from Go.

Also, KallistiOS is written in C, so we can directly call its functions.

---

## What Runs Before main()?

Your Go `main()` isn't the first thing that runs. `libgodcbegin.a` provides the C `main()` (in `go-main.c`) that sets everything up:

```
Dreamcast powers on
        │
        ▼
KallistiOS boots
        │
        ▼
C main() [go-main.c]
        │
        ├──→ runtime_args()              Save argc/argv
        ├──→ runtime_init()
        │       ├──→ gc_init()           Set up garbage collector
        │       ├──→ map_init()          Initialize map subsystem
        │       ├──→ sudog_pool_init()   Pre-allocate channel waiters
        │       ├──→ stack_pool_preallocate()  Pre-allocate goroutine stacks
        │       ├──→ proc_init()         Set up scheduler (tls_init, g0)
        │       └──→ panic_init()        Set up panic/recover
        │
        ├──→ __go_go(main_wrapper)       Create goroutine for main.main
        │
        └──→ scheduler_run_loop()        Start scheduler
                    │
                    ▼
            YOUR CODE RUNS HERE
```

