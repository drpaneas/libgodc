# KOS API Bindings

KOS is written in C. Your game is written in Go. gccgo's `//extern` directive
lets you call C functions directly with no wrapper overhead.

```
┌─────────────────────────────────────────────────────┐
│  Go Code                                            │
│      kos.PvrInitDefaults()                          │
│                │                                    │
│                ▼                                    │
│  //extern pvr_init_defaults                         │
│  func PvrInitDefaults() int32                       │
│                │                                    │
│                ▼                                    │
│  pvr_init_defaults() in libkallisti.a               │
│                │                                    │
│                ▼                                    │
│  Dreamcast Hardware                                 │
└─────────────────────────────────────────────────────┘
```

## Basic Syntax

### Function with No Arguments

```go
//go:build gccgo

package kos

//extern pvr_scene_begin
func PvrSceneBegin()
```

The `//extern` comment must immediately precede the function declaration,
with no blank lines between them. The function has no body—gccgo generates
the call directly.

### Function with Arguments

```go
//extern pvr_list_begin
func PvrListBegin(list uint32) int32

//extern pvr_poly_compile
func pvrPolyCompile(header uintptr, context uintptr)
```

Arguments are passed according to the SH-4 ABI: first four in registers
(r4-r7), remainder on the stack.

### Function with Return Value

```go
//extern pvr_mem_available
func PvrMemAvailable() uint32

//extern timer_us_gettime64
func TimerUsGettime64() uint64
```

Return values come back in r0 (32-bit) or r0:r1 (64-bit).

## Type Mappings

The SH-4 is a 32-bit architecture with 4-byte alignment.

| C Type          | Go Type           | Size  | Notes                    |
|-----------------|-------------------|-------|--------------------------|
| `void`          | (no return)       | -     |                          |
| `int`           | `int32`           | 4     | SH-4 int is 32-bit       |
| `unsigned int`  | `uint32`          | 4     |                          |
| `int8_t`        | `int8`            | 1     |                          |
| `uint8_t`       | `uint8`           | 1     |                          |
| `int16_t`       | `int16`           | 2     |                          |
| `uint16_t`      | `uint16`          | 2     |                          |
| `int32_t`       | `int32`           | 4     |                          |
| `uint32_t`      | `uint32`          | 4     |                          |
| `int64_t`       | `int64`           | 8     |                          |
| `uint64_t`      | `uint64`          | 8     |                          |
| `float`         | `float32`         | 4     |                          |
| `double`        | `float64`         | 8     | Software emulated—slow   |
| `void*`         | `unsafe.Pointer`  | 4     |                          |
| `char*`         | `*byte`           | 4     | Or `unsafe.Pointer`      |
| `size_t`        | `uint32`          | 4     | uintptr also works       |
| `struct foo*`   | `*Foo`            | 4     | Define matching Go struct|

### Pointer Size

All pointers are 4 bytes. Code that assumes 64-bit pointers will break.
`unsafe.Sizeof(uintptr(0))` is 4, not 8.

## Struct Mappings

When a KOS function takes a pointer to a struct, you have two options:

### Option 1: unsafe.Pointer (Quick and Dirty)

```go
//extern pvr_vertex_submit
func pvrVertexSubmit(data unsafe.Pointer, size int32)

// Usage:
func SubmitVertex(v *PvrVertex) {
    pvrVertexSubmit(unsafe.Pointer(v), int32(unsafe.Sizeof(*v)))
}
```

Works but provides no type safety. Fine for prototyping.

### Option 2: Matching Go Struct (Correct)

Define a Go struct with identical layout to the C struct:

```c
// From dc/pvr.h
typedef struct {
    uint32_t flags;
    float x, y, z;
    float u, v;
    uint32_t argb;
    uint32_t oargb;
} pvr_vertex_t;
```

```go
// In Go
type PvrVertex struct {
    Flags      uint32
    X, Y, Z    float32
    U, V       float32
    ARGB       uint32
    OARGB      uint32
}

//extern pvr_prim
func pvrPrim(data unsafe.Pointer, size int32)

// PvrPrimVertex submits a vertex to the TA
func PvrPrimVertex(v *PvrVertex) {
    pvrPrim(unsafe.Pointer(v), 32)  // 32 bytes
}
```

Verify the struct size matches:

