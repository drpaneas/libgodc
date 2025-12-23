# Conclusion

## What We Built

We started with a simple question: *Can Go run on a 1998 game console?*

The answer is yes. Not perfectly, not completely, but yes.

```text
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   libgodc: A Go Runtime for the Sega Dreamcast              │
│                                                             │
│   ┌─────────────────────────────────────────────────────┐   │
│   │  ✓ Memory allocation with bump allocator            │   │
│   │  ✓ Garbage collection (semi-space copying)          │   │
│   │  ✓ Goroutines (cooperative M:1 scheduling)          │   │
│   │  ✓ Channels (buffered and unbuffered)               │   │
│   │  ✓ Select statement                                 │   │
│   │  ✓ Defer, panic, and recover                        │   │
│   │  ✓ Maps, slices, strings, interfaces                │   │
│   │  ✓ Direct C interop via //extern                    │   │
│   └─────────────────────────────────────────────────────┘   │
│                                                             │
│   All running on 16MB RAM and a 200MHz CPU.                 │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## The Trade-offs We Made

Every design decision was a trade-off. Here's what we chose and why:

| Decision               | What We Gave Up      | What We Gained                  |
|------------------------|----------------------|---------------------------------|
| Semi-space GC          | 50% of heap unusable | No fragmentation, simple code   |
| Cooperative scheduling | Preemption           | No locks, predictable timing    |
| Fixed 64KB stacks      | Stack growth         | Simplicity, no stack probes     |
| M:1 model              | Parallelism          | No thread synchronization       |
| setjmp/longjmp panic   | DWARF unwinding      | Works without debug info        |
| No finalizers          | Destructor patterns  | Simpler GC, predictable cleanup |

These aren't the "right" choices for every platform. They're the **our** choices for *this* platform.

---

## What We Didn't Build

libgodc is not a complete Go implementation. We deliberately left out:

- **Race detector** — No parallelism means no data races
- **CPU/memory profiling** — Use `println` and timers
- **Debugger support** — Not available go debugger
- **Full reflection** — Binary size matters
- **Preemptive scheduling** — Complexity for no benefit
- **Concurrent GC** — Single core, stop-the-world is fine

---

## Lessons for Runtime Implementers

If you're building a runtime for another constrained platform, here's what we learned:

* Don't plan everything upfront. Get `println("Hello")` working first. The linker errors will guide you to the next step.
* When documentation fails, read the code. gccgo's `libgo/runtime/` directory answered questions no documentation could.
* Our first GC was embarrassingly slow. It didn't matter. Once it worked, we could measure and optimize. Premature optimization would have wasted months.
* Emulators lie. Timing is different. Memory layout is different. Test on hardware as soon as you can run anything.
* Fighting the hardware is futile. The SH-4 has 16MB RAM and a 200MHz CPU. Accept it. Design for it. Work with it.

---

## The Bigger Picture

Coding this project, helped me understand better what Go actually does.

When you write `go func() {}`, something has to:
- Allocate a stack
- Save the entry point
- Add it to a run queue
- Eventually switch contexts to run it

When you write `x := make([]int, 10)`, something has to:
- Calculate the size
- Find free memory
- Initialize the slice header
- Eventually clean up when it's garbage

That "something" is the runtime. Every high-level language has one. 
Understanding how it works makes you a better programmer in any language.

---

## What's Next?

libgodc is open source. You can:

1. **Use it** — Build games for the Dreamcast in Go
2. **Extend it** — Add features you need
3. **Learn from it** — Apply these patterns to other platforms
4. **Contribute** — Fix bugs, improve performance, write examples

The Dreamcast community is small but passionate. Join us at:
- [Dreamcast-Talk Forums](https://www.dreamcast-talk.com/forum/)
- [Simulant Discord](https://discord.gg/simulant)
- [GitHub Issues](https://github.com/drpaneas/libgodc/issues)

---

## Final Words

The Sega Dreamcast was released on November 27, 1998, in Japan. It was discontinued on March 31, 2001—a commercial failure that outlived its corporate support by decades.

Twenty-five years later, people are still writing code for it. Still pushing its limits. Still finding joy in its constraints.

That's the magic of retro computing. It's not about nostalgia. It's about craft. Modern development gives us infinite resources and infinite complexity. Old hardware gives us finite resources and forces elegant solutions.

libgodc exists because someone asked: "Can Go run on a Dreamcast?"

The answer is yes. And now you know how.

---


Thank you for reading,
Panos

