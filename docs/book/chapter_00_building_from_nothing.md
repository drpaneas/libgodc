# Building From Nothing

## The Real Starting Point

Most documentation starts *after* the hard part. "Here's the GC" assumes you know you need one. "Here's how goroutines work" assumes you figured out the symbol names.

Let's go back to the real beginning:

```go
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   DAY 0: THE SITUATION                                      │
│                                                             │
│   You have:                                                 │
│   • sh-elf-gccgo (Go compiler for SH-4)                     │
│   • KallistiOS (Dreamcast SDK)                              │
│   • A simple Go program: println("Hello, Dreamcast!")       │
│                                                             │
│   You try to compile it. What happens?                      │
│                                                             │
│   $ sh-elf-gccgo -c hello.go                                │
│   $ sh-elf-gcc hello.o -o hello.elf                         │
│                                                             │
│   LINKER ERRORS. Hundreds of them.                          │
│                                                             │
│   undefined reference to `runtime.printstring'              │
│   undefined reference to `runtime.printnl'                  │
│   undefined reference to `__go_runtime_error'               │
│   undefined reference to `runtime.newobject'                │
│   ...                                                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

Those undefined references are **the holes** we discussed in Chapter 2. The compiler generated calls to runtime functions that don't exist.

**Your job: Provide implementations for every one of them.**

---

## Part 1: The Discovery Process

### How Do You Know What gccgo Expects?

This is the question nobody answers. Where is it documented? What's the ABI?

**Answer: It's not well-documented. You have to investigate.**

Here's the process we used:

### Method 1: Read the Linker Errors

The linker tells you exactly what's missing:

```bash
sh-elf-gccgo -c myprogram.go -o myprogram.o
sh-elf-gcc myprogram.o -o myprogram.elf 2>&1 | grep "undefined reference"
```

You'll see output like:

```
undefined reference to `runtime.printstring'
undefined reference to `runtime.printnl'
undefined reference to `__go_runtime_error'
undefined reference to `runtime.newobject'
undefined reference to `runtime.makeslice'
```go

**Start here.** Each undefined symbol is a function you need to write.

### Method 2: Read the gccgo Source

The gccgo frontend lives in the GCC source tree. The key directories:

```
gcc/go/gofrontend/      ← The Go parser and type checker
libgo/runtime/          ← The reference runtime (for Linux)
libgo/go/               ← Go standard library
```go

When gccgo compiles `make([]int, 10)`, it emits a call to `runtime.makeslice`. To find the expected signature:

```bash
# In the GCC source tree
grep -r "makeslice" libgo/runtime/
```

You'll find the actual implementation. Study its parameters and return type.

### Method 3: Use nm on Object Files

Compile your Go code and inspect what symbols it references:

```bash
sh-elf-gccgo -c test.go -o test.o
sh-elf-nm test.o | grep " U "   # "U" = undefined (needs linking)
```

This shows you every external symbol your code needs.

### Method 4: Disassemble and Trace

When things don't work, disassemble:

```bash
sh-elf-objdump -d test.o | less
```go

Look at how functions are called. What registers hold arguments? What's expected in return registers?

### The Symbol Naming Convention

gccgo uses a specific naming scheme:

| Go Concept | Symbol Name |
|------------|-------------|
| `runtime.X` | `runtime.X` (literal dot) |
| `main.foo` | `main.foo` |
| Method on type T | `T.MethodName` |
| Interface method | Complex mangling |

Since C can't have dots in identifiers, we use the `__asm__` trick:

```c
void runtime_printstring(String s) __asm__("runtime.printstring");

void runtime_printstring(String s) {
    // Implementation
}
```

---

## Part 2: The Build Order

You can't build everything at once. There are dependencies:

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   DEPENDENCY GRAPH                                          │
│                                                             │
│                       ┌─────────┐                           │
│                       │ println │                           │
│                       └────┬────┘                           │
│                            │ needs                          │
│                       ┌────▼────┐                           │
│                       │ strings │                           │
│                       └────┬────┘                           │
│                            │ needs                          │
│                       ┌────▼────┐                           │
│                       │ memory  │                           │
│                       │ alloc   │                           │
│                       └────┬────┘                           │
│                            │ needs                          │
│                       ┌────▼────┐                           │
│                       │  heap   │                           │
│                       │  init   │                           │
│                       └─────────┘                           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```go

### Milestone 1: Hello World

**Goal:** Print a string. No GC, no goroutines, nothing fancy.

**What you need:**

1. **Memory allocator** — Even `println` allocates internally
2. **Print functions** — `runtime.printstring`, `runtime.printnl`, `runtime.printint`
3. **String support** — Go strings are `{pointer, length}` structs
4. **Entry point** — Something to call `main.main`

**The minimal files:**

```text
runtime/
├── go-main.c           # Entry point, calls main.main
├── malloc_dreamcast.c  # Basic malloc wrapper
├── go-print.c          # Print functions
└── runtime.h           # Common definitions
```

**Test:**

```go
package main

