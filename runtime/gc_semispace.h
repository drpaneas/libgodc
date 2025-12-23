/* libgodc/runtime/gc_semispace.h - copying GC for Dreamcast
 *
 * WARNING: This GC moves objects. Hardware DMA pointers become stale after
 * collection. Use pvr_mem_malloc() for textures, or disable GC during DMA.
 */
#ifndef GC_SEMISPACE_H
#define GC_SEMISPACE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "godc_config.h"

struct __go_type_descriptor;

void runtime_throw(const char *s) __attribute__((noreturn));

#ifdef GC_SEMISPACE_SIZE_KB
#define GC_SEMISPACE_SIZE ((GC_SEMISPACE_SIZE_KB) * 1024)
#else
#define GC_SEMISPACE_SIZE (2 * 1024 * 1024)
#endif
#define GC_TOTAL_HEAP_SIZE (2 * GC_SEMISPACE_SIZE)

#ifdef GC_LARGE_OBJECT_THRESHOLD_KB
#define GC_LARGE_OBJECT_THRESHOLD ((GC_LARGE_OBJECT_THRESHOLD_KB) * 1024)
#else
#define GC_LARGE_OBJECT_THRESHOLD (64 * 1024) // Default: 64KB
#endif

// Alignment: 8 bytes for Go compatibility
#define GC_ALIGN 8
#define GC_ALIGN_MASK (GC_ALIGN - 1)

// Object header size: 8 bytes
#define GC_HEADER_SIZE 8

// Minimum object size (header only)
#define GC_MIN_OBJECT_SIZE GC_HEADER_SIZE

/* Object header layout (8 bytes)
//
// During normal operation:
// ┌──────────────────────────────────────────────────────────┐
// │  Bits 31-24: type_tag (8 bits) - index into type table  │
// │  Bits 23-0:  size (24 bits) - object size in bytes      │
// ├──────────────────────────────────────────────────────────┤
// │  type_ptr (32 bits) - pointer to type descriptor        │
// └──────────────────────────────────────────────────────────┘
//
// During GC (forwarded object):
// ┌──────────────────────────────────────────────────────────┐
// │  Bits 31: 1 (forwarding bit set)                        │
// │  Bits 30-0: (original size preserved for debugging)     │
// ├──────────────────────────────────────────────────────────┤
// │  forwarding_ptr (full 32-bit pointer)                   │
// └──────────────────────────────────────────────────────────┘
//
// NOTE: Dreamcast RAM addresses (0x8C000000-0x8E000000) have bit 31 set,
// so we CANNOT pack the forwarding pointer into size_and_flags. Instead,
// we store it in the type field (which is unused after forwarding).

// Header flags (bits 24-31 in size_and_flags)
// Layout: [31:forwarded][30:noscan][29-24:tag][23-0:size]
//
// Bit 31: FORWARDED - object has been copied, type field contains forward ptr
// Bit 30: NOSCAN    - object contains no pointers (ptrdata == 0)
// Bits 24-29: TYPE_TAG - 6 bits for Go type kind (64 values, Go has ~27 kinds)
// Bits 0-23: SIZE - object size including header (max 16MB)
 */
#define GC_HEADER_FORWARDED 0x80000000 // Bit 31: object has been forwarded
#define GC_HEADER_NOSCAN 0x40000000    // Bit 30: object contains no pointers
#define GC_HEADER_TYPE_MASK 0x3F000000 // Bits 24-29: 6-bit type tag
#define GC_HEADER_SIZE_MASK 0x00FFFFFF // Bits 0-23: size (max 16MB)

// Go type kind flags (from typekind.go)
// The raw kind value fits in 5 bits (0-31), but __code may have flags in bits 5-6
#define GC_KIND_MASK 0x1F         // Low 5 bits are the actual kind (kindBool..kindUnsafePointer)
#define GC_KIND_DIRECT_IFACE 0x20 // Bit 5: direct interface
#define GC_KIND_GCPROG 0x40       // Bit 6: type uses GC program (not bitmap)

// Check if type uses GC program instead of bitmap
#define GC_TYPE_USES_GCPROG(type) \
    ((type) && ((type)->__code & GC_KIND_GCPROG))