```go
func init() {
    if unsafe.Sizeof(PvrVertex{}) != 32 {
        panic("PvrVertex size mismatch")
    }
}
```

### Alignment Matters

C structs may have padding for alignment. Go structs follow Go's alignment
rules, which may differ. Always verify sizes match.

```go
// C struct with padding:
// struct { char a; int b; }  // 8 bytes (3 bytes padding after a)

// Go equivalent:
type Example struct {
    A   byte
    _   [3]byte  // Explicit padding
    B   int32
}
```

## Stub Files for Host Compilation

Go files using `//extern` only compile with gccgo. For IDE support and
host-side testing, create stub files:

### pvr.go (Dreamcast build)

```go
//go:build gccgo

package kos

//extern pvr_init_defaults
func PvrInitDefaults() int32

//extern pvr_scene_begin
func PvrSceneBegin()
```

### pvr_stub.go (Host build)

```go
//go:build !gccgo

package kos

func PvrInitDefaults() int32 { panic("kos: not on Dreamcast") }
func PvrSceneBegin()         { panic("kos: not on Dreamcast") }
```

The build tag ensures the right file is used:

- `gccgo` tag: compiles with sh-elf-gccgo (Dreamcast)
- `!gccgo` tag: compiles with standard go (host)

## Common Patterns

### Wrapper for Type Safety

Expose a safe public API, hide the unsafe internals:

```go
// Private: direct C binding
//extern maple_dev_status
func mapleDevStatus(dev uintptr) uintptr

// Public: type-safe wrapper with method syntax
func (d *MapleDevice) ContState() *ContState {
    if d == nil {
        return nil
    }
    ptr := mapleDevStatus(uintptr(unsafe.Pointer(d)))
    if ptr == 0 {
        return nil
    }
    return (*ContState)(unsafe.Pointer(ptr))
}
```

### Slice to C Array

C functions expect a pointer and length. Go slices have both:

```go
//extern pvr_txr_load
func pvrTxrLoad(src unsafe.Pointer, dst unsafe.Pointer, count uint32)

func PvrTxrLoad(src []byte, dst unsafe.Pointer) {
    if len(src) == 0 {
        return
    }
    pvrTxrLoad(unsafe.Pointer(&src[0]), dst, uint32(len(src)))
}
```

Always check for empty slices—`&src[0]` panics on an empty slice.

### String to C String

Go strings are not null-terminated. C functions expect null-terminated
strings.

```go
import "unsafe"

// Convert Go string to C string (allocates)
func cstring(s string) *byte {
    b := make([]byte, len(s)+1)
    copy(b, s)
    b[len(s)] = 0
    return &b[0]
}

// Usage:
//extern fs_open
func fsOpen(path *byte, mode int32) int32

func Open(path string) int32 {
    return fsOpen(cstring(path), O_RDONLY)
}
```

For hot paths, avoid allocation by using fixed buffers:

```go
var pathBuf [256]byte

func OpenFast(path string) int32 {
    if len(path) >= 255 {
        panic("path too long")
    }
    copy(pathBuf[:], path)
    pathBuf[len(path)] = 0
    return fsOpen(&pathBuf[0], O_RDONLY)
}
```

### Callback Functions

Some KOS functions take callbacks. This requires careful handling:

```go
//extern pvr_set_bg_color
func PvrSetBgColor(r, g, b float32)

// For callbacks, you often need to use //export to make a Go function
// callable from C. However, this is complex with gccgo.
// Prefer polling over callbacks when possible.
```

Callbacks from C to Go are tricky because:

1. The callback runs on whatever stack C chooses
2. The Go scheduler may not be in a consistent state
3. The GC may be running

Poll instead of using callbacks when you can.

## Caveats

### Stack Usage

KOS functions run on the calling goroutine's stack. Deep C call chains
can overflow the 64KB stack:

```go
// DANGEROUS: Unknown stack depth
func LoadLevel(path string) {
    // fs_open -> iso9660_read -> g2_read -> ...
    // How deep does this go?
}
```

Solutions:
1. Call from the main goroutine (larger stack)
2. Limit recursion depth in your code
3. Move heavy I/O to loading screens

### Blocking Calls

Some KOS functions block (file I/O, CD reads). During blocking:

- No other goroutines run (M:1 scheduler is blocked)
- Timers don't fire
- The game freezes

```go
// BAD: Blocks entire game for 200ms+
data := loadFile("/cd/level.dat")

// BETTER: Do during loading screen
showLoadingScreen()
data := loadFile("/cd/level.dat")
hideLoadingScreen()

// BEST: Stream data over multiple frames
go streamFile("/cd/level.dat", dataChan)
```

### GBR Register

libgodc uses a global pointer for goroutine TLS, leaving GBR for KOS.
This means KOS `_Thread_local` variables work correctly.

If you're writing assembly or using inline asm, don't touch GBR—it's
reserved for KOS.

## Building the kos Package

The `kos/` directory contains the official bindings. To rebuild:

```sh
cd kos/
make clean
make
make install  # Copies to $KOS_BASE/lib/
```

This produces:
- `kos.gox` — Export data for the Go compiler
- `libkos.a` — Compiled bindings for the linker

## Adding New Bindings

### Step 1: Find the C Declaration

```sh
grep -r "pvr_mem_reset" $KOS_BASE/include/
# Found in dc/pvr.h:
# void pvr_mem_reset(void);
```

### Step 2: Write the Go Binding

```go
//extern pvr_mem_reset
func PvrMemReset()
```

For functions with complex signatures, check the header carefully:

```c
// From dc/pvr.h
int pvr_prim(void *data, size_t size);
```

```go
//extern pvr_prim
func pvrPrim(data unsafe.Pointer, size uint32) int32
```

### Step 3: Add Type-Safe Wrapper (Optional)

```go
// For polygon headers (using helper function)
func PvrPrim(hdr *PvrPolyHdr) int32 {
    return goPvrPrimHdr(unsafe.Pointer(hdr))
}

// For vertices (using helper function)
func PvrPrimVertex(v *PvrVertex) int32 {
    return goPvrPrimVertex(unsafe.Pointer(v))
}
```

Note: For performance-critical paths like vertex submission, libgodc uses
specialized C helper functions (`__go_pvr_prim_hdr`, `__go_pvr_prim_vertex`)
that handle store queue operations efficiently.

### Step 4: Add Stub

```go
func PvrMemReset() {
    panic("kos: not on Dreamcast")
}
```

### Step 5: Rebuild

```sh
make clean && make && make install
```

## Reference: KOS Subsystems

| Subsystem | Header          | Prefix     | Description              |
|-----------|-----------------|------------|--------------------------|
| PVR       | `dc/pvr.h`      | `pvr_`     | PowerVR graphics         |
| Maple     | `dc/maple.h`    | `maple_`   | Controllers, VMU, etc.   |
| Sound     | `dc/sound/`     | `snd_`     | AICA sound chip          |
| Streaming | `dc/snd_stream.h` | `snd_stream_` | Audio streaming     |
| Filesystem| `kos/fs.h`      | `fs_`      | File operations          |
| Timer     | `arch/timer.h`  | `timer_`   | High-resolution timing   |
| Video     | `dc/video.h`    | `vid_`     | Video modes              |
| G2 Bus    | `dc/g2bus.h`    | `g2_`      | Bus transfers            |
| CDROM     | `dc/cdrom.h`    | `cdrom_`   | CD access                |
| VMU       | `dc/vmu_*.h`    | `vmu_`     | Visual Memory Unit       |
| BFont     | `dc/biosfont.h` | `bfont_`   | BIOS font rendering      |

## Example: Complete PVR Bindings

### pvr.go

