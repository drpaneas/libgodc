# Panic and Recover

## Two Kinds of Errors

Most errors in Go are... boring. And that's good! You handle them like this:

```go
file, err := openFile("game.sav")
if err != nil {
    // No saved game? No problem.
    // Start a new game instead.
}
```

The function tells you something went wrong, and you decide what to do. Maybe you retry. Maybe you use a default. Maybe you tell the user. It's your choice.

But some errors are different. They're **programmer mistakes**:

```go
enemies := []Enemy{orc, goblin, troll}
enemy := enemies[99]  // WAIT. There's only 3 enemies!
```

This isn't "the file doesn't exist." This is "the code is broken." There's no sensible way to continue.

This is when Go **panics**.

---

## What Happens When You Panic

Here's the sequence, step by step:

```
                  Normal Execution
                        ↓
        ┌───────────────────────────────┐
        │  enemies := []Enemy{...}      │
        │  enemy := enemies[99]         │ ← PANIC!
        │  moveEnemy(enemy)             │ ← never runs
        └───────────────────────────────┘
                        ↓
              EXECUTION STOPS
                        ↓
        ┌───────────────────────────────┐
        │  Run all deferred functions   │
        │  (in reverse order!)          │
        └───────────────────────────────┘
                        ↓
          Did any defer call recover()?
                  /           \
                YES             NO
                 ↓               ↓
        Program continues   Program dies
```

The key insight: **deferred functions always run**, even during a panic. This is Go's cleanup guarantee.
Well... there are some really really bad cases (e.g. panic before runtime init or too many nested panics) where
this statement is _false_.

---

## Defer: The Cleanup Crew

Before we talk more about panic, let's understand defer. It's simple but powerful.

```go
func processEnemy(e *Enemy) {
    file := openLog("combat.log")
    defer closeLog(file)  // "Remember to do this when I leave!"
    
    damage := calculateDamage(e)
    applyDamage(e, damage)
    
    // closeLog runs here, automatically
}
```

The `defer` keyword says: "Don't run this now. Run it when the function exits."

No matter how you exit—return, panic, whatever—the deferred function runs.

### Multiple Defers: LIFO

If you have multiple defers, they run in **reverse order**. Last in, first out. Like a stack of plates:

```go
func setup() {
    defer println("First defer")   // Runs 3rd
    defer println("Second defer")  // Runs 2nd
    defer println("Third defer")   // Runs 1st
    println("Normal code")
}

// Output:
// Normal code
// Third defer
// Second defer
// First defer
```

Why reverse order? Think about it: if you opened file A, then file B, you want to close B before A. The last thing you set up is the first thing you tear down.

### Visualizing the Defer Chain

Each goroutine maintains a linked list of deferred functions:

```
G.defer → [cleanup3] → [cleanup2] → [cleanup1]
            newest                    oldest
             runs                      runs
             first                     last
```

When the function returns (or panics):
1. Pop cleanup3, run it
2. Pop cleanup2, run it
3. Pop cleanup1, run it
4. Done!

---

## Recover: Catching the Fall

Here's the safety net. `recover()` catches a panic mid-flight:

```go
func safeGameLoop() {
    if runtime_checkpoint() != 0 {
        // We land here after recovering from a panic
        // libgodc needs this, if you are going to use "recover" mechanisms
        println("Recovered! Returning to main menu...")
        return
    }
    
    defer func() {
        if r := recover(); r != nil {
            println("Caught panic:", r)
        }
    }()
    
    runGame()  // If this panics, we catch it!
}

func main() {
    safeGameLoop()
    println("Program continues!")  // This runs even after panic!
}
```

> **Note:** libgodc requires `runtime_checkpoint()` for recover to work properly. Without it, even a successful recover() will terminate the program. Standard Go handles this automatically via DWARF unwinding, but we use setjmp/longjmp instead (explained later in this chapter).

Let's trace what happens:

```
1. safeGameLoop() starts
2. runtime_checkpoint() saves recovery point, returns 0
3. defer registers our recovery function
4. runGame() starts
5. ... something bad happens ...
6. PANIC!
7. Deferred function runs
8. recover() catches the panic, marks it recovered
9. longjmp back to checkpoint, runtime_checkpoint() returns 1
10. "Recovered!" prints, function returns normally
11. "Program continues!" prints
```