// Header manipulation macros
#define GC_HEADER_GET_SIZE(h) ((h)->size_and_flags & GC_HEADER_SIZE_MASK)
#define GC_HEADER_GET_TAG(h) (((h)->size_and_flags & GC_HEADER_TYPE_MASK) >> 24)
#define GC_HEADER_IS_FORWARDED(h) ((h)->size_and_flags & GC_HEADER_FORWARDED)
#define GC_HEADER_IS_NOSCAN(h) ((h)->size_and_flags & GC_HEADER_NOSCAN)
#define GC_HEADER_SET_NOSCAN(h) ((h)->size_and_flags |= GC_HEADER_NOSCAN)

// Forwarding pointer is stored in the type field (reused during GC)
#define GC_HEADER_GET_FORWARD(h) ((void *)((h)->type))

// Set header: tag is masked to 6 bits, size to 24 bits
// Note: FORWARDED and NOSCAN bits are cleared by this macro
#define GC_HEADER_SET(h, tag, size) \
    ((h)->size_and_flags = (((uint32_t)(tag) & 0x3F) << 24) | ((size) & GC_HEADER_SIZE_MASK))

// Store forwarding pointer in type field, set forwarding flag in size_and_flags
#define GC_HEADER_SET_FORWARD(h, ptr)                     \
    do                                                    \
    {                                                     \
        (h)->size_and_flags |= GC_HEADER_FORWARDED;       \
        (h)->type = (struct __go_type_descriptor *)(ptr); \
    } while (0)

// Object header structure
typedef struct gc_header
{
    uint32_t size_and_flags;           // Size + type tag + flags
    struct __go_type_descriptor *type; // Full type descriptor pointer
} gc_header_t;

// Verify header size matches GC_HEADER_SIZE constant
_Static_assert(sizeof(gc_header_t) == 8, "gc_header_t must be 8 bytes");
_Static_assert(sizeof(gc_header_t) == GC_HEADER_SIZE,
               "gc_header_t size must match GC_HEADER_SIZE");

// Get header from user pointer
static inline gc_header_t *gc_get_header(void *ptr)
{
    return (gc_header_t *)((uint8_t *)ptr - GC_HEADER_SIZE);
}

// Get user pointer from header
static inline void *gc_get_user_ptr(gc_header_t *header)
{
    return (void *)((uint8_t *)header + GC_HEADER_SIZE);
}

/* Semi-Space Heap */
typedef struct gc_heap
{
    // Semi-spaces (allocated from KOS malloc)
    uint8_t *space[2]; // Two semi-spaces
    int active_space;  // Currently active space (0 or 1)

    // Bump allocator state
    uint8_t *alloc_ptr;   // Next allocation point
    uint8_t *alloc_limit; // End of active space

    // Scan pointer for Cheney's algorithm
    uint8_t *scan_ptr; // For copying collection

    // Statistics
    size_t space_size;            // Size of each semi-space
    size_t bytes_allocated;       // Bytes currently in use (reset after GC)
    size_t total_bytes_allocated; // Cumulative bytes ever allocated (never decreases)
    uint64_t total_alloc_count;   // Cumulative allocation count (for MemStats.Mallocs)
    size_t bytes_copied;          // Bytes copied in last GC
    uint32_t gc_count;            // Number of collections

    // GC pause timing (microseconds)
    uint64_t last_pause_us;  // Last GC pause duration
    uint64_t total_pause_us; // Total time spent in GC

    // Large object tracking
    uint32_t large_alloc_count;
    size_t large_alloc_total;

    // State flags
    bool initialized;
    bool gc_in_progress;

    /*
     * Incremental cache invalidation state.
     *
     * Instead of invalidating the entire old space (2MB) in one shot at the
     * end of GC, we spread the work across multiple frames. This avoids a
     * potential ~20ms stall that could cause frame drops.
     *
     * The scheduler calls gc_invalidate_incremental() which invalidates
     * a small chunk (e.g., 64KB) per call. Games can also call this
     * directly during vblank for predictable timing.
     */
    void *pending_invalidate_space; // Old space awaiting invalidation
    uintptr_t invalidate_offset;    // Current offset within that space
} gc_heap_t;

// Global heap instance
extern gc_heap_t gc_heap;

/* Root Management */

// Maximum number of explicit root pointers
#define GC_MAX_ROOTS 256

// Root table for explicit roots (gc_add_root/gc_remove_root)
typedef struct gc_roots
{
    void **roots[GC_MAX_ROOTS]; // Pointers to root locations
    int count;                  // Number of registered roots
} gc_roots_t;

extern gc_roots_t gc_root_table;

/* Compiler-Generated Root Lists (registerGCRoots) */

