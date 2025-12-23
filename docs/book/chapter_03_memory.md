# Chapter 3: Memory Management

## The Problem with Memory

In C, you're the janitor:

```c
char *name = malloc(100);
strcpy(name, "Mario");
free(name);  // Forget this? Memory leak.
             // Do it twice? Crash.
```

It's like putting your cup of coffee to your desk every morning and never putting back. Monday is fine, but Friday looks like a pile of
empty coffee mugs all over the place. 

Go says: "I'll handle the cleaning the trash and your coffee mugs."

```go
player := &Player{name: "Mario"} // struct allocation (heap)
enemies := make([]Enemy, 10) // slice allocation (heap)
scores := make(map[string]int) // map allocation (heap)
// That's it. Go cleans up automatically when you're done with them.
```

### Stack vs Heap: Where Does Memory Live?

If you're coming from Python or JavaScript, you might never have thought about *where* your variables live. In those languages, everything "just works" in the sense where you create objects, use them, and the runtime cleans up. But programs actually use two different regions of RAM: the **stack** and the **heap**. Both are in main memory, but they're managed very differently.

```go
func calculate() int {
    x := 42           // stack: lives only during this function call
    y := x * 2        // stack: same, gone when function returns
    return y          // value is copied out, then x and y disappear
}

func createPlayer() *Player {
    p := &Player{name: "Mario"}   // heap: we're returning a pointer
    return p                      // p (the pointer) disappears, but the
                                  // Player data survives on the heap
}
```go

The **stack** is memory that belongs to the current function call. When the function returns, that memory is immediately reclaimed—no cleanup needed, no garbage collector involved. But the data is gone forever.

The **heap** is memory that persists beyond the function that created it. When you take the address of something (`&Player{...}`), return a pointer, or use `make()` for slices/maps, Go allocates on the heap. That memory sticks around until the garbage collector determines nothing references it anymore.

There's also the **data segment** where global variables live. These are allocated once when the program starts and exist until the program exits—no cleanup, no GC, they just persist for the program's entire lifetime.

```go
var highScore int       // data segment - exists from start to end

func main() {
    x := 42             // stack - gone when main() returns
    p := &Player{}      // heap - GC cleans up when unreferenced
    highScore = 9999    // modifying global, not allocating
}
```

On Dreamcast, there are additional memory regions you'll encounter:

| Region | Size | Contains |
|--------|------|----------|
| **Code** | varies | Your compiled program (read-only instructions) |
| **Data/BSS** | varies | Global variables |
| **Stack** | 64 KB per goroutine | Local variables, function calls |
| **Heap** | ~4 MB (2 MB usable) | GC-managed allocations |
| **VRAM** | 8 MB total | Textures, framebuffer (via PVR functions) |
| **Sound RAM** | 2 MB | Audio samples (via sound functions) |

