# Effective Dreamcast Go

A practical guide to writing efficient Go code for the Sega Dreamcast.

These patterns come from real debugging sessions with the libgodc runtime. Follow them to write games that run smooth at 60fps on the Dreamcast's 200MHz SH-4 processor with 16MB RAM.

## Memory Model

| Resource | Limit | Notes |
|----------|-------|-------|
| Total RAM | 16 MB | Shared with VRAM, sound, OS |
| GC Heap | 2 MB × 2 | Semispace collector, 4MB total |
| Goroutine Stack | 64 KB | Fixed size, cannot grow |
| Large Object Threshold | 64 KB | Objects larger bypass GC |

## 1. Pre-allocate During Loading

The garbage collector can pause your game for several milliseconds. Allocate everything during load screens, not gameplay.

### Bad: Allocating during gameplay

```go
func UpdateParticles() {
    for i := 0; i < 100; i++ {
        p := new(Particle)  // GC pause risk every frame!
        particles = append(particles, p)
    }
}
```

### Good: Object pooling

```go
// Pre-allocated pool
var particlePool [1000]Particle
var activeCount int

func Init() {
    activeCount = 0
}

func SpawnParticle() *Particle {
    if activeCount >= len(particlePool) {
        return nil  // Pool exhausted
    }
    p := &particlePool[activeCount]
    activeCount++
    *p = Particle{}  // Reset to zero
    return p
}

func DespawnParticle(index int) {
    // Swap with last active
    activeCount--
    particlePool[index] = particlePool[activeCount]
}
```

## 2. Respect the 64KB Stack Limit

Each goroutine has a fixed 64KB stack. Unlike desktop Go, stacks cannot grow. Deep recursion or large local variables will crash your game.

### Bad: Large local arrays

```go
func ProcessFrame() {
    var buffer [16384]float32  // 64KB on stack - CRASH!
    // ...
}
```

### Good: Use globals or heap for large data

```go
var frameBuffer [8192]float32  // Global, not on stack

func ProcessFrame() {
    // Use frameBuffer safely
    for i := range frameBuffer {
        frameBuffer[i] = 0
    }
}
```

### Bad: Deep recursion

```go
func TraverseTree(node *Node) {
    if node == nil { return }
    TraverseTree(node.left)   // Stack grows each call
    TraverseTree(node.right)  // Can overflow on deep trees
}
```

### Good: Iterative with explicit stack

```go
func TraverseTree(root *Node) {
    stack := make([]*Node, 0, 64)  // Heap-allocated
    stack = append(stack, root)
    
    for len(stack) > 0 {
        node := stack[len(stack)-1]
        stack = stack[:len(stack)-1]
        
        if node == nil { continue }
        // Process node...
        stack = append(stack, node.left, node.right)
    }
}
```

## 3. Reuse Slices

Creating new slices allocates memory. Reuse existing slices by resetting their length.

### Bad: New slice every frame

```go
func GetVisibleEnemies() []Enemy {
    result := make([]Enemy, 0)  // Allocation every call!
    for _, e := range allEnemies {
        if e.visible {
            result = append(result, e)
        }
    }
    return result
}
```

### Good: Reuse with length reset

```go
var visibleEnemies []Enemy

func Init() {
    visibleEnemies = make([]Enemy, 0, 100)  // Once during init
}

func GetVisibleEnemies() []Enemy {
    visibleEnemies = visibleEnemies[:0]  // Reset length, keep capacity
    for _, e := range allEnemies {
        if e.visible {
            visibleEnemies = append(visibleEnemies, e)
        }
    }
    return visibleEnemies
}
```

## 4. Minimize Goroutines

Each goroutine consumes 64KB of stack space. 100 goroutines = 6.4MB RAM—40% of total Dreamcast memory!

### Bad: Goroutine per entity

```go
for _, enemy := range enemies {
    go enemy.Think()  // 100 enemies = 6.4MB just for stacks!
}
```

### Good: Process on main goroutine

