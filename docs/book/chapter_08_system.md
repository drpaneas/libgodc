# Chapter 8: System Integration

## The Layer Cake

Imagine your game as an office building. You're on the top floor, writing Go code. But when you need something done — read a file, play a sound, draw a sprite. Well, obviously there is no such thing as "the cloud". Someone else does the actual work.

```text
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   Floor 4:  Your Go Program                                 │
│             "I want to play a sound!"                       │
│                    ↓                                        │
│   Floor 3:  libgodc (Go runtime)                            │
│             "Let me translate that..."                      │
│                    ↓                                        │
│   Floor 2:  KallistiOS                                      │
│             "I know how to talk to hardware."               │
│                    ↓                                        │
│   Floor 1:  Dreamcast Hardware                              │
│             *beep boop*                                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

Each floor speaks a different language. libgodc translates Go into something KallistiOS understands. KallistiOS translates that into hardware register writes.

You don't need to know all the details, but understanding the stack helps you debug problems.

---

## Part 1: Timers and Sleep

### How Does Sleep Work?

When you write:

```go
time.Sleep(100 * time.Millisecond)
```

What actually happens? Let's trace it:

```
┌─────────────────────────────────────────────────────────────┐
│   WHAT HAPPENS WHEN YOU SLEEP                               │
│                                                             │
│   Step 1: "I want to sleep for 100ms"                       │
│           ↓                                                 │
│   Step 2: Calculate wake time: now + 100ms = 4:00:00.100    │
│           ↓                                                 │
│   Step 3: Add timer to the timer heap                       │
│           ┌─────────────────────────────┐                   │
│           │ wake_time: 4:00:00.100      │                   │
│           │ goroutine: G7               │                   │
│           └─────────────────────────────┘                   │
│           ↓                                                 │
│   Step 4: Park the goroutine (it's now sleeping)            │
│           ↓                                                 │
│   Step 5: Scheduler runs OTHER goroutines                   │
│           ...100ms pass...                                  │
│           ↓                                                 │
│   Step 6: Scheduler checks timer heap                       │
│           "Hey, it's 4:00:00.100! Wake G7!"                 │
│           ↓                                                 │
│   Step 7: G7 wakes up, continues executing                  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**Key insight:** Your goroutine isn't actually sleeping on a couch somewhere. It's parked in a queue, and the scheduler knows when to wake it.

### Where Does Time Come From?

The SH-4 CPU has hardware timers. KallistiOS reads them:

```go
//extern timer_us_gettime64
func TimerUsGettime64() uint64
```

This returns microseconds since boot. Accurate to about 1 μs. Fast to read.

In your Go code, you can use this for precise timing:

```go
//extern timer_us_gettime64
func timerUsGettime64() uint64

func measureSomething() {
    start := timerUsGettime64()
    doExpensiveWork()
    elapsed := timerUsGettime64() - start
    println("Took", elapsed, "microseconds")
}
```

### The Timer Heap

Multiple goroutines can sleep at once. Go keeps them in a heap (priority queue) sorted by wake time:

```
Timer Heap:
┌───────────────────────────────────────────────────────────┐
│                                                           │
│   [G3: wake at 100ms]    ← Earliest, checked first        │
│           /\                                              │
│          /  \                                             │
│ [G7: 200ms]  [G2: 150ms]                                  │
│       /                                                   │
│  [G5: 500ms]                                              │
│                                                           │
└───────────────────────────────────────────────────────────┘
```

The scheduler only needs to check the top of the heap. If the earliest timer hasn't fired, none of them have.

---

## Part 2: File I/O (The Danger Zone)

### The Problem

You want to load a texture:

```go
data := loadFile("/cd/textures/enemy.pvr")
```

Seems innocent, right? Here's what actually happens:

```
┌─────────────────────────────────────────────────────────────┐
│   GD-ROM READ: THE SILENT KILLER                            │
│                                                             │
│   Time: 0ms    → loadFile() called                          │
│   Time: 0ms    → KOS asks GD-ROM to seek                    │
│   Time: 50ms   → Drive head moves (mechanical!)             │
│   Time: 100ms  → Data starts streaming                      │
│   Time: 150ms  → Still reading...                           │
│   Time: 200ms  → loadFile() returns                         │
│                                                             │
│   DURING THOSE 200ms:                                       │
│   • No other goroutines run                                 │
│   • Game loop frozen                                        │
│   • Audio buffer might run dry → glitch!                    │
│   • Player sees: lag, stutter, freeze                       │
│                                                             │
│   At 60 FPS, you have 16.6ms per frame.                     │
│   A 200ms file read = 12 FROZEN FRAMES!                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**Why does this happen?** KOS file operations are **synchronous**. The CPU sits in a loop waiting for the CD drive. No scheduler runs. Nothing else happens.

### The Solutions

**Solution 1: Loading Screens**

Load everything at startup or level transitions:

```go
func main() {
    showLoadingScreen()
    
    // All the slow stuff happens here
    textures = loadAllTextures()
    sounds = loadAllSounds()
    levelData = loadLevel(1)
    
    hideLoadingScreen()
    
    // Now game loop is safe
    for {
        gameLoop()
    }
}
```

**Solution 2: Streaming in Chunks**

If you must load during gameplay, do it in small pieces:

```go
func streamTexture(path string) {
    file := openFile(path)
    defer closeFile(file)
    
    for !file.EOF() {
        chunk := file.Read(4096)  // Read 4KB
        processChunk(chunk)
        runtime.Gosched()  // Let other goroutines run!
    }
}
```

**Solution 3: Pre-load into RAM**

The Dreamcast has 16 MB of RAM. Use it!

```go
// At startup, load everything you might need
var textureCache = make(map[string][]byte)

func preloadTexture(name string) {
    textureCache[name] = loadFile("/cd/textures/" + name)
}

// During gameplay, instant access
func getTexture(name string) []byte {
    return textureCache[name]  // Already in RAM!
}
```

---

## Part 3: Calling C Functions

### The //extern Magic

Go code can call C functions directly:

```go
//extern pvr_wait_ready
func PvrWaitReady() int32

//extern maple_enum_dev
func mapleEnumDev(port, unit int32) uintptr

func main() {
    PvrWaitReady()  // Calls the C function!
}
```

No CGo. No runtime overhead. Just a direct function call.

### The Danger

Here's the catch: C functions run on your goroutine's stack. Goroutines have fixed stacks (64 KB by default). If the C function is stack-hungry:

```
┌─────────────────────────────────────────────────────────────┐
│   STACK OVERFLOW SCENARIO                                   │
│                                                             │
│   Goroutine stack: 64 KB                                    │
│                                                             │
│   ┌────────────────────┐ ← Stack top                        │
│   │ Your Go function   │ 1 KB used                          │
│   ├────────────────────┤                                    │
│   │ C function called  │                                    │
│   │   local arrays...  │ 6 KB used                          │
│   │   more locals...   │                                    │
│   ├────────────────────┤                                    │
│   │ C calls another C  │                                    │
│   │   BOOM!            │ OVERFLOW!                          │
│   └────────────────────┘ ← Stack bottom (guard page)        │
│                                                             │
│   Result: Memory corruption, crash, mysterious bugs         │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Part 4: Debugging Without Fancy Tools

### The Detective's Toolkit

**Tool 1: Print Statements**

The oldest debugging technique is still the best:

```go
func suspiciousFunction(x int) {
    println(">>> suspiciousFunction start, x =", x)
    
    result := doSomething(x)
    println("    after doSomething, result =", result)
    
    processResult(result)
    println("<<< suspiciousFunction end")
}
```

**Tool 2: Binary Search Debugging**

Program crashes somewhere. Where?

```
1. Add print at function start and end
2. If it prints START but not END, crash is inside
3. Add print in the middle
4. Repeat until you find the exact line
```

**Tool 3: The Assumptions Checklist**

When something "can't possibly be wrong," check it:

```go
func processEnemy(e *Enemy) {
    // CHECK YOUR ASSUMPTIONS
    if e == nil {
        println("BUG: e is nil!")
        return
    }
    if e.Health < 0 {
        println("BUG: negative health:", e.Health)
    }
    if e.X < 0 || e.X > 640 {
        println("BUG: X out of bounds:", e.X)
    }
    
    // Now do the actual work
    // ...
}
```

### Reading Crash Information

When your game crashes, you might see:

```
panic: index out of range [99] with length 3

Registers:
  PC=8c015678  PR=8c015432

Stack trace:
  0x8c015678
  0x8c015432
  0x8c014000
```

What does this mean?

- **PC (Program Counter)** — Where the crash happened
- **PR (Procedure Register)** — Who called us (return address)
- **Stack trace** — Chain of function calls

### Finding the Function Name

You have an address: `0x8c015678`. Where is it?

**Method 1: addr2line**

```bash
sh-elf-addr2line -e game.elf 0x8c015678
# Output: /path/to/main.go:42
```

This tells you the exact line number!

**Method 2: Symbol Table**

```bash
sh-elf-nm game.elf | sort > symbols.txt
# Then search for addresses near 0x8c015678
```

**Method 3: With Function Names**

```bash
sh-elf-addr2line -f -C -i -e game.elf 0x8c015678
# Output: functionName
#         main.go:42
```go

### Common Bugs and Fixes

| Symptom | Likely Cause | Fix |
|---------|--------------|-----|
| Hangs, no output | Infinite loop without yield | Add `runtime.Gosched()` in loops |
| Garbage on screen | Memory corruption | Check array bounds |
| Random crashes | Stack overflow | Check deep recursion, big C calls |
| GC panic | Too much live data | Reduce heap usage, trigger GC earlier |
| Works in emu, fails on hw | Timing differences | Test on real hardware earlier! |

### Troubleshooting Flowchart

Use this decision tree when things go wrong:

```text
┌──────────────────────────────────────────────────────────────┐
│   TROUBLESHOOTING FLOWCHART                                  │
│                                                              │
│   What's happening?                                          │
│         │                                                    │
│         ├─► CRASH (program terminates)                       │
│         │         │                                          │
│         │         ├─► Panic message visible?                 │
│         │         │         │                                │
│         │         │         ├─► YES: Read the message!       │
│         │         │         │   • "index out of range"       │
│         │         │         │     → Check slice bounds       │
│         │         │         │   • "nil pointer"              │
│         │         │         │     → Check for nil before use │
│         │         │         │   • "out of memory"            │
│         │         │         │     → Reduce allocations       │
│         │         │         │                                │
│         │         │         └─► NO: Stack overflow likely    │
│         │         │             → Reduce local variables     │
│         │         │             → Convert recursion to loop  │
│         │         │                                          │
│         ├─► FREEZE (no crash, no progress)                   │
│         │         │                                          │
│         │         ├─► Any goroutines running?                │
│         │         │         │                                │
│         │         │         ├─► Only one: Infinite loop      │
│         │         │         │   → Add runtime.Gosched()      │
│         │         │         │                                │
│         │         │         └─► Multiple: Deadlock           │
│         │         │             → Check channel usage        │
│         │         │             → Ensure sends have receivers│
│         │         │                                          │
│         ├─► STUTTER (periodic lag)                           │
│         │         │                                          │
│         │         └─► GC pauses likely                       │
│         │             → Reduce live heap size                │
│         │             → Trigger GC during loading            │
│         │             → Use object pools                     │
│         │                                                    │
│         └─► WRONG OUTPUT (runs but incorrect)                │
│                   │                                          │
│                   └─► Add println() everywhere               │
│                       → Check variable values                │
│                       → Verify assumptions                   │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### The 5-Step Debug Process

```text
┌─────────────────────────────────────────────────────────────┐
│   THE DEBUGGING ALGORITHM                                   │
│                                                             │
│   1. REPRODUCE                                              │
│      Can you make it happen consistently?                   │
│      If not, add logging until you can.                     │
│                                                             │
│   2. NARROW DOWN                                            │
│      Binary search with prints.                             │
│      "Does it crash before this line or after?"             │
│                                                             │
│   3. CHECK ASSUMPTIONS                                      │
│      Print everything. That variable you're SURE is         │
│      correct? Print it anyway.                              │
│                                                             │
│   4. SIMPLIFY                                               │
│      Create the smallest program that shows the bug.        │
│      Often, you'll find the bug while simplifying.          │
│                                                             │
│   5. TAKE A BREAK                                           │
│      Seriously. Walk away. Fresh eyes find bugs faster      │
│      than tired eyes.                                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Part 5: Testing on a Game Console

### The Test Structure

Our tests are simple: standalone executables that print PASS or FAIL.

```text
tests/
├── test_types.go      → test_types.elf      (maps, interfaces, structs)
├── test_goroutines.go → test_goroutines.elf (goroutines, channels)
├── test_memory.go     → test_memory.elf     (allocation, GC)
└── test_control.go    → test_control.elf    (defer, panic, recover)
```

No fancy test framework. No JUnit. Just:

1. Do something
2. Check if it worked
3. Print the result

### A Minimal Test

```go
package main

func TestMaps() {
    println("maps:")
    passed := 0
    total := 0

    total++
    m := make(map[string]int)
    m["score"] = 100
    if m["score"] == 100 {
        passed++
        println("  PASS: read after write")
    } else {
        println("  FAIL: read after write")
    }

    total++
    if m["missing"] == 0 {
        passed++
        println("  PASS: missing key returns zero")
    } else {
        println("  FAIL: missing key returns zero")
    }

    total++
    delete(m, "score")
    _, ok := m["score"]
    if !ok {
        passed++
        println("  PASS: delete removes key")
    } else {
        println("  FAIL: delete removes key")
    }

    println("  ", passed, "/", total)
}

func main() {
    TestMaps()
}
```

### Running Tests

```bash
# Build the test
make test_types

# Run on Dreamcast
dc-tool-ip -t 192.168.2.205 -x test_types.elf

# Output:
# maps:
#   PASS: read after write
#   PASS: missing key returns zero
#   PASS: delete removes key
#   3 / 3
```

### Emulator vs Hardware

| Aspect | Emulator | Real Hardware |
|--------|----------|---------------|
| Speed | Fast iteration | Slower uploads |
| Debugging | Can use host tools | printf only |
| Accuracy | Close but not exact | The truth |
| Timing | May differ | Definitive |

**The Strategy:**

```
┌─────────────────────────────────────────────────────────────┐
│   DEVELOPMENT WORKFLOW                                      │
│                                                             │
│   80% of time: Emulator                                     │
│   ├── Fast compile-run cycle                                │
│   ├── Quick iteration                                       │
│   └── Good for logic bugs                                   │
│                                                             │
│   20% of time: Real Hardware                                │
│   ├── Catches timing issues                                 │
│   ├── Finds memory/stack problems                           │
│   └── Final validation before release                       │
│                                                             │
│   RULE: Never release without testing on real hardware!     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

The Dreamcast is a 25-year-old console with 16 MB of RAM, no debugger, and a CD-ROM that takes 200ms to seek. And yet, people made incredible games for it. You can too. You just need patience, println, and the knowledge in this chapter.