**VRAM and Sound RAM are physically separate chips**—they can't corrupt main RAM or each other. If you run out of VRAM, `PvrMemMalloc()` returns 0. If you don't check and try to use that zero pointer, your program crashes. Use `PvrMemAvailable()` to check how much VRAM remains (the framebuffer takes some of the 8 MB, so you won't have all of it for textures).

When your game ends (power off or reset), all memory is simply gone—the "cleanup" is turning off the console.

```go
func example() {
    // STACK - temporary, fast, automatic cleanup:
    count := 10
    sum := 0.0
    flag := true

    // HEAP - persists, needs GC to clean up:
    player := &Player{}           // pointer escapes? heap
    enemies := make([]Enemy, 5)   // slices go to heap
    scores := make(map[string]int) // maps always heap
}
```

The compiler decides where each variable lives through **escape analysis**: if the data could be used after the function returns (passed around, stored somewhere, returned), it goes to the heap. Otherwise, it stays on the stack.

The **garbage collector** (GC) finds stuff you're not using anymore and reclaims the memory. But here's the catch—it takes time to run.

---

## How Allocation Works

When you create something in Go, where does the memory come from?

We use **bump allocation**. Think of it like a notepad:

```
┌─────────────────────────────────────────────────────┐
│ Mario │ Luigi │ Peach │                             │
└─────────────────────────────────────────────────────┘
                        ↑
                     You are here
                   (next free spot)
```

To allocate: just write at the current spot and move the marker.

```
┌─────────────────────────────────────────────────────┐
│ Mario │ Luigi │ Peach │ Toad │                      │
└─────────────────────────────────────────────────────┘
                               ↑
                            Moved!
```

That's it! Just move a pointer. **Way faster than malloc.**

### Verifying Allocations: A Hands-On Example

Embedded developers are used to inspecting memory directly. Here's how you can see these allocations in action:

```go
package main

import "unsafe"

type Player struct {
    X, Y  float32
    Score int32
}

//go:noinline
func allocOnHeap() *Player {
    return &Player{X: 10, Y: 20, Score: 100}
}

func main() {
    // Stack allocation
    var local Player
    stackAddr := uintptr(unsafe.Pointer(&local))
    println("Stack allocation at:", stackAddr)

    // Heap allocation
    p := allocOnHeap()
    heapAddr := uintptr(unsafe.Pointer(p))
    println("Heap allocation at:", heapAddr)

    // Multiple heap allocations - watch the bump pointer move
    for i := 0; i < 5; i++ {
        obj := allocOnHeap()
        addr := uintptr(unsafe.Pointer(obj))
        println("  Player", i, "at:", addr)
    }
}
```

**Actual output from Dreamcast hardware** (from `tests/test_alloc_inspect.elf`):
```
Stack allocation:
  Address (hex):     0x8c494cc4

Heap allocation:
  Address (hex):     0x8c084b00

Allocating 5 Player structs consecutively:
  Player 0 at: 0x8c084b50
  Player 1 at: 0x8c084b68  (+ 24 bytes)
  Player 2 at: 0x8c084b80  (+ 24 bytes)
  Player 3 at: 0x8c084b98  (+ 24 bytes)
  Player 4 at: 0x8c084bb0  (+ 24 bytes)

Global variable at:  0x8c05ecc0
  → Data segment (matches .data section start)
```

Notice the heap addresses increment by 24 bytes each time—that's the 12-byte `Player` struct plus the 8-byte GC header, rounded up to 8-byte alignment. The bump pointer just keeps moving forward.

**Using GDB to inspect:**
```bash
# Start dc-tool with GDB server enabled
$ dc-tool-ip -t 192.168.x.x -g -x your_game.elf

# In another terminal, connect GDB
$ sh-elf-gdb your_game.elf
(gdb) target remote :2159

# Set breakpoint and run
(gdb) break main.main
(gdb) continue

# Examine heap memory (address from test output)
(gdb) x/32x 0x8c084b00    # Dump heap region
(gdb) info registers r15  # Stack pointer (SP)

# View GC heap structure
(gdb) p gc_heap           # Print GC heap state
(gdb) p gc_heap.alloc_ptr # Current bump pointer
```

**Memory layout from real hardware (16 MB RAM at 0x8c000000-0x8d000000):**
```
0x8c000000 ┌─────────────────────────────────────┐
           │ KOS kernel and system data          │
0x8c010000 ├─────────────────────────────────────┤
           │ .text (your compiled code)          │ ← Binary starts here
0x8c052aa0 ├─────────────────────────────────────┤
           │ .rodata (read-only data, strings)   │
0x8c05ecc0 ├─────────────────────────────────────┤
           │ .data (global variables)            │ ← Global at 0x8c05ecc0
0x8c0622ac ├─────────────────────────────────────┤
           │ Heap (KOS malloc)                   │
           │   - GC semi-spaces                  │ ← Heap alloc at 0x8c084b00
           │   - KOS thread stacks               │ ← Stack var at 0x8c494cc4
           │   - Other malloc allocations        │
           │                                     │
0x8d000000 └─────────────────────────────────────┘
```

Note: KOS manages thread stacks via `malloc`, so both heap allocations and stack memory come from the same pool. The addresses above are from running `test_alloc_inspect.elf` on real hardware.

But wait...! We never erase anything. Eventually we run out of pages. Yikes!

---

## Why Two Spaces? (Semi-Space Collection)

The bump allocator has a problem: it can only allocate, never free individual objects. When the space fills up, we need a way to reclaim garbage.

**Why not free objects in place?** Because it creates fragmentation:

```
┌──────────────────────────────────────────────────────┐
│ Player │ FREE │ Enemy │ FREE │ FREE │ Bullet │ FREE  │
└──────────────────────────────────────────────────────┘
          ↑       can't fit a 3-slot object here
```

You end up with "free" holes everywhere. A 3-slot object might not fit even though there's enough total free space.

**The solution: copy to a second space.** Instead of freeing in place:

1. Allocate a second space of equal size
2. When the first space fills, scan for live objects (objects still referenced)
3. Copy only live objects to the second space
4. The first space is now 100% garbage—reset the bump pointer to the start

```
BEFORE (Space A full):           AFTER (Space B active):
┌────────────────────────┐       ┌────────────────────────┐
│ Player │ xxx │ Enemy │ │  →    │ Player │ Enemy │ Bullet│
│ xxx │ Bullet │ xxx │   │       │                        │
└────────────────────────┘       └────────────────────────┘
 (xxx = garbage)                  (compacted, no gaps!)
```

This **copying collection** solves two problems at once:
- **Garbage is reclaimed**: everything left in Space A is garbage
- **Memory is compacted**: no fragmentation in Space B

### How Copying Works: Cheney's Algorithm

The copying process uses an elegant algorithm invented by C.J. Cheney in 1970. It needs only two pointers and no recursion:

```
TO-SPACE:
┌────────────────────────────────────────────────────────┐
│ Player │ Enemy │ Bullet │                              │
└────────────────────────────────────────────────────────┘
         ↑                 ↑
       SCAN              ALLOC
```

1. **Start with roots** (global variables, stack references, CPU registers)
   
   Why roots? The GC needs to know which objects are still in use. It can't ask the running program—the program is paused. The only way to determine if an object is "live" is to check: **can any code reach it?** Roots are the starting points—references the program definitely has access to. If an object isn't reachable from any root (directly or through a chain of pointers), no code can ever access it again. It's garbage.

2. **Copy each root object** to to-space at the ALLOC position, then move ALLOC forward by the object's size (this is the same bump allocation from earlier—just `alloc_ptr += size`)

3. **Scan copied objects** (starting at SCAN pointer) for pointers to other objects
   
   "Scan" doesn't mean checking every byte—that would be slow and error-prone. Each object has type information (the `__gcdata` bitmap from its type descriptor) that tells the GC exactly which fields are pointers. The GC only checks those fields.

4. **If a referenced object hasn't been copied**, copy it to to-space

5. **Update the pointer** to point to the new location

6. **Repeat until SCAN catches up with ALLOC**—all live objects are now copied

The clever part: when you copy an object, you leave a **forwarding pointer** in the old location. If another reference points to that same object, you find the forwarding pointer and update the reference without copying again.

```c
// Simplified from runtime/gc_copy.c
void *gc_copy_object(void *old_ptr) {
    gc_header_t *header = gc_get_header(old_ptr);
    
    // Already copied? Return the forwarding address
    if (GC_HEADER_IS_FORWARDED(header))
        return GC_HEADER_GET_FORWARD(header);
    
    size_t obj_size = GC_HEADER_GET_SIZE(header);
    
    // Copy to to-space at current alloc_ptr
    gc_header_t *new_header = (gc_header_t *)gc_heap.alloc_ptr;
    memcpy(new_header, header, obj_size);
    gc_heap.alloc_ptr += obj_size;
    
    void *new_ptr = gc_get_user_ptr(new_header);
    
    // Leave forwarding pointer in old location
    GC_HEADER_SET_FORWARD(header, new_ptr);
    
    return new_ptr;
}
```

**Why this algorithm is elegant:**
- **O(live objects)** time—dead objects aren't even touched
- **No recursion**—just two pointers chasing each other
- **Single pass**—scan and copy happen together
- **Compaction is free**—objects naturally pack together

**The trade-off:** 50% of heap is always reserved for the copy destination.

---

## The 50% Memory Cost

You may have noticed the trade-off mentioned earlier: one space is always reserved for copying. That means half your heap is "unusable" at any given time.

```
┌─────────────────────────────────────────────────────┐
│        4 MB total GC heap                           │
│  ┌──────────────────┬──────────────────┐            │
│  │   Space A        │   Space B        │            │
│  │   2 MB           │   2 MB           │            │
│  │   (active)       │   (copy target)  │            │
│  └──────────────────┴──────────────────┘            │
│                                                     │
│  Usable at any time: 2 MB                           │
└─────────────────────────────────────────────────────┘
```

Why accept this 50% cost? Because you get:

- **No fragmentation**: Cheney's algorithm compacts automatically
- **O(1) allocation**: just bump `alloc_ptr`, no free-list search
- **O(live objects) collection**: dead objects aren't even touched
- **Simple implementation**: fewer bugs in the runtime
- **Cache-friendly**: live objects end up packed together

It's a deliberate trade-off: memory for speed and simplicity. On a 16 MB system where you're also using VRAM and Sound RAM for assets, 2 MB of usable GC heap is often sufficient.

> **Customizing heap size:** The default is `GC_SEMISPACE_SIZE_KB=2048` (2 MB per space, 4 MB total). To change it, edit `runtime/godc_config.h` or rebuild libgodc with `make CFLAGS="-DGC_SEMISPACE_SIZE_KB=1024"` for 1 MB usable, leaving more RAM for game assets.

---

## The Freeze

Here's the bad news. When the GC runs, your game **stops**.

```
Timeline:
────────────────────────────────────────────────────────
Game:   ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓████████████▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓
                        ↑            ↑
                      GC starts    GC ends
                        
                     "stop-the-world"
```

All Go code freezes, game logic, physics, input handling. No goroutines run during collection. (Music keeps playing though—the AICA sound processor runs independently of the SH-4 CPU.)

How long does this take? Let's find out with real numbers.

---

## Real Benchmark Results

Benchmarks from actual Dreamcast hardware (from `tests/bench_architecture.elf`), verified December 2025:

```
┌─────────────────────────────────────────────────────┐
│  SCENARIO                   GC PAUSE                │
├─────────────────────────────────────────────────────┤
│  Large objects (≥128 KB)    ~73 μs   (bypass GC)    │
│  64 KB live data            ~2.2 ms                 │
│  32 KB live data            ~6.2 ms                 │
└─────────────────────────────────────────────────────┘
```go

GC pause **scales with the number of objects**, not just total size. Many small objects (32 KB scenario) require more traversal and copying than fewer large objects.

**Key insight:** Allocations ≥64 KB bypass the GC heap entirely (go straight to `malloc`), which is why the "large objects" scenario shows only ~73 μs—that's just the baseline GC setup cost with nothing to copy.

> See the [Glossary](../appendix/glossary.md#performance-numbers) for a complete reference of all benchmark numbers.

---

## What This Means for Games

Let's do the math with real data (assuming ~128KB live data = ~6ms pause):

```text
┌─────────────────────────────────────────────────────┐
│  TARGET FPS    FRAME BUDGET    GC PAUSE (~6ms)      │
├─────────────────────────────────────────────────────┤
│  60 FPS        16.7 ms         ~1/3 frame stutter   │
│  30 FPS        33.3 ms         barely noticeable    │
│  20 FPS        50 ms           unnoticeable         │
└─────────────────────────────────────────────────────┘
```

At 60 FPS, a 6ms GC pause is **noticeable but brief**. Keep live data small, and pauses stay short.

---

## Big Objects Get Special Treatment

Here's a surprise: big allocations skip the GC entirely!

```go
small := make([]byte, 1000)      // → GC heap
big := make([]byte, 100*1024)    // → malloc (bypasses GC!)
```

The threshold is **64 KB**:

```
┌─────────────────────────────────────────────────────┐
│  SIZE           WHERE IT GOES     FREED BY          │
├─────────────────────────────────────────────────────┤
│  < 64 KB        GC heap           GC (automatic)    │
│  ≥ 64 KB        malloc            NEVER! (manual)   │
└─────────────────────────────────────────────────────┘
```

Wait, **never**? That's right. Big objects are **never automatically freed**.

Why? Copying a 256 KB texture during GC would be too slow. So we skip it entirely. But that means you're responsible for freeing it.

```
      ⚠️  WARNING  ⚠️
      
      Large objects (≥64 KB) are NEVER 
      automatically freed by the GC!
      
      This is a memory leak unless you
      call freeExternal() manually (see next section).
```

### When Is This OK?

**Fine:** Loading a texture at game start. It lives forever anyway.

**Problem:** Loading new textures every level without freeing old ones.

---

## Freeing Big Objects

Here's how to clean up big allocations:

```go
import "unsafe"

//extern _runtime.FreeExternal
func freeExternal(ptr unsafe.Pointer)

// Load a big texture
texture := make([]byte, 256*1024)  // 256KB, bypasses GC

// Later, when done with it:
freeExternal(unsafe.Pointer(&texture[0]))
texture = nil  // Don't use it anymore!
```

The best time to do this? **Level transitions.**

```go
func LoadLevel(num int) {
    // Free old level's big stuff
    if oldTexture != nil {
        freeExternal(unsafe.Pointer(&oldTexture[0]))
        oldTexture = nil
    }
    
    // Load new level
    oldTexture = loadTexture(num)
    
    // Clean up small stuff too
    runtime.GC()
}
```

### EXERCISE

**3.3** You load a 128 KB texture each level. After 10 levels without calling `freeExternal()`, how much memory have you leaked?

---

## Making GC Hurt Less

Techniques to reduce GC impact, validated by real benchmarks from `tests/bench_gc_techniques.elf`.

### Technique 1: Pre-allocate Slices

**Benchmark result: 78% faster!**

Real numbers from Dreamcast:
- Growing slice: 72,027 ns/iteration
- Pre-allocated: 40,450 ns/iteration

```go
// SLOW: Slice grows, triggers multiple allocations
var items []int
for i := 0; i < 100; i++ {
    items = append(items, i)
}
```

**Why is this slow?** A slice in Go is three things: a pointer to data, a length, and a capacity. When you `append` beyond capacity, Go must:

1. Allocate a new, larger array (typically 2x the size)
2. Copy all existing elements to the new array
3. Abandon the old array (becomes garbage for GC to collect)

Here's what happens in memory when appending 5 items to an empty slice:

```
append #1:  Allocate [_], write item         → 1 alloc, 0 copies
append #2:  Full! Allocate [_,_], copy 1     → 2 allocs, 1 copy
append #3:  Full! Allocate [_,_,_,_], copy 2 → 3 allocs, 3 copies total
append #4:  Space available, just write      → 3 allocs, 3 copies total
append #5:  Full! Allocate [_,_,_,_,_,_,_,_], copy 4 → 4 allocs, 7 copies total
```

For 100 items, this triggers ~7 reallocations and copies ~200 elements total. Each abandoned array is garbage that fills the heap faster.

```
Memory timeline (growing slice):
┌─────────────────────────────────────────────────────┐
│ [1]  ← alloc #1 (abandoned)                         │
│ [1,2]  ← alloc #2 (abandoned)                       │
│ [1,2,3,_]  ← alloc #3 (abandoned)                   │
│ [1,2,3,4,5,_,_,_]  ← alloc #4 (abandoned)           │
│ [1,2,3,4,5,6,7,8,9,...]  ← alloc #5 (current)       │
│                                                     │
│ GC must eventually clean up allocs #1-#4!           │
└─────────────────────────────────────────────────────┘
```

**The fix:** If you know (or can estimate) how many items you'll need, pre-allocate:

```go
// FAST: Pre-allocate with known capacity
items := make([]int, 0, 100)  // length=0, capacity=100
for i := 0; i < 100; i++ {
    items = append(items, i)
}
```

```
Memory timeline (pre-allocated):
┌─────────────────────────────────────────────────────┐
│ [_,_,_,_,_,...100 slots...]  ← single allocation    │
│ [1,_,_,_,_,...] → [1,2,_,_,...] → [1,2,3,_,...]     │
│                                                     │
│ No copying. No garbage. Just fill in the blanks.    │
└─────────────────────────────────────────────────────┘
```

No growing. No copying. No garbage. **78% faster.**

**When to use:** Loading enemy spawns from a level file? You know the count. Parsing a protocol with a length header? Pre-allocate. Even a rough estimate (round up to next power of 2) beats growing from zero.

---

### Technique 2: Object Pools

**Important: Pools are NOT faster for allocation!**

Real numbers from Dreamcast:
- new() allocation: 201 ns/object
- Pool get/return: 1,450 ns/object (7x slower!)

This is counter-intuitive if you're coming from desktop Go or other languages. Let's understand why.

**Why is `new()` so fast?** Our bump allocator is essentially one operation:

```
new(Bullet):
┌───────────────────────────────────────────────────────┐
│ alloc_ptr → [████████ used █████|▓▓▓▓ free ▓▓▓▓▓]     │
│                                 ↑                     │
│                            alloc_ptr += sizeof(Bullet)│
│                                                       │
│ Total: 1 pointer increment. Done.                     │
└───────────────────────────────────────────────────────┘
```

That's it. No free lists to search. No size classes. No locking. Just bump the pointer forward. This is why 201 ns is achievable—it's maybe 40-50 CPU cycles.

**Why are pools slower?** Pool operations involve slice manipulation:

```
GetFromPool():
┌─────────────────────────────────────────────────────┐
│ 1. Check if len(pool) > 0      ← bounds check       │
│ 2. Read pool[len-1]            ← memory access      │
│ 3. pool = pool[:len-1]         ← slice header write │
│ 4. Return pointer              ← done               │
│                                                     │
│ ReturnToPool():                                     │
│ 1. Reset object fields         ← memory writes      │
│ 2. pool = append(pool, obj)    ← may grow slice!    │
│                                                     │
│ Total: ~7x more work than bump allocation           │
└─────────────────────────────────────────────────────┘
```

**So why use pools at all?** The trade-off isn't about allocation speed. It's about **when you pay the cost**:

```
WITHOUT POOL (100 bullets/frame):
─────────────────────────────────────────────────────
Frame 1:  new new new new... (100x)  │ 20 μs │ smooth
Frame 2:  new new new new... (100x)  │ 20 μs │ smooth
Frame 3:  new new new new... (100x)  │ 20 μs │ smooth
  ...
Frame 50: GC TRIGGERED!              │ 6 ms  │ ← STUTTER!
─────────────────────────────────────────────────────
                                     └─ 60 FPS target = 16.6 ms
                                        6 ms pause = 1/3 frame drop


WITH POOL (100 bullets/frame):
─────────────────────────────────────────────────────
Frame 1:  get get get... return...   │ 145 μs │ smooth
Frame 2:  get get get... return...   │ 145 μs │ smooth
Frame 3:  get get get... return...   │ 145 μs │ smooth
  ...
Frame 50: (no GC needed)             │ 145 μs │ still smooth!
─────────────────────────────────────────────────────
```

You're trading ~125 μs per frame for **no GC pauses**. For a bullet hell game, that's worth it.

**When to use pools:**
- High-frequency create/destroy (bullets, particles, audio events)
- Objects with predictable lifetimes (spawned and despawned together)
- When you need consistent frame times (no surprise stutters)

**When NOT to use pools:**
- Objects created once and kept (player, level geometry)
- Low churn rate (a few allocations per second)
- Prototype/debugging (just use `new()`, it's simpler)

**Simple pool implementation:**

```go
var pool []*Bullet

func GetBullet() *Bullet {
    if len(pool) > 0 {
        b := pool[len(pool)-1]
        pool = pool[:len(pool)-1]
        return b
    }
    return new(Bullet)  // Pool empty? Allocate fresh
}

func ReturnBullet(b *Bullet) {
    b.X, b.Y, b.Active = 0, 0, false  // Reset state!
    pool = append(pool, b)
}
```

**Pro tip:** Pre-populate the pool at game start to avoid any `new()` calls during gameplay:

```go
func InitBulletPool(size int) {
    pool = make([]*Bullet, size)
    for i := range pool {
        pool[i] = new(Bullet)
    }
}
```

Now `GetBullet()` never allocates during gameplay—predictable performance every frame.

---

### Technique 3: Trigger GC at Safe Times

**Benchmark: Manual GC takes ~35 μs with minimal live data**

The problem with automatic GC is **unpredictability**. You don't control when it runs. It just happens when the heap fills up. That might be during a boss fight.

**GC pause times from real benchmarks** (from `bench_gc_pause.elf`):

| Live Data | GC Pause | Impact at 60 FPS |
|-----------|----------|------------------|
| Minimal   | ~100 μs  | Unnoticeable     |
| 32 KB     | ~2 ms    | Minor stutter    |
| 128 KB    | ~6 ms    | 1/3 frame drop   |

The key insight: GC pause scales with **live data**, not garbage. If you trigger GC when live data is minimal (between levels, during menus), the pause is tiny.

**Uncontrolled vs Controlled GC:**

```
UNCONTROLLED (GC surprises you):
─────────────────────────────────────────────────────────────
│ Gameplay ││ Gameplay ││ Gameplay ││ GC! ││ Gameplay       │
│  smooth  ││  smooth  ││  smooth  ││6 ms!││  smooth        │
─────────────────────────────────────────────────────────────
                                      ↑
                                 Player notices!
                                 "Why did it stutter
                                  when I jumped?"


CONTROLLED (you choose when):
─────────────────────────────────────────────────────────────
│ Gameplay ││ Menu Opens ││ Gameplay ││ Level End ││ Next   │
│  smooth  ││ GC (35 μs) ││  smooth  ││ GC (35 μs)││Level   │
─────────────────────────────────────────────────────────────
              ↑                         ↑
         Player is reading         Victory animation
         menu anyway               playing anyway
```

**How to trigger GC manually:**

```go
//go:linkname forceGC runtime.GC
func forceGC()
```

**Best times to trigger GC** (player won't notice):

```go
func OnDialogueStart() {
    forceGC()  // Text appearing letter-by-letter anyway
}

func OnMenuOpen() {
    forceGC()  // Player is reading options
}

func OnLevelComplete() {
    forceGC()  // Victory fanfare playing, score tallying
}

func OnLoadingScreen() {
    forceGC()  // Already showing "Loading..."
}

func OnRoomTransition() {
    forceGC()  // Screen is fading to black
}

func OnCutsceneStart() {
    forceGC()  // Video/animation taking over
}
```

**Important caveats:**

1. **Don't trigger too often.** GC still takes time. Once per scene transition is reasonable. Once per frame defeats the purpose.

2. **This doesn't reduce garbage.** You're just choosing *when* to pay the cost. Combine with pre-allocation and pools to reduce *how much* garbage you create.

3. **Live data still matters.** If you have 128 KB of permanent game state, even manual GC takes ~6 ms. Keep live data lean.

```
Good: Trigger GC → level enemies/items are garbage → fast GC
Bad:  Trigger GC → 10,000 persistent objects → slow GC anyway
```

---

### Technique 4: Reuse Slices

**Benchmark: 5% faster** (13,200 ns → 12,500 ns)

Small gain per-call, but the real win is **less garbage over time**. Reset with `[:0]` instead of allocating new:

```go
// BAD: New allocation every frame
func ProcessFrame() {
    items := make([]int, 0, 100)  // ← garbage next frame
    // ...
}

// GOOD: Reuse backing array
var items = make([]int, 0, 100)  // Allocate once

func ProcessFrame() {
    items = items[:0]  // Reset length, keep capacity
    // ...
}
```

The `[:0]` trick keeps the backing array. Over 1000 frames: 1 allocation instead of 1000.

**Bonus pattern—shift without allocating:**
```go
// Creates new slice header:
queue = append(queue[1:], newItem)

// Reuses existing array:
copy(queue, queue[1:])
queue[len(queue)-1] = newItem
```

---

### Technique 5: Compact In-Place

When entities die, don't allocate a filtered slice. Compact the existing one:

```go
// BAD: Allocates new slice
alive := make([]*Enemy, 0)
for _, e := range enemies {
    if e.Active {
        alive = append(alive, e)  // ← garbage
    }
}
enemies = alive

// GOOD: Compact in place
n := 0
for _, e := range enemies {
    if e.Active {
        enemies[n] = e
        n++
    }
}
enemies = enemies[:n]  // Shrink, no allocation
```

Visual:
```
Before: [A, _, B, _, _, C]  (3 active, 3 dead)
         ↓ compact
After:  [A, B, C]           (same backing array, shorter length)
```

Classic game loop pattern: every frame, compact dead bullets/particles/enemies without touching the allocator.