```go
func UpdateAllEnemies() {
    for i := range enemies {
        enemies[i].Think()  // Sequential, predictable
    }
}
```

### Acceptable: Few dedicated goroutines

```go
func main() {
    go audioMixer()      // One for audio streaming
    go networkHandler()  // One for network (if needed)
    
    // Main loop handles game logic
    for {
        Update()
        Render()
    }
}
```

## 5. Use Value Types for Small Structs

Small structs passed by value stay on the stack. Pointers may escape to the heap.

### Good: Pass small structs by value

```go
type Vec3 struct {
    X, Y, Z float32  // 12 bytes
}

func Add(a, b Vec3) Vec3 {
    return Vec3{a.X + b.X, a.Y + b.Y, a.Z + b.Z}
}

// Usage - no heap allocation
pos := Add(velocity, acceleration)
```

### Bad: Unnecessary pointer for small struct

```go
func Add(a, b *Vec3) *Vec3 {
    return &Vec3{a.X + b.X, a.Y + b.Y, a.Z + b.Z}  // Escapes to heap!
}
```

Structs under ~64 bytes are fine to pass by value.

## 6. Avoid String Operations During Gameplay

Strings are immutable. Concatenation creates new strings (garbage).

### Bad: String building in loop

```go
var log string
for i := 0; i < 100; i++ {
    log = log + "entry"  // New allocation each iteration!
}
```

### Bad: Formatted strings every frame

```go
func DrawHUD() {
    scoreText := fmt.Sprintf("Score: %d", score)  // Allocates!
    DrawText(scoreText)
}
```

### Good: Pre-render or avoid strings

```go
// For HUD: use digit sprites
func DrawScore(score int) {
    x := 100
    for score > 0 {
        digit := score % 10
        DrawSprite(digitSprites[digit], x, 10)
        x -= 16
        score /= 10
    }
}

// For debug: print directly (still allocates, but debug only)
println("Debug:", value)
```

## 7. Large Assets Bypass GC

Allocations over 64KB use `malloc` directly and are **not garbage collected**.

```go
// This 128KB texture is NOT managed by GC
texture := make([]byte, 256*256*2)

// It will live forever (or until program exit)
// This is usually fine - load assets once, keep forever
```

Implications:
- Large slices don't pressure the GC
- They also don't get freed automatically
- Perfect for textures, sounds, level data

## 8. Escape Analysis Awareness

The Go compiler decides whether variables go on stack (fast) or heap (needs GC). Variables "escape" to heap when:

- Returned from a function
- Stored in a slice, map, or struct field
- Passed to a goroutine
- Address taken and stored somewhere

### Stack allocated (good):

```go
func Calculate() int {
    x := 42        // Stays on stack
    y := x * 2     // Stays on stack
    return y       // Value returned, not pointer
}
```

### Heap allocated (be aware):

```go
func MakeEnemy() *Enemy {
    e := Enemy{}   // Must escape - we return pointer
    return &e      // Heap allocation here
}
```

### Force stack when possible:

```go
// Instead of returning pointer...
func MakeEnemy() *Enemy {
    return &Enemy{HP: 100}  // Heap
}

// Return value and let caller decide:
func NewEnemy() Enemy {
    return Enemy{HP: 100}  // Caller's stack or their choice
}
```

## 9. Map Usage Patterns

Maps allocate internally. Pre-size them and avoid creating during gameplay.

### Bad: Maps created during gameplay

```go
func SpawnWave() {
    enemyTypes := make(map[string]int)  // Allocation!
    enemyTypes["goblin"] = 10
    // ...
}
```

### Good: Pre-allocated maps

```go
var enemyTypes map[string]int

func Init() {
    enemyTypes = make(map[string]int, 10)  // Pre-size at init
}

func SpawnWave() {
    // Clear and reuse
    for k := range enemyTypes {
        delete(enemyTypes, k)
    }
    enemyTypes["goblin"] = 10
}
```

## 10. The Game Loop Pattern

A typical Dreamcast game structure:

