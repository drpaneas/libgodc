# Glossary

Quick reference for terms used throughout this documentation.

## Runtime Terms

### Bump Allocator
An allocation strategy where memory is allocated by simply incrementing a pointer. O(1) allocation, but cannot free individual objects. libgodc uses this for the GC heap.

### Cheney's Algorithm
A garbage collection algorithm that copies live objects from one semispace to another using two pointers (scan and alloc). Named after C.J. Cheney who invented it in 1970.

### Context Switch
Saving one goroutine's CPU registers and loading another's, allowing multiple goroutines to share a single CPU. On SH4, this involves saving 64 bytes of state.

### Cooperative Scheduling
A scheduling model where goroutines must voluntarily yield control. Contrast with preemptive scheduling where the runtime can interrupt goroutines at any time.

### Forwarding Pointer
During garbage collection, a pointer left in an object's old location that points to its new location. Prevents copying the same object twice.

### G (Goroutine Struct)
The data structure representing a goroutine. Contains stack bounds, saved CPU context, defer chain, panic state, and scheduling information.

### GC Heap
The memory region managed by the garbage collector. In libgodc, this is 4MB total (two 2MB semispaces), with 2MB usable at any time.

### hchan
The internal structure representing a Go channel. Contains the buffer, send/receive indices, and wait queues.

### M:1 Model
A threading model where many goroutines (M) run on one OS thread (1). All goroutines share a single CPU, providing concurrency but not parallelism.

### Root
A starting point for garbage collection tracing. Roots include global variables, stack variables, and CPU registers that contain pointers.

### Run Queue
A list of goroutines that are ready to execute. The scheduler picks goroutines from this queue.

### SemiSpace Collector
A garbage collector that divides memory into two equal halves. Objects are allocated in one half; during collection, live objects are copied to the other half.

### Stop the World
A GC phase where all program execution pauses while the collector runs. libgodc uses stoptheworld collection exclusively.

### Sudog
"Sender/receiver descriptor"  a structure representing a goroutine waiting on a channel operation. Contains pointers to the goroutine, the channel, and the data being transferred.

### TLS (ThreadLocal Storage)
Pergoroutine storage. In libgodc, each goroutine has its own TLS block containing runtime state.

### Type Descriptor
Compilergenerated metadata about a Go type, including size, alignment, hash, and a bitmap indicating which fields contain pointers.

## Hardware Terms

### AICA
The Dreamcast's sound processor. An ARM7based chip with 2MB of dedicated sound RAM. Runs independently of the SH4 CPU.

### Cache Line
The unit of data transfer between cache and main memory. 32 bytes on SH4. Accessing one byte loads the entire cache line.

### GBR (Global Base Register)
An SH4 register reserved for threadlocal storage in KallistiOS. libgodc does not use GBR for goroutine TLS.

### KallistiOS (KOS)
The standard opensource SDK for Dreamcast homebrew development. Provides hardware abstraction, memory management, and drivers. It's pronounced "Kay os", so it resembles the sound of the word "chaos".

### PowerVR2
The Dreamcast's GPU. A tilebased deferred renderer with 8MB of dedicated VRAM.

### SH4
The Hitachi (now Renesas) SuperH4 processor used in the Dreamcast. 200MHz, 32bit, littleendian, with an FPU optimized for singleprecision math.

### VRAM
Video RAM. 8MB dedicated to the PowerVR2 GPU for textures and framebuffers. Allocated via `PvrMemMalloc()`, not the GC.

## Go Terms

### //extern
A gccgo directive that declares a function implemented in C. Allows Go code to call KOS functions directly.

### Escape Analysis
Compiler analysis that determines whether a variable can stay on the stack or must be allocated on the heap.

### gccgo
The GCC frontend for Go. Uses GCC's backend for code generation, supporting architectures like SH4 that the standard Go compiler doesn't support.

### Interface
A Go type that specifies a set of methods. Variables of interface type can hold any value that implements those methods.

### libgo
The standard gccgo runtime library. libgodc replaces this with a Dreamcastspecific implementation.

### Slice Header
The 12byte structure representing a Go slice: a pointer to the backing array, length, and capacity.

### String Header
The 8byte structure representing a Go string: a pointer to the character data and length.

## Abbreviations

| Abbr | Full Form | Meaning |
||||
| ABI | Application Binary Interface | How functions pass arguments and return values |
| BBA | Broadband Adapter | Dreamcast network adapter (10/100 Ethernet) |
| DMA | Direct Memory Access | Hardwaretohardware memory transfer without CPU |
| FPU | Floating Point Unit | CPU component for floatingpoint math |
| GC | Garbage Collector | Automatic memory management system |
| KB | Kilobyte | 1,024 bytes |
| MB | Megabyte | 1,048,576 bytes |
| MMU | Memory Management Unit | Hardware for virtual memory (Dreamcast doesn't have one) |
| PC | Program Counter | CPU register pointing to current instruction |
| PR | Procedure Register | SH4 register holding return address |
| SP | Stack Pointer | CPU register pointing to top of stack |
| TA | Tile Accelerator | PowerVR2 component that processes geometry |
| TLS | ThreadLocal Storage | Perthread/goroutine private data |
| VMU | Visual Memory Unit | Dreamcast memory card with LCD screen |

## Performance Numbers

Reference benchmarks from real Dreamcast hardware (200MHz SH4).

*Verified using `tests/bench_architecture.elf`:*

| Operation | Time | Notes |
||||
| `runtime.Gosched()` | 120 ns | Minimal yield |
| Direct function call | 140 ns | Baseline comparison |
| Buffered channel op | 1,459 ns | ~1.5 μs |
| Context switch | 6,634 ns | ~6.6 μs, full register save/restore |
| Unbuffered channel roundtrip | 12,782 ns | ~13 μs, send + receive |
| Goroutine spawn + run | 33,659 ns | ~34 μs, 240× overhead vs direct call |

### GC Pause Times

| Scenario | Pause | Notes |
||||
| Minimal/bypass (≥128 KB objects) | 73 μs | Objects bypass GC heap |
| 64 KB live data | 2,199 μs | ~2.2 ms |
| 32 KB live data | 6,172 μs | ~6.2 ms |

> **Note:** Objects ≥64 KB bypass the GC heap and go directly to `malloc`, hence the minimal pause. The 32 KB scenario with many small objects shows the highest pause because more objects must be scanned and copied.

### Memory Configuration

| Parameter | Value |
|||
| Goroutine stack | 64 KB |
| Context size | 64 bytes |
| GC header | 8 bytes |
| Large object threshold | 64 KB |

Run `tests/bench_architecture.elf` on your hardware to verify these numbers.