// Single GC root entry (matches gccgo's gcRoot)
typedef struct gc_root
{
    void *decl;            // Pointer to the variable
    uintptr_t size;        // Size of variable
    uintptr_t ptrdata;     // Length of pointer data
    const uint8_t *gcdata; // Pointer mask bitmap
} gc_root_t;

// List of roots from a package (matches gccgo's gcRootList)
typedef struct gc_root_list
{
    struct gc_root_list *next; // Link to next list
    size_t count;              // Number of roots in this list
    gc_root_t roots[];         // Flexible array of roots
} gc_root_list_t;

// Head of compiler-generated root lists
extern gc_root_list_t *gc_global_roots;

// Register roots (called by compiler-generated init code)
void registerGCRoots(gc_root_list_t *roots);

/* All zero-size allocations return this address */
extern uint8_t gc_zerobase[8];

/* Core API */

// Initialization
void gc_init(void);

// Allocation (fast bump allocator)
//
// These functions throw on failure (runtime_throw), so they always return
// a valid pointer. The returns_nonnull attribute helps the compiler:
// - Skip NULL checks after allocation
// - Warn if code pointlessly checks for NULL
// The malloc attribute enables alias analysis optimizations.
//
void *gc_alloc(size_t size, struct __go_type_descriptor *type)
    __attribute__((returns_nonnull, malloc, warn_unused_result));
void *gc_alloc_no_gc(size_t size, struct __go_type_descriptor *type)
    __attribute__((returns_nonnull, malloc, warn_unused_result));

/* Collection (Cheney's algorithm) */
void gc_collect(void);
void gc_collect_if_needed(size_t requested_size);

/* GC inhibit for map operations (prevents moving GC during critical sections) */
void gc_inhibit_collection(void);
void gc_allow_collection(void);

/*
 * gc_invalidate_incremental - Process a chunk of deferred cache invalidation.
 *
 * After GC, the old space needs cache invalidation so RAM reads are fresh
 * next cycle. Instead of doing this all at once (~20ms for 2MB), we spread
 * it across frames.
 *
 * Call this from:
 * - The scheduler idle loop (automatic)
 * - Your game's vblank handler for predictable timing
 *
 * @return true if there's more work to do, false if done
 */
bool gc_invalidate_incremental(void);

/*
 * gc_invalidate_on_vblank - Process invalidation during vertical blank.
 *
 * Call this from your vblank interrupt handler or vblank callback.
 * Processes invalidation chunks while there's vblank time remaining.
 * This hides the invalidation latency completely by doing work when
 * the GPU is refreshing the display and the CPU would otherwise be idle.
 *
 * Usage:
 *     void my_vblank_handler(void) {
 *         gc_invalidate_on_vblank(2000);  // Use up to 2ms of vblank
 *         // ... other vblank work ...
 *     }
 *
 * @param budget_us  Maximum microseconds to spend (0 = one chunk only)
 * @return           Remaining chunks to process (0 = all done)
 */
int gc_invalidate_on_vblank(uint32_t budget_us);

/*
 * gc_invalidation_pending - Check if there's pending cache invalidation.
 *
 * Use this to decide whether to call gc_invalidate_on_vblank().
 *
 * @return true if invalidation work is pending
 */
bool gc_invalidation_pending(void);

// Root management
void gc_add_root(void **root_ptr);
void gc_remove_root(void **root_ptr);

// Statistics
void gc_stats(size_t *used, size_t *total, uint32_t *collections);

/* gccgo Runtime Interface */

// These functions implement the gccgo runtime allocation interface
// They wrap gc_alloc() with proper type handling

void *runtime_newobject(struct __go_type_descriptor *type);
void *runtime_mallocgc(size_t size, struct __go_type_descriptor *type, bool needzero);
void *runtime_makeslice(struct __go_type_descriptor *elem_type, intptr_t len, intptr_t cap);

// Map and channel allocation declarations are in map_dreamcast.c

/* External Allocation (non-GC objects) */

// For large buffers, textures, audio, etc. that shouldn't be in GC heap
// These use KOS malloc directly

void *gc_external_alloc(size_t size);
void gc_external_free(void *ptr);

#if GC_DEBUG
void gc_verify_heap(void);
void gc_dump_object(void *ptr);
void gc_dump_heap(int max_objects);
#else
#define gc_verify_heap() ((void)0)
#define gc_dump_object(p) ((void)0)
#define gc_dump_heap(n) ((void)0)
#endif

#endif // GC_SEMISPACE_H