func main() {
    println("Hello, Dreamcast!")
}
```

If this prints, you have a foundation.

### Milestone 2: Basic Types

**Goal:** Slices, arrays, basic type operations.

**What you need:**

1. **makeslice** — Create slices
2. **growslice** — Append to slices
3. **Type descriptors** — Compiler generates these, you need to understand them
4. **Memory operations** — `memcpy`, `memset`, `memmove` wrappers

**New files:**

```
runtime/
├── slice_dreamcast.c   # Slice operations
├── string_dreamcast.c  # String operations
└── type_descriptors.h  # Type metadata structures
```

**Test:**

```go
package main

func main() {
    s := make([]int, 5)
    s[0] = 42
    println(s[0])
}
```go

### Milestone 3: Panic and Defer

**Goal:** Error handling works.

**Why before GC?** Because GC needs defer for cleanup. And panic is simpler than GC.

**What you need:**

1. **Defer chain** — Linked list of deferred calls per goroutine
2. **Panic mechanism** — setjmp/longjmp based
3. **Recover** — Check if in deferred function

**Test:**

```go
package main

func main() {
    defer println("world")
    println("hello")
}
// Should print: hello, then world
```go

### Milestone 4: Maps

**Goal:** Hash tables work.

**The problem:** Go maps have complex semantics:
- Iteration order is randomized
- Growing rehashes everything
- Keys can be any comparable type

**What you need:**

1. **Hash function** — For each key type
2. **Bucket structure** — Go uses a specific layout
3. **makemap, mapaccess, mapassign, mapdelete** — Core operations
4. **Map iteration** — Complex state machine

**Lesson learned:** Map iteration state is stored in a `hiter` struct. If you get this wrong, `range` loops break mysteriously.

### Milestone 5: Garbage Collection

**Goal:** Automatic memory management.

**Design decision:** We chose semi-space copying GC because:
- No fragmentation
- Simple implementation
- Predictable pause times (though not short)

**What you need:**

1. **Root scanning** — Find all pointers on stack and in globals
2. **Object copying** — Move live objects to new space
3. **Pointer updating** — Fix all references
4. **Type bitmaps** — Know which words are pointers

**The hard part:** Knowing which stack slots are pointers. gccgo generates `__gcdata` bitmaps for types, but stack scanning is conservative.

### Milestone 6: Goroutines

**What you need:**

1. **G struct** — Goroutine state
2. **Stack allocation** — Each goroutine needs its own stack
3. **Context switching** — Save/restore CPU registers (assembly!)
4. **Scheduler** — Pick which goroutine runs next
5. **Run queue** — List of runnable goroutines

**The assembly is unavoidable.** You must write `swapcontext` in SH-4 assembly. There's no way around it. You see, you have to do context switching in the actual registers, but C doesn't give you access to talk to them. The compiler manages the registers behind your back.

```asm
! Save current context
mov.l   r8, @-r4
mov.l   r9, @-r4
! ... save all callee-saved registers ...

! Load new context
mov.l   @r5+, r8
mov.l   @r5+, r9
! ... restore all registers ...

rts
```

### Milestone 7: Channels

**Goal:** Goroutines can communicate.

**Channels require:**
- Wait queues (goroutines blocked on send/receive)
- Buffered storage (ring buffer)
- Select statement (waiting on multiple channels)

**The "3 days of debugging" commit touched channels.** The issue was usually:
- Waking the wrong goroutine
- Corrupting state during concurrent access
- Stack misalignment after context switch

---

## Part 3: Resources You'll Need

### Essential Reading

1. **gccgo source code** — `gcc/go/gofrontend/` and `libgo/runtime/`
2. **Go runtime source** — `$GOROOT/src/runtime/` (different ABI, but same concepts)
3. **SH-4 programming manual** — For assembly and ABI
4. **KallistiOS documentation** — For Dreamcast specifics

### Tools

| Tool | Purpose |
|------|---------|
| `sh-elf-nm` | List symbols in object files |
| `sh-elf-objdump` | Disassemble code |
| `sh-elf-addr2line` | Convert addresses to line numbers |
| `dc-tool-ip` | Upload and run on Dreamcast |
| `lxdream` | Dreamcast emulator (for faster iteration) |

### The Checklist Mentality

Before each phase, write down:
1. What symbols must I implement?
2. What's the expected signature?
3. How will I test it?

After each phase:
1. Did all tests pass?
2. What surprised me?
3. What would I do differently?

---

The journey from nothing to a working Go runtime is not easy. But it is *achievable*. Every problem has a solution. Every bug can be found. Every undefined symbol can be implemented.

You now have the map. Go build it.