The panic was caught. The program lives.

---

## The Golden Rule

Here's the catch: **recover only works inside a deferred function**.

```go
// THIS WORKS ✓
defer func() {
    recover()  // Called directly in defer
}()

// THIS DOESN'T WORK ✗
recover()  // Not in a defer—does nothing!
```

Why? Because recover needs to intercept the panic **during the cleanup phase**. If you're not in a defer, you're not in cleanup mode.

> **libgodc note**: Standard Go is even stricter—recover must be called *directly* in the defer, not in a helper function. We relaxed this rule because it's complex to implement and the behavior difference is benign for games. More panics get caught, which is fine.

---

## How We Implement It

Standard Go uses something called **DWARF unwinding**. It's sophisticated: the compiler generates detailed metadata about every function's stack layout, and a runtime library uses this to carefully walk back up the stack.

That's a lot of complexity. We don't have DWARF support on Dreamcast, yet (?).

Instead, we use an old C trick: **setjmp/longjmp**.

### The Teleportation Trick

Imagine setjmp as dropping a bookmark:

```c
jmp_buf bookmark;

if (setjmp(bookmark) == 0) {
    // First time through: setjmp returns 0
    printf("Starting...\n");
    doRiskyThing();
    printf("Made it!\n");
} else {
    // After longjmp: setjmp returns 1
    printf("Something went wrong!\n");
}
```

And longjmp teleports you back to that bookmark:

```c
void doRiskyThing() {
    // ...
    if (disaster) {
        longjmp(bookmark, 1);  // TELEPORT!
    }
    // ...
}
```

When longjmp is called, execution **jumps back** to setjmp, which now returns 1 instead of 0. All the function calls in between? Gone. Skipped. Like they never happened.

### The Recovery Path

```
┌─────────────────────────────────────────────────────────────┐
│   PANIC WITH CHECKPOINT                                     │
│                                                             │
│   func risky() {                                            │
│       if runtime_checkpoint() != 0 {                        │
│           return  // Recovered! Continue here.              │
│       }                                                     │
│       defer func() {                                        │
│           recover()                                         │
│       }()                                                   │
│       panic("oops")  // longjmp to checkpoint               │
│   }                                                         │
│                                                             │
│   → Clean, predictable                                      │
│   → Required for recover() to work in libgodc               │
└─────────────────────────────────────────────────────────────┘
```

> **Important:** Without `runtime_checkpoint()`, calling `recover()` will still mark the panic as recovered, but the program will terminate with "FATAL: recover without checkpoint". The checkpoint is **required** for proper recovery in libgodc.

---

## When Nobody Catches the Panic

If no recover catches the panic, the program dies. On Dreamcast, you'll see:

```
panic: index out of range [99] with length 3

goroutine 1 [running]:
  0x8c010234
  0x8c010456
  0x8c010678

Memory: arena=4194304 used=1258291 free=2936013
```

The console halts. The user has to manually reset. This is intentional. A crash is better than continuing with corrupted state and zombies.

---

## When Should You Panic?

Here's the decision tree:

```
Is this a programmer mistake?
        │
        ├── YES → Maybe panic is okay
        │           ├── nil pointer dereference
        │           ├── index out of bounds
        │           └── calling method on nil
        │
        └── NO → DON'T PANIC. Return an error.
                    ├── File not found
                    ├── Network timeout
                    ├── Invalid user input
                    └── Resource unavailable
```

### When Recover Makes Sense

Use recover at **boundaries**—places where you want to contain failures. In libgodc, remember to use `runtime_checkpoint()`:

```go
func handleEventSafely(event Event) {
    if runtime_checkpoint() != 0 {
        println("Event handler crashed, continuing...")
        return
    }
    
    defer func() {
        if r := recover(); r != nil {
            println("Caught:", r)
        }
    }()
    
    handleEvent(event)  // If this panics, we catch it
}
```

One bad event handler shouldn't kill the entire game.

> For general Go error handling best practices (when to panic vs return errors), see [Effective Go](https://go.dev/doc/effective_go#errors).

---