```go
//go:build gccgo

package kos

import "unsafe"

// PvrPtr is a pointer to PVR video memory (VRAM)
type PvrPtr uintptr

// PVR list types
const (
    PVR_LIST_OP_POLY uint32 = 0  // Opaque polygons
    PVR_LIST_OP_MOD  uint32 = 1  // Opaque modifiers
    PVR_LIST_TR_POLY uint32 = 2  // Translucent polygons
    PVR_LIST_TR_MOD  uint32 = 3  // Translucent modifiers
    PVR_LIST_PT_POLY uint32 = 4  // Punch-through polygons
)

// Initialization
//extern pvr_init_defaults
func PvrInitDefaults() int32

// Scene management
//extern pvr_scene_begin
func PvrSceneBegin()

//extern pvr_scene_finish
func PvrSceneFinish() int32

//extern pvr_wait_ready
func PvrWaitReady() int32

// List management
//extern pvr_list_begin
func PvrListBegin(list uint32) int32

//extern pvr_list_finish
func PvrListFinish() int32

// Primitive submission via helper functions
//extern __go_pvr_prim_hdr
func goPvrPrimHdr(data unsafe.Pointer) int32

//extern __go_pvr_prim_vertex
func goPvrPrimVertex(data unsafe.Pointer) int32

type PvrVertex struct {
    Flags      uint32
    X, Y, Z    float32
    U, V       float32
    ARGB       uint32
    OARGB      uint32
}

// PvrPrim submits a polygon header
func PvrPrim(hdr *PvrPolyHdr) int32 {
    return goPvrPrimHdr(unsafe.Pointer(hdr))
}

// PvrPrimVertex submits a vertex
func PvrPrimVertex(v *PvrVertex) int32 {
    return goPvrPrimVertex(unsafe.Pointer(v))
}

// Memory management
//extern pvr_mem_malloc
func PvrMemMalloc(size uint32) PvrPtr

//extern pvr_mem_free
func PvrMemFree(ptr PvrPtr)

//extern pvr_mem_available
func PvrMemAvailable() uint32
```

### pvr_stub.go

```go
//go:build !gccgo

package kos

type PvrPtr uintptr

const (
    PVR_LIST_OP_POLY uint32 = 0
    PVR_LIST_OP_MOD  uint32 = 1
    PVR_LIST_TR_POLY uint32 = 2
    PVR_LIST_TR_MOD  uint32 = 3
    PVR_LIST_PT_POLY uint32 = 4
)

type PvrVertex struct {
    Flags      uint32
    X, Y, Z    float32
    U, V       float32
    ARGB       uint32
    OARGB      uint32
}

func PvrInitDefaults() int32           { panic("kos: not on Dreamcast") }
func PvrSceneBegin()                   { panic("kos: not on Dreamcast") }
func PvrSceneFinish() int32            { panic("kos: not on Dreamcast") }
func PvrWaitReady() int32              { panic("kos: not on Dreamcast") }
func PvrListBegin(list uint32) int32   { panic("kos: not on Dreamcast") }
func PvrListFinish() int32             { panic("kos: not on Dreamcast") }
func PvrPrim(hdr *PvrPolyHdr) int32    { panic("kos: not on Dreamcast") }
func PvrPrimVertex(v *PvrVertex) int32 { panic("kos: not on Dreamcast") }
func PvrMemMalloc(size uint32) PvrPtr  { panic("kos: not on Dreamcast") }
func PvrMemFree(ptr PvrPtr)            { panic("kos: not on Dreamcast") }
func PvrMemAvailable() uint32          { panic("kos: not on Dreamcast") }
```

## Usage in Games

```go
package main

import "kos"

func main() {
    kos.PvrInitDefaults()

    for {
        kos.PvrWaitReady()
        kos.PvrSceneBegin()

        kos.PvrListBegin(kos.PVR_LIST_OP_POLY)
        drawOpaqueGeometry()
        kos.PvrListFinish()

        kos.PvrListBegin(kos.PVR_LIST_TR_POLY)
        drawTranslucentGeometry()
        kos.PvrListFinish()

        kos.PvrSceneFinish()
    }
}

func drawOpaqueGeometry() {
    // First submit a polygon header
    var hdr kos.PvrPolyHdr
    var ctx kos.PvrPolyCxt
    kos.PvrPolyCxtCol(&ctx, kos.PVR_LIST_OP_POLY)
    kos.PvrPolyCompile(&hdr, &ctx)
    kos.PvrPrim(&hdr)
    
    // Then submit vertices
    v := kos.PvrVertex{
        Flags: kos.PVR_CMD_VERTEX_EOL,  // End of strip
        X: 320, Y: 240, Z: 1,
        ARGB: 0xffffffff,
    }
    kos.PvrPrimVertex(&v)
}
```

## Further Reading

- [Design](design.md) — Runtime architecture
- [KOS Documentation](http://gamedev.allusion.net/docs/kos/) — Full API reference
- [PVR Tutorial](http://gamedev.allusion.net/softprj/kos/pvr-intro/) — Graphics programming
- `examples/` — Working code samples