```go
package main

// === PRE-ALLOCATED RESOURCES ===
var (
    enemies     [100]Enemy
    particles   [500]Particle
    projectiles [200]Projectile
    
    activeEnemies     []*Enemy
    activeParticles   []*Particle
    activeProjectiles []*Projectile
)

func Init() {
    // Pre-allocate slice capacity
    activeEnemies = make([]*Enemy, 0, 100)
    activeParticles = make([]*Particle, 0, 500)
    activeProjectiles = make([]*Projectile, 0, 200)
    
    // Load assets (large allocations OK here)
    LoadTextures()
    LoadSounds()
    LoadLevel()
}

func Update() {
    // Reset working slices
    activeEnemies = activeEnemies[:0]
    
    // Process game logic (no allocations!)
    for i := range enemies {
        if enemies[i].active {
            enemies[i].Update()
            activeEnemies = append(activeEnemies, &enemies[i])
        }
    }
}

func Render() {
    // Draw using pre-allocated data
    for _, e := range activeEnemies {
        e.Draw()
    }
}

func main() {
    Init()
    
    for !shouldExit {
        Input()
        Update()
        Render()
        // VSync handled by PVR
    }
}
```

## Quick Reference Card

### DO

```go
var pool [N]Object             // Pre-allocated pools
slice = slice[:0]              // Reset slice, keep capacity
for i := range arr { }         // Index iteration
small := Vec3{1, 2, 3}         // Value types
make([]T, 0, capacity)         // Pre-sized slices (at init)
val, ok := m[key]              // Safe map access
select { default: }            // Yield in loops
runtime_checkpoint()           // For panic recovery
```

### AVOID (during gameplay)

```go
make([]T, n)                   // New slices
append(s, x)                   // When at capacity  
new(T)                         // For small types
go func() {}()                 // Excessive goroutines
string + string                // String concatenation
fmt.Sprintf()                  // Formatted strings
recover()                      // Use runtime_checkpoint instead
for { busyWork() }             // Loops without yielding
```

## 11. Panic/Recover Limitation

Standard Go's `recover()` does **not work** on Dreamcast due to ABI differences. Use the `runtime_checkpoint()` pattern instead:

### Bad: Standard recover (won't work)

```go
func SafeCall() {
    defer func() {
        if r := recover(); r != nil {  // NEVER catches panics!
            println("recovered")
        }
    }()
    panic("oops")
}
```

### Good: Use runtime_checkpoint

```go
import _ "unsafe"

//go:linkname runtime_checkpoint runtime.runtime_checkpoint
func runtime_checkpoint() int

func SafeCall() (recovered bool) {
    defer func() {
        if runtime_checkpoint() != 0 {
            recovered = true
            return
        }
        // Normal cleanup here
    }()
    panic("oops")
    return false
}
```

Most game code shouldn't need recover. Design to avoid panics:
- Check bounds before indexing
- Validate inputs at entry points
- Use `ok` form for map access: `val, ok := m[key]`

## 12. Cooperative Scheduling

The Dreamcast scheduler is **cooperative**, not preemptive. Goroutines run until they yield.

### Goroutines yield when they:
- Send/receive on channels
- Call `select` (including with `default`)
- Call explicit yield functions
- Block on I/O

### Bad: Infinite loop without yielding

```go
go func() {
    for {
        doWork()  // Never yields - blocks all other goroutines!
    }
}()
```

### Good: Yield periodically

```go
go func() {
    for {
        doWork()
        select {
        case <-done:
            return
        default:
            // Yields to scheduler, then continues
        }
    }
}()
```

### Better: Use channels for work

```go
go func() {
    for item := range workQueue {  // Yields while waiting
        process(item)
    }
}()
```

### Timing is not guaranteed

Because of cooperative scheduling:
- Don't rely on precise goroutine ordering
- Deadlines are "best effort", not hard guarantees
- For real-time needs, keep critical work on main goroutine

## 13. Select with Default

`select` with `default` is an efficient polling pattern that yields correctly:

