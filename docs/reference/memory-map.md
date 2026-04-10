# Dreamcast Memory Map

Retail Dreamcast consoles have 16MB of main RAM in the half-open interval
`[0x8C000000, 0x8D000000)` and 8MB of VRAM for the PowerVR2 GPU.
This document shows the intended libgodc layout for that 16MB main RAM space.

## Main RAM (16MB)

```
  0x8D000000 ┌─────────────────────────────────────┐ High
             │  KOS main thread stack (128KB)      │
             │  Scheduler/runtime, grows downward  │
             ├─────────────────────────────────────┤
             │                                     │
             │  KOS malloc arena (conceptual)      │
             │  ┌─────────────────────────────────┐│
             │  │ GC semispace 1        (2MB)     ││
             │  ├─────────────────────────────────┤│
             │  │ GC semispace 0        (2MB)     ││
             │  ├─────────────────────────────────┤│
             │  │ goroutine stacks   (64KB each)  ││
             │  ├─────────────────────────────────┤│
             │  │ large objects       (>64KB)     ││
             │  ├─────────────────────────────────┤│
             │  │ G structs, misc runtime allocs  ││
             │  └─────────────────────────────────┘│
             │                                     │
             ├─────────────────────────────────────┤
             │  .bss  (uninitialized globals)      │
             ├─────────────────────────────────────┤
             │  .data (initialized globals)        │
             ├─────────────────────────────────────┤
             │  .rodata (type descriptors, strings)│
             ├─────────────────────────────────────┤
             │  .text  (compiled code)             │
  0x8C010000 ├─────────────────────────────────────┤
             │  KOS / dcload reserved  (64KB)      │
  0x8C000000 └─────────────────────────────────────┘ Low
```

Addresses within the KOS malloc arena are not fixed. The GC semispaces,
goroutine stacks, large objects, and runtime structs are all allocated via
`memalign()`/`malloc()` at startup or on demand. Their positions depend on
binary size and allocation order.

The sub-regions shown inside the KOS malloc arena are conceptual categories,
not a fixed top-to-bottom address order.

ATTENTION: Go `main.main` is started as a goroutine in `runtime/go-main.c`, so it runs
on a goroutine stack allocated by `goroutine_stack_init()`, not on the KOS
main thread stack shown at the top of the diagram.

## VRAM (8MB, separate)

The PowerVR2 GPU has its own 8MB of video RAM, not part of the 16MB main RAM.
Textures and framebuffers live here, allocated via `kos.PvrMemMalloc()`.
See [kos-wrappers.md](kos-wrappers.md) for the Go API.

## Region Reference

| Region | Size | Allocated by | Source | Config knob |
|--------|------|-------------|--------|-------------|
| KOS / dcload reserved (`0x8C000000`-`0x8C010000`) | 64KB | KOS boot/toolchain | KOS: `kernel/arch/dreamcast/include/arch/arch.h` defines `page_phys_base = 0x8c010000` as the start of usable RAM; this row is the 64KB below that boundary | - |
| Program binary (.text, .rodata, .data, .bss) | varies | linker | KOS: `utils/ldscripts/shlelf.xc` (`LOAD_OFFSET = 0x8c010000`) | - |
| GC semispace 0 | 2MB | `memalign(32, ...)` in `gc_init()` | `runtime/gc_heap.c` | `GC_SEMISPACE_SIZE_KB` in `runtime/godc_config.h` |
| GC semispace 1 | 2MB | `memalign(32, ...)` in `gc_init()` | `runtime/gc_heap.c` | `GC_SEMISPACE_SIZE_KB` in `runtime/godc_config.h` |
| Goroutine stacks | 64KB each | `memalign(8, ...)` in `stack_alloc()` | `runtime/stack.c` | `GOROUTINE_STACK_SIZE` in `runtime/godc_config.h` |
| Large objects (>64KB) | varies | `malloc()` in `gc_external_alloc()` | `runtime/gc_heap.c` | `GC_LARGE_OBJECT_THRESHOLD_KB` in `runtime/godc_config.h` |
| KOS main thread stack | 128KB | KOS thread init | `runtime/kos_startup.c` | `-DKOS_MAIN_STACK_SIZE=N` |
| VRAM | 8MB | `pvr_mem_malloc()` | KOS PVR driver | - |

RAM constants are defined in `runtime/dc_platform.h`:
`DC_RAM_START` (`0x8C000000`) and `DC_RAM_END` (`0x8D000000`).

In code, `DC_RAM_START` and `DC_RAM_END` define
`[DC_RAM_START, DC_RAM_END)`, where `DC_RAM_END` is exclusive.

KOS references above are paths relative to the KOS source tree.

For allocation strategy details, see the
[Memory Model](design.md#memory-model) section in `design.md`.