```go
func pollChannels() {
    for {
        select {
        case msg := <-inputChan:
            handleInput(msg)
        case result := <-resultChan:
            handleResult(result)
        default:
            // No message ready - yields to other goroutines
            // then returns immediately
        }
        
        // Can do other work here
        processFrame()
    }
}
```

This pattern works well for:
- Non-blocking channel checks
- Game loops that need to poll multiple sources
- Background workers that shouldn't block the main loop

## Platform Constraints

### Goroutine Leak

Dead goroutines retain ~160 bytes each (G struct only). The stack memory and
TLS are properly reclaimed, and the G struct is kept in a free list for reuse
by future goroutines. When you spawn a new goroutine, it reuses a G from the
free list if available.

If you spawn 10,000 goroutines that all exit without spawning new ones, you'll
have ~1.6MB in the free list. This memory is reused when you spawn new
goroutines. Monitor goroutine count with `runtime.NumGoroutine()`.

### Unrecoverable Runtime Panics

User `panic()` is recoverable. Runtime panics are not:

- Nil pointer dereference
- Array/slice bounds check
- Integer divide by zero
- Stack overflow

These crash immediately. A bounds check failure means program invariants are
violated—continuing would corrupt data.

### 32-bit Pointers

All pointers are 4 bytes. Code assuming 64-bit pointers will break.
`unsafe.Sizeof(uintptr(0))` returns 4, not 8.

### Single-Precision FPU

The SH-4 FPU operates in single precision. Double precision is software
emulated—extremely slow. Avoid `float64` in hot paths.

### Cache Coherency

DMA operations require explicit cache management. Use KOS cache functions
from C or via `//extern`:

```c
#include <arch/cache.h>

dcache_flush_range((uintptr_t)ptr, size);  // Before DMA write (CPU -> HW)
dcache_inval_range((uintptr_t)ptr, size);  // After DMA read (HW -> CPU)
```

### Not Implemented

- Race detector
- CPU/memory profiling
- Debugger support (delve, gdb)
- Plugin package
- cgo (use `//extern` for C functions)
- Signals (`os.Signal`, `signal.Notify`)
- Networking (requires Broadband Adapter)

### Limited Implementation

- **reflect**: Basic type inspection only, no `reflect.MakeFunc`
- **unsafe**: Works, but remember 4-byte pointers
- **sync**: Mutexes work, but with M:1 scheduling no other goroutine runs
  while you hold a lock—deadlock is impossible but starvation is easy

### Compatibility

- gccgo only (not the standard gc compiler)
- KallistiOS required
- SH-4 architecture only

## Debugging Tips

Available tools:
- Serial output via `println()` (routed to dc-tool)
- `LIBGODC_ERROR` / `LIBGODC_CRITICAL` macros (defined in runtime.h)
- GC statistics via the C function `gc_stats(&used, &total, &collections)`
- `runtime.NumGoroutine()` to count active goroutines
- KOS debug console (`dbglog()`)

Not available: stack traces, core dumps, breakpoints, variable inspection, heap profiling. When something goes wrong, you have `println()` and your brain.

If your game stutters:

1. **Check GC pauses**: Add timing around `forceGC()` calls to measure
2. **Count allocations**: Use pools and count `activeCount` 
3. **Monitor goroutines**: Keep count of active goroutines
4. **Profile stack usage**: Deep call chains near 64KB will crash

If your game freezes (but doesn't crash):

1. **Goroutine not yielding**: A goroutine in a tight loop starves others
2. **Deadlock**: Two goroutines waiting on each other's channels
3. **Main blocked**: Main goroutine waiting on a channel nobody sends to

If your game crashes:

1. **Stack overflow**: Reduce recursion, shrink local arrays
2. **Nil pointer**: Check slice bounds, map existence
3. **GC corruption**: Ensure pointers are valid (not into freed memory)
4. **Panic without checkpoint**: Use `runtime_checkpoint()` for recovery

## Further Reading

- `docs/DESIGN.md` - Runtime architecture
- `docs/KOS_WRAPPERS.md` - Hardware access
- `examples/` - Working game examples

Console development is the art of saying 'no' to malloc.

