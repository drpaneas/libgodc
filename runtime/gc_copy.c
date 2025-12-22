#include "gc_semispace.h"
#include "type_descriptors.h"
#include "goroutine.h"
#include "runtime.h"
#include <string.h>
#include <arch/cache.h>
#include <arch/timer.h>
#include "dc_platform.h"

#define GC_PREFETCH(addr) __asm__ volatile("pref @%0" : : "r"(addr))

/* SH-4 store queue for large copies */
#define SQ_BASE 0xE0000000
#define SQ_MIN_SIZE 128

static void gc_sq_copy(void *dst, const void *src, size_t size)
{
    if (size < SQ_MIN_SIZE ||
        ((uintptr_t)dst & 31) != 0 ||
        ((uintptr_t)src & 31) != 0)
    {
        memcpy(dst, src, size);
        return;
    }

    uint32_t dst_addr = (uint32_t)(uintptr_t)dst;

    volatile uint32_t *QACR0 = (volatile uint32_t *)0xFF000038;
    volatile uint32_t *QACR1 = (volatile uint32_t *)0xFF00003C;
    *QACR0 = (dst_addr >> 26) & 0x1C;
    *QACR1 = (dst_addr >> 26) & 0x1C;

    uint32_t *sq = (uint32_t *)(SQ_BASE | (dst_addr & 0x03FFFFE0));
    const uint32_t *s = (const uint32_t *)src;

    size_t blocks = size / 32;
    size_t remainder = size & 31;

    while (blocks >= 2)
    {
        GC_PREFETCH(s + 16);

        sq[0] = s[0];
        sq[1] = s[1];
        sq[2] = s[2];
        sq[3] = s[3];
        sq[4] = s[4];
        sq[5] = s[5];
        sq[6] = s[6];
        sq[7] = s[7];
        __asm__ volatile("pref @%0" : : "r"(sq));
        s += 8;
        sq = (uint32_t *)((uintptr_t)sq + 32);

        sq[0] = s[0];
        sq[1] = s[1];
        sq[2] = s[2];
        sq[3] = s[3];
        sq[4] = s[4];
        sq[5] = s[5];
        sq[6] = s[6];
        sq[7] = s[7];
        __asm__ volatile("pref @%0" : : "r"(sq));
        s += 8;
        sq = (uint32_t *)((uintptr_t)sq + 32);
        blocks -= 2;
    }

    if (blocks > 0)
    {
        sq[0] = s[0];
        sq[1] = s[1];
        sq[2] = s[2];
        sq[3] = s[3];
        sq[4] = s[4];
        sq[5] = s[5];
        sq[6] = s[6];
        sq[7] = s[7];
        __asm__ volatile("pref @%0" : : "r"(sq));
        s += 8;
    }

    if (remainder > 0)
        memcpy((uint8_t *)dst + (size - remainder), s, remainder);

    __asm__ volatile("" : : : "memory");
}

static void *gc_saved_stack_lo;
static void *gc_saved_stack_hi;
static bool gc_stack_bounds_valid;

static void save_stack_bounds(void)
{
    G *gp = getg();
    if (gp && gp->stack_hi)
    {
        gc_saved_stack_lo = gp->stack_lo;
        gc_saved_stack_hi = gp->stack_hi;
        gc_stack_bounds_valid = true;
    }
    else
    {
        gc_saved_stack_lo = NULL;
        gc_saved_stack_hi = NULL;
        gc_stack_bounds_valid = false;
    }
}

static void *gc_copy_object(void *ptr);
static void gc_scan_object(void *obj);
static void gc_scan_roots(void);
static void gc_scan_stack(void);
static inline bool gc_validate_header(gc_header_t *header);
static bool gc_is_valid_object_start(void *ptr);
void gc_scan_range_conservative(void *start, size_t size);

void gc_collect(void)
{
    if (!gc_heap.initialized)
    {
        gc_init();
        return;
    }

    if (gc_heap.gc_in_progress)
        return;

    save_stack_bounds();

    uint64_t start_time = timer_us_gettime64();

    gc_heap.gc_in_progress = true;
    gc_heap.gc_count++;

    GODC_RUNTIME_ASSERT(gc_heap.active_space < 2, "active_space corrupt");
    GODC_RUNTIME_ASSERT(gc_heap.alloc_ptr >= gc_heap.space[gc_heap.active_space], "alloc_ptr corrupt");
    GODC_RUNTIME_ASSERT(gc_heap.alloc_ptr <= gc_heap.alloc_limit, "alloc_ptr overflow");

    int old_space = gc_heap.active_space;
    int new_space = 1 - old_space;

    gc_heap.active_space = new_space;
    gc_heap.alloc_ptr = gc_heap.space[new_space];
    gc_heap.alloc_limit = gc_heap.space[new_space] + gc_heap.space_size;
    gc_heap.scan_ptr = gc_heap.space[new_space];

    gc_scan_roots();

    /* Cheney's scan */
    while (gc_heap.scan_ptr < gc_heap.alloc_ptr)
    {
        gc_header_t *header = (gc_header_t *)gc_heap.scan_ptr;
        size_t obj_size = GC_HEADER_GET_SIZE(header);

        if (unlikely(obj_size < GC_HEADER_SIZE) ||
            unlikely(obj_size > gc_heap.space_size) ||
            unlikely(obj_size & GC_ALIGN_MASK))
            break;

        uint8_t *next_obj = gc_heap.scan_ptr + obj_size;
        if (next_obj < gc_heap.alloc_ptr)
            GC_PREFETCH(next_obj);

        void *obj = gc_get_user_ptr(header);
        gc_scan_object(obj);
        gc_heap.scan_ptr += obj_size;
    }

    size_t after_size = gc_heap.alloc_ptr - gc_heap.space[gc_heap.active_space];
    gc_heap.bytes_copied = after_size;
    gc_heap.bytes_allocated = after_size;

    /* Defer cache invalidation */
    gc_heap.pending_invalidate_space = gc_heap.space[old_space];
    gc_heap.invalidate_offset = 0;

    uint64_t elapsed = timer_us_gettime64() - start_time;
    gc_heap.last_pause_us = elapsed;
    gc_heap.total_pause_us += elapsed;

    gc_heap.gc_in_progress = false;
    gc_stack_bounds_valid = false;
}

/* GC inhibit for map operations that hold derived pointers */
volatile int gc_inhibit_count = 0;

void gc_inhibit_collection(void)
{
    gc_inhibit_count++;
}

void gc_allow_collection(void)
{
    gc_inhibit_count--;
}

void gc_collect_if_needed(size_t requested_size)
{
    extern int32_t gc_percent;

    if (gc_inhibit_count > 0)
        return;

    /* gc_percent < 0 disables automatic GC */
    if (gc_percent < 0)
        return;

    size_t remaining = gc_heap.alloc_limit - gc_heap.alloc_ptr;
    if (remaining < requested_size || remaining < gc_heap.space_size / 4)
        gc_collect();
}

bool gc_invalidate_incremental(void)
{
    if (!gc_heap.pending_invalidate_space)
        return false;

    const uintptr_t chunk_size = 64 * 1024;
    uintptr_t base = (uintptr_t)gc_heap.pending_invalidate_space;
    uintptr_t remaining = gc_heap.space_size - gc_heap.invalidate_offset;

    if (remaining == 0)
    {
        gc_heap.pending_invalidate_space = NULL;
        gc_heap.invalidate_offset = 0;
        return false;
    }

    uintptr_t to_invalidate = (remaining < chunk_size) ? remaining : chunk_size;
    dcache_inval_range(base + gc_heap.invalidate_offset, to_invalidate);
    gc_heap.invalidate_offset += to_invalidate;

    if (gc_heap.invalidate_offset >= gc_heap.space_size)
    {
        gc_heap.pending_invalidate_space = NULL;
        gc_heap.invalidate_offset = 0;
        return false;
    }
    return true;
}

bool gc_invalidation_pending(void)
{
    return gc_heap.pending_invalidate_space != NULL;
}

int gc_invalidate_on_vblank(uint32_t budget_us)
{
    if (!gc_heap.pending_invalidate_space)
        return 0;

    uint64_t deadline = timer_us_gettime64() + budget_us;

    while (gc_heap.pending_invalidate_space)
    {
        gc_invalidate_incremental();
        if (budget_us == 0 || timer_us_gettime64() >= deadline)
            break;
    }

    if (!gc_heap.pending_invalidate_space)
        return 0;

    const uintptr_t chunk_size = 64 * 1024;
    return (int)((gc_heap.space_size - gc_heap.invalidate_offset + chunk_size - 1) / chunk_size);
}

/* --- Object Copying --- */

/* Copy object from old space to new space, return new address. */
static void *gc_copy_object(void *ptr)
{
    if (ptr == NULL)
    {
        return NULL;
    }

    // Check if this is even a heap pointer
    uintptr_t addr = (uintptr_t)ptr;
    uint8_t *old_space = gc_heap.space[1 - gc_heap.active_space];

    // Must be in old (from) space
    if (addr < (uintptr_t)old_space ||
        addr >= (uintptr_t)(old_space + gc_heap.space_size))
    {
        // Not in from-space, might be external or already copied
        return ptr;
    }

    gc_header_t *header = gc_get_header(ptr);

    // Check if already forwarded
    if (GC_HEADER_IS_FORWARDED(header))
    {
        void *new_ptr = GC_HEADER_GET_FORWARD(header);
        return new_ptr;
    }

    /*
     * Header validation before copy.
     *
     * CRITICAL INSIGHT: For PRECISE scanning (type-directed), we trust the
     * type descriptor to tell us which fields are pointers. But the pointer
     * VALUES in those fields might be stale - pointing to memory that was
     * freed in a previous GC cycle.
     *
     * When this happens, the "header" we read might be:
     *   - All zeros (memory was never reallocated)
     *   - Small values like 1, 2, 3 (garbage from reused memory)
     *   - Unaligned sizes (definitely not a real header)
     *   - A forwarding pointer from a PREVIOUS GC (not current)
     *
     * In all these cases, the safe thing to do is SKIP this pointer.
     * Returning the original ptr means the field keeps its stale value,
     * which will either:
     *   1. Be overwritten before next use (common case)
     *   2. Cause a crash if dereferenced (bug in Go code, not GC)
     *
     * This is the same approach taken by Go's standard GC for non-heap
     * pointers found in pointer fields.
     */
    if (unlikely(!gc_validate_header(header)))
    {
        /*
         * Return unchanged for ANY invalid header.
         *
         * We used to crash on non-zero invalid headers, but testing shows
         * that stale pointers can point to memory containing any garbage.
         * The slice grow/shrink pattern produces exactly this:
         *   - Slice backing array is allocated
         *   - Slice is re-sliced to smaller length
         *   - GC runs, old backing array becomes garbage
         *   - New allocation reuses that memory
         *   - Next GC finds stale pointer to now-garbage memory
         *
         * Crashing is wrong - we should skip gracefully.
         */
        return ptr;
    }

    size_t obj_size = GC_HEADER_GET_SIZE(header);

    /*
     * DEFENSIVE ALIGNMENT FIX:
     *
     * The stored size SHOULD already be 8-byte aligned (GC_HEADER_SIZE +
     * gc_align_size(user_size)), but we enforce alignment here to be safe.
     * This catches any edge cases where objects were created through
     * non-standard paths.
     */
    size_t aligned_obj_size = (obj_size + GC_ALIGN_MASK) & ~GC_ALIGN_MASK;

    gc_header_t *new_header = (gc_header_t *)gc_heap.alloc_ptr;

    if (unlikely(gc_heap.alloc_ptr + aligned_obj_size > gc_heap.alloc_limit))
    {
        /*
         * To-space overflow is FATAL. We cannot return the old pointer
         * because it's in from-space which will be invalid after GC.
         * Returning ptr here would cause silent use-after-free corruption.
         *
         * If you hit this, your heap is too small for your live data.
         * Increase GC_SEMISPACE_SIZE_KB or reduce allocation rate.
         */
        runtime_throw(
            "GC to-space overflow - your live data doesn't fit in the heap.\n"
            "You have too much live data, not a GC bug. Options:\n"
            "  1. Increase GC_SEMISPACE_SIZE_KB (costs RAM)\n"
            "  2. Allocate less (reuse objects, use pools)\n"
            "  3. Call runtime.GC() more often to free garbage earlier");
    }

    /*
     * Use Store Queue for large object copies.
     * SQ gives ~3x speedup for objects > 128 bytes (textures, large arrays).
     * Small objects use regular memcpy (SQ setup overhead not worth it).
     *
     * Note: We copy obj_size bytes (the actual data) but advance alloc_ptr
     * by aligned_obj_size to maintain 8-byte alignment.
     */
    gc_sq_copy(new_header, header, obj_size);
    gc_heap.alloc_ptr += aligned_obj_size;

    // Calculate new user pointer
    void *new_ptr = gc_get_user_ptr(new_header);

    // Leave forwarding pointer in old object
    GC_HEADER_SET_FORWARD(header, new_ptr);

    return new_ptr;
}

/* --- Object Scanning --- */

/*
 * Update pointer field: copy if in from-space, update to new address.
 * Validates pointer is in valid RAM before following. Skips non-heap
 * pointers (interface{} type ptrs, .rodata, stack, etc).
 */
static void gc_update_pointer_field(void **field)
{
    void *old_ptr = *field;
    if (old_ptr == NULL)
    {
        return;
    }

    uintptr_t addr = (uintptr_t)old_ptr;

    /*
     * Reject pointers outside Dreamcast RAM.
     *
     * Valid Dreamcast RAM: 0x8c000000 - 0x8cffffff (16MB)
     * Also valid: 0xac000000 - 0xacffffff (P2 uncached mirror)
     *
     * Anything else is definitely not a heap pointer.
     */
    bool in_p1_ram = (addr >= 0x8c000000 && addr < 0x8d000000);
    bool in_p2_ram = (addr >= 0xac000000 && addr < 0xad000000);

    if (!in_p1_ram && !in_p2_ram)
    {
        return; /* Not a valid RAM address - skip */
    }

    /*
     * Fast-path bailout for non-heap pointers.
     *
     * Check if pointer is in from-space BEFORE calling gc_copy_object.
     * This is both an optimization (avoids function call) and a correctness
     * fix for interface{} type pointers.
     *
     * On Dreamcast, the binary is loaded at 0x8c010000, so .rodata and .data
     * are at lower addresses than the GC heap (which is allocated via
     * memalign from the malloc arena at higher addresses).
     *
     * This check will correctly skip:
     *   - Type pointers in interface{} (point to .rodata type descriptors)
     *   - Boxed small integers (point to staticuint64s in .data)
     *   - String constant data (in .rodata)
     *   - Stack pointers (on the stack, above heap)
     *   - External allocations (may be anywhere, but not in from-space)
     */
    uint8_t *from_space = gc_heap.space[1 - gc_heap.active_space];
    uintptr_t from_lo = (uintptr_t)from_space;
    uintptr_t from_hi = (uintptr_t)(from_space + gc_heap.space_size);

    if (addr < from_lo || addr >= from_hi)
    {
        /*
         * Pointer is NOT in from-space. Do not modify it.
         * This is the normal case for interface{} type pointers,
         * static data pointers, and stack pointers.
         */
        return;
    }

    /*
     * Pointer IS in from-space. This is a heap pointer that needs to
     * be copied/forwarded. Call gc_copy_object to handle it.
     */
    void *new_ptr = gc_copy_object(old_ptr);

    // Update the field if the object was moved
    if (new_ptr != old_ptr)
    {
        *field = new_ptr;
    }
}

/* De Bruijn CTZ table - SH-4 lacks hardware CTZ, this is O(1) without branching */
static const uint8_t ctz_debruijn32[32] = {
    0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
    31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9};

/* De Bruijn magic number */
#define DEBRUIJN32 0x077CB531U

/* Find position of lowest set bit using de Bruijn technique */
static inline int ctz32(uint32_t v)
{
    /* v must be non-zero */
    return ctz_debruijn32[((uint32_t)((v & -(int32_t)v) * DEBRUIJN32)) >> 27];
}

/*
 * Scan gcdata bitmap with 4-byte aligned reads, de Bruijn bit scanning.
 * O(popcount) iterations vs O(32) for sparse pointer layouts.
 */
static void gc_scan_gcdata_bitmap(void *obj, const uint8_t *gcdata, size_t nwords)
{
    size_t word_idx = 0;
    size_t byte_idx = 0;

    /*
     * Fast path: process 32 words (4 bytes of gcdata) at a time.
     * Only use aligned 4-byte reads.
     *
     * Uses de Bruijn technique: iterate only over SET bits, not all 32.
     * For a struct with 3 pointer fields out of 32 words, this does
     * 3 iterations instead of 32.
     *
     * Prefetch pointer targets before copying.
     * SH-4 has a ~10 cycle cache miss penalty. Prefetching hides this
     * by starting the memory fetch while we're still processing the
     * current pointer.
     */
    if (((uintptr_t)gcdata & 3) == 0)
    {
        /* gcdata is 4-byte aligned - use word reads */
        while (word_idx + 32 <= nwords)
        {
            uint32_t mask32 = *(const uint32_t *)&gcdata[byte_idx];

            /*
             * Prefetch phase - scan bitmask and prefetch targets.
             * We do a quick scan to prefetch all pointers in this chunk
             * BEFORE we start copying. This hides memory latency.
             */
            {
                uint32_t prefetch_mask = mask32;
                int prefetch_count = 0;
                const int MAX_PREFETCH = 4; /* Don't overflow prefetch queue */

                while (prefetch_mask != 0 && prefetch_count < MAX_PREFETCH)
                {
                    int bit = ctz32(prefetch_mask);
                    void **slot = (void **)((uint8_t *)obj +
                                            (word_idx + bit) * sizeof(void *));
                    void *ptr = *slot;
                    if (ptr != NULL)
                    {
                        GC_PREFETCH(ptr); /* Prefetch the object header */
                    }
                    prefetch_mask &= prefetch_mask - 1;
                    prefetch_count++;
                }
            }

            /* Process only set bits using isolate-lowest technique */
            while (mask32 != 0)
            {
                /* Find lowest set bit position */
                int bit = ctz32(mask32);

                void **slot = (void **)((uint8_t *)obj +
                                        (word_idx + bit) * sizeof(void *));
                gc_update_pointer_field(slot);

                /* Clear lowest set bit */
                mask32 &= mask32 - 1;
            }

            word_idx += 32;
            byte_idx += 4;
        }
    }

    /*
     * Medium path: process 8 words (1 byte of gcdata) at a time.
     * Byte reads are fine for the remainder. Use same de Bruijn technique
     * but with 8-bit mask.
     */
    while (word_idx + 8 <= nwords)
    {
        uint32_t mask = gcdata[byte_idx]; /* promote to 32-bit for ctz32 */

        while (mask != 0)
        {
            int bit = ctz32(mask);
            void **slot = (void **)((uint8_t *)obj +
                                    (word_idx + bit) * sizeof(void *));
            gc_update_pointer_field(slot);

            /* Clear lowest set bit */
            mask &= mask - 1;
        }

        word_idx += 8;
        byte_idx++;
    }

    /* Slow path: remaining words (0-7) - few enough that bit-by-bit is fine */
    while (word_idx < nwords)
    {
        if (gcdata[word_idx / 8] & (1U << (word_idx % 8)))
        {
            void **slot = (void **)((uint8_t *)obj + word_idx * sizeof(void *));
            gc_update_pointer_field(slot);
        }
        word_idx++;
    }
}

/* Scan element using gcdata bitmap (precise scanning). */
static void gc_scan_element_with_gcdata(void *obj, struct __go_type_descriptor *type)
{
    if (!type->__gcdata || type->__ptrdata == 0)
        return;

    /*
     * Validate gcdata pointer before dereferencing.
     *
     * gcdata lives in .rodata section (0x8c050000+ typically).
     * If it's outside valid RAM, the type descriptor is corrupted.
     */
    {
        uintptr_t gcdata_addr = (uintptr_t)type->__gcdata;
        bool in_p1_ram = (gcdata_addr >= 0x8c000000 && gcdata_addr < 0x8d000000);
        bool in_p2_ram = (gcdata_addr >= 0xac000000 && gcdata_addr < 0xad000000);

        if (!in_p1_ram && !in_p2_ram)
        {
            return;
        }
    }

    gc_scan_gcdata_bitmap(obj, type->__gcdata, type->__ptrdata / sizeof(void *));
}

// Scan a single element using its type descriptor
static void gc_scan_single_element(void *elem, struct __go_type_descriptor *type)
{
    if (!type || type->__ptrdata == 0)
    {
        return; // No pointers
    }

    /*
     * Validate type pointer before accessing its fields.
     *
     * Even though gc_scan_object validated the header's type pointer,
     * this function can be called from array iteration paths.
     */
    {
        uintptr_t type_addr = (uintptr_t)type;
        bool in_p1_ram = (type_addr >= 0x8c000000 && type_addr < 0x8d000000);
        bool in_p2_ram = (type_addr >= 0xac000000 && type_addr < 0xad000000);

        if (!in_p1_ram && !in_p2_ram)
        {
            return;
        }
    }

    // Check if type uses GC program (KindGCProg) instead of bitmap
    // GC programs are used for large types (typically >128 bytes of ptrdata)
    // For Dreamcast, fall back to conservative scan for these rare cases
    if (GC_TYPE_USES_GCPROG(type))
    {
        gc_scan_range_conservative(elem, type->__ptrdata);
        return;
    }

    // Use precise scanning with gcdata bitmap
    if (type->__gcdata)
    {
        gc_scan_element_with_gcdata(elem, type);
    }
    else
    {
        // No gcdata available - conservative scan
        gc_scan_range_conservative(elem, type->__ptrdata);
    }
}

// Scan an object for pointer fields
static void gc_scan_object(void *obj)
{
    gc_header_t *header = gc_get_header(obj);
    struct __go_type_descriptor *type = header->type;
    size_t obj_size = GC_HEADER_GET_SIZE(header) - GC_HEADER_SIZE;

    // Fast path: NOSCAN flag set during allocation (ptrdata == 0)
    if (GC_HEADER_IS_NOSCAN(header))
    {
        return; // No pointers to scan - skip entirely
    }

    // Case 1: No type info - conservative scan entire object
    if (!type)
    {
        gc_scan_range_conservative(obj, obj_size);
        return;
    }

    /*
     * Validate type pointer before dereferencing.
     *
     * The type descriptor pointer in the header might be stale garbage
     * if this object was incorrectly "copied" (actually a forwarded stale
     * pointer that we didn't catch earlier).
     *
     * Valid type descriptors live in .rodata or .data sections, which are
     * in the 0x8c000000+ range. Reject obviously invalid addresses.
     */
    {
        uintptr_t type_addr = (uintptr_t)type;
        bool in_p1_ram = (type_addr >= 0x8c000000 && type_addr < 0x8d000000);
        bool in_p2_ram = (type_addr >= 0xac000000 && type_addr < 0xad000000);

        if (!in_p1_ram && !in_p2_ram)
        {
            gc_scan_range_conservative(obj, obj_size);
            return;
        }
    }

    // Case 2: No pointers in this type
    if (type->__ptrdata == 0)
    {
        return; // "noscan" object
    }

    /* Case 3: Array allocation - obj_size > type->size means multiple elements */
    /* The compiler calls mallocgc with element type but allocates n * elem_size */
    if (type->__size > 0 && obj_size > type->__size)
    {
        size_t n_elements = obj_size / type->__size;

        /*
         * Sanity check: max elements we'll scan precisely.
         *
         * GC_SEMISPACE_SIZE / sizeof(void*) is the absolute maximum number
         * of pointers that could fit in one semispace. Any count beyond
         * that indicates a corrupted header. Fall back to conservative
         * scan which is slower but won't loop forever on garbage.
         */
        size_t max_sane_elements = gc_heap.space_size / sizeof(void *);
        if (n_elements > max_sane_elements)
        {
            gc_scan_range_conservative(obj, obj_size);
            return;
        }

        for (size_t i = 0; i < n_elements; i++)
        {
            void *elem = (uint8_t *)obj + i * type->__size;
            gc_scan_single_element(elem, type);
        }
        return;
    }

    /*
     * Case 4: Single object.
     *
     * gc_scan_single_element handles ALL sub-cases:
     * - Has gcdata bitmap: precise scan
     * - Uses GCProg: conservative scan of ptrdata region
     * - No gcdata: conservative scan of ptrdata region
     *
     * The old type-kind switch was dead code - we never reached it.
     */
    gc_scan_single_element(obj, type);
}

/* Header validation to reject invalid pointers during conservative scanning */

/* Validate header structure. Rejects false positives aggressively. */
static inline bool gc_validate_header(gc_header_t *header)
{
    size_t size = GC_HEADER_GET_SIZE(header);

    /* Size must be at least header, at most semispace */
    if (size < GC_HEADER_SIZE || size > gc_heap.space_size)
        return false;

    /* Size must be aligned to GC_ALIGN */
    if (size & GC_ALIGN_MASK)
        return false;

    /*
     * If forwarded, the forward pointer must be in to-space.
     * This catches stale pointers to objects that were already copied
     * in a previous scan pass (double-forwarding bug).
     */
    if (GC_HEADER_IS_FORWARDED(header))
    {
        uintptr_t fwd = (uintptr_t)GC_HEADER_GET_FORWARD(header);
        uint8_t *to_space = gc_heap.space[gc_heap.active_space];

        if (fwd < (uintptr_t)to_space ||
            fwd >= (uintptr_t)(to_space + gc_heap.space_size))
        {
            return false;
        }

        /* Forward pointer must also be aligned */
        if (fwd & GC_ALIGN_MASK)
            return false;
    }
    else
    {
        /*
         * Validate type pointer for non-forwarded objects.
         *
         * Type pointer must be either:
         *   - NULL (untyped allocation like map buckets)
         *   - In valid .rodata/.data section (type descriptors)
         *
         * Type descriptors are NEVER in the heap - they're static compile-time
         * structures in the binary. If the "type" field points into the heap,
         * this is a false positive and we should reject it.
         *
         * On Dreamcast:
         *   - Binary loads at 0x8C010000
         *   - .rodata/.data are near the binary start
         *   - Heap is allocated by memalign() at higher addresses
         *   - Stack grows down from near 0x8D000000
         *
         * If type is non-NULL but in heap range or stack range, it's garbage.
         */
        struct __go_type_descriptor *type = header->type;
        if (type != NULL)
        {
            uintptr_t type_addr = (uintptr_t)type;

            /*
             * Type descriptors are in the binary, which starts at 0x8C010000.
             * The heap is allocated dynamically and is typically at higher
             * addresses. The stack is near the top of RAM.
             *
             * Valid type addresses are roughly 0x8C010000 to 0x8C100000
             * (1MB for a typical binary). Reject anything in heap range.
             */
            uint8_t *space0 = gc_heap.space[0];
            uint8_t *space1 = gc_heap.space[1];
            uintptr_t heap0_lo = (uintptr_t)space0;
            uintptr_t heap0_hi = heap0_lo + gc_heap.space_size;
            uintptr_t heap1_lo = (uintptr_t)space1;
            uintptr_t heap1_hi = heap1_lo + gc_heap.space_size;

            /* Reject if type pointer is in either semispace */
            if ((type_addr >= heap0_lo && type_addr < heap0_hi) ||
                (type_addr >= heap1_lo && type_addr < heap1_hi))
            {
                return false;
            }

            /* Type pointer must be in valid RAM range */
            if (type_addr < 0x8C000000 || type_addr >= 0x8D000000)
            {
                return false;
            }

            /*
             * Type pointer should be reasonably aligned.
             * Type descriptors are structs with pointer-sized fields,
             * so they should be at least 4-byte aligned.
             */
            if (type_addr & 0x3)
            {
                return false;
            }
        }
    }

    return true;
}

/* Check if pointer is to a valid object start, rejecting interior pointers. */
static bool gc_is_valid_object_start(void *ptr)
{
    if (!ptr)
        return false;

    uintptr_t addr = (uintptr_t)ptr;
    uint8_t *old_space = gc_heap.space[1 - gc_heap.active_space];

    /* Must be in from-space with room for header before it */
    if (addr < (uintptr_t)(old_space + GC_HEADER_SIZE) ||
        addr >= (uintptr_t)(old_space + gc_heap.space_size))
    {
        return false;
    }

    /* Check alignment - all GC objects are 8-byte aligned */
    if (addr & GC_ALIGN_MASK)
        return false;

    /* Get the header and validate its structure */
    gc_header_t *header = gc_get_header(ptr);

    if (!gc_validate_header(header))
        return false;

    size_t obj_size = GC_HEADER_GET_SIZE(header);

    /*
     * Interior pointer rejection.
     *
     * Calculate where the object SHOULD end based on header size.
     * The user pointer (ptr) is at header + GC_HEADER_SIZE.
     * Object data size is obj_size - GC_HEADER_SIZE.
     *
     * For a valid object start, header + obj_size should equal
     * the object end, which must be within the heap.
     */
    uintptr_t obj_end = (uintptr_t)header + obj_size;
    if (obj_end > (uintptr_t)(old_space + gc_heap.space_size))
        return false;

    /*
     * Additional interior pointer check.
     *
     * Walk backwards from this address to see if there's a "better"
     * header that claims to own this address. If so, reject.
     *
     * This is expensive so we only do it for suspicious cases:
     * objects that seem very small relative to their claimed position.
     *
     * Heuristic: if claimed object size is less than 64 bytes,
     * and we're far into the heap, double-check.
     *
     * Disabled for now - adds ~10 cycles and most corruption is caught
     * by the validation above. Enable with GODC_PARANOID if needed.
     */
#ifdef GODC_PARANOID
    if (obj_size < 64)
    {
        /* Check if a previous object overlaps this address */
        uintptr_t check_addr = (uintptr_t)header - GC_HEADER_SIZE;
        if (check_addr >= (uintptr_t)old_space)
        {
            gc_header_t *prev = (gc_header_t *)check_addr;
            size_t prev_size = GC_HEADER_GET_SIZE(prev);

            if (prev_size >= GC_HEADER_SIZE &&
                prev_size <= gc_heap.space_size &&
                (prev_size & GC_ALIGN_MASK) == 0)
            {
                /* Previous "header" claims to be valid */
                uintptr_t prev_end = check_addr + prev_size;
                if (prev_end > (uintptr_t)header)
                {
                    /* Previous object would overlap - reject as interior */
                    return false;
                }
            }
        }
    }
#endif

    return true;
}

/*
 * 8x unrolled conservative scan. Loads 8 words, filters against heap bounds,
 * only calls update for actual heap pointers. ~8x fewer function calls than
 * naive loop. SH-4: prefetch hides ~10 cycle cache miss penalty.
 */

/* Heap bounds - cached for inner loop (avoid repeated gc_heap access) */
typedef struct
{
    uintptr_t lo; /* Lowest valid heap address */
    uintptr_t hi; /* One past highest valid heap address */
} heap_bounds_t;

/*
 * Inline heap range check - the HOT filter.
 *
 * Returns true if value MIGHT be a heap pointer (needs full validation).
 * Returns false if value DEFINITELY is NOT a heap pointer (fast reject).
 *
 * This is the first filter - catches 90%+ of non-pointers.
 */
static inline bool gc_might_be_heap_ptr(uintptr_t val, heap_bounds_t bounds)
{
    /*
     * Single comparison trick.
     *
     * Instead of: val >= lo && val < hi
     * We use:     (val - lo) < (hi - lo)
     *
     * This works because if val < lo, the subtraction wraps around to a
     * huge positive number, which fails the < check. Two comparisons
     * become one subtraction + one comparison.
     *
     * Also checks alignment in the same expression.
     */
    return ((val - bounds.lo) < (bounds.hi - bounds.lo)) &&
           ((val & GC_ALIGN_MASK) == 0);
}

/*
 * gc_scan_range_conservative - Scan memory range for potential heap pointers.
 *
 * Treats every aligned pointer-sized word as a potential pointer.
 * Used for stack scanning and untyped memory regions.
 *
 * 8x unrolled with batched heap filtering for optimal performance.
 */
void gc_scan_range_conservative(void *start, size_t size)
{
    void **p, **end;
    uint8_t *old_space;
    heap_bounds_t bounds;

    uintptr_t start_addr = (uintptr_t)start;
    if (start_addr < DC_RAM_START || start_addr >= DC_RAM_END)
        return;
    if (start_addr + size > DC_RAM_END)
        size = DC_RAM_END - start_addr;

    size &= ~(sizeof(void *) - 1);
    if (size == 0)
        return;

    old_space = gc_heap.space[1 - gc_heap.active_space];
    p = (void **)start;
    end = (void **)((uint8_t *)start + size);

    /*
     * Cache heap bounds in local struct.
     * Avoids repeated gc_heap global access in the hot loop.
     */
    bounds.lo = (uintptr_t)old_space;
    bounds.hi = (uintptr_t)(old_space + gc_heap.space_size);

    /*
     * 8x unrolled loop with batched filtering.
     *
     * The magic: we read 8 values into local variables, then filter them
     * through the fast heap check. Only values that pass the filter go
     * through the expensive gc_is_valid_object_start() check.
     *
     * On a typical stack:
     *   - ~90% of words are NOT in heap range (fast reject)
     *   - ~9% are in range but not valid objects (misaligned, interior)
     *   - ~1% are actual heap pointers (call update)
     *
     * This means 8 reads, ~0.8 full checks, ~0.08 updates per iteration.
     */
    while (p + 8 <= end)
    {
        /* Prefetch next iteration's data (2 cache lines = 64 bytes) */
        GC_PREFETCH(p + 16);

        uintptr_t v0 = (uintptr_t)p[0];
        uintptr_t v1 = (uintptr_t)p[1];
        uintptr_t v2 = (uintptr_t)p[2];
        uintptr_t v3 = (uintptr_t)p[3];
        uintptr_t v4 = (uintptr_t)p[4];
        uintptr_t v5 = (uintptr_t)p[5];
        uintptr_t v6 = (uintptr_t)p[6];
        uintptr_t v7 = (uintptr_t)p[7];

        /*
         * Batched heap check with fast reject.
         *
         * Each check: ~3 cycles if rejected (common), ~30+ cycles if accepted.
         * The gc_is_valid_object_start is expensive but rarely called.
         */
        if (v0 && gc_might_be_heap_ptr(v0, bounds) &&
            gc_is_valid_object_start((void *)v0))
            gc_update_pointer_field(&p[0]);

        if (v1 && gc_might_be_heap_ptr(v1, bounds) &&
            gc_is_valid_object_start((void *)v1))
            gc_update_pointer_field(&p[1]);

        if (v2 && gc_might_be_heap_ptr(v2, bounds) &&
            gc_is_valid_object_start((void *)v2))
            gc_update_pointer_field(&p[2]);

        if (v3 && gc_might_be_heap_ptr(v3, bounds) &&
            gc_is_valid_object_start((void *)v3))
            gc_update_pointer_field(&p[3]);

        if (v4 && gc_might_be_heap_ptr(v4, bounds) &&
            gc_is_valid_object_start((void *)v4))
            gc_update_pointer_field(&p[4]);

        if (v5 && gc_might_be_heap_ptr(v5, bounds) &&
            gc_is_valid_object_start((void *)v5))
            gc_update_pointer_field(&p[5]);

        if (v6 && gc_might_be_heap_ptr(v6, bounds) &&
            gc_is_valid_object_start((void *)v6))
            gc_update_pointer_field(&p[6]);

        if (v7 && gc_might_be_heap_ptr(v7, bounds) &&
            gc_is_valid_object_start((void *)v7))
            gc_update_pointer_field(&p[7]);

        p += 8;
    }

    /*
     * 4x unrolled cleanup for 4-7 remaining words.
     * Don't fall through to 1x loop for small remainders.
     */
    if (p + 4 <= end)
    {
        uintptr_t v0 = (uintptr_t)p[0];
        uintptr_t v1 = (uintptr_t)p[1];
        uintptr_t v2 = (uintptr_t)p[2];
        uintptr_t v3 = (uintptr_t)p[3];

        if (v0 && gc_might_be_heap_ptr(v0, bounds) &&
            gc_is_valid_object_start((void *)v0))
            gc_update_pointer_field(&p[0]);
        if (v1 && gc_might_be_heap_ptr(v1, bounds) &&
            gc_is_valid_object_start((void *)v1))
            gc_update_pointer_field(&p[1]);
        if (v2 && gc_might_be_heap_ptr(v2, bounds) &&
            gc_is_valid_object_start((void *)v2))
            gc_update_pointer_field(&p[2]);
        if (v3 && gc_might_be_heap_ptr(v3, bounds) &&
            gc_is_valid_object_start((void *)v3))
            gc_update_pointer_field(&p[3]);

        p += 4;
    }

    /* Handle final 0-3 words (rare, not worth unrolling further) */
    while (p < end)
    {
        uintptr_t v = (uintptr_t)*p;
        if (v && gc_might_be_heap_ptr(v, bounds) &&
            gc_is_valid_object_start((void *)v))
            gc_update_pointer_field(p);
        p++;
    }
}

/* --- Root Scanning --- */

static void gc_scan_root_variable(gc_root_t *root)
{
    if (!root->decl || root->ptrdata == 0)
        return;

    if (root->gcdata)
    {
        /* Use precise scanning with gcdata bitmap (optimized for SH-4) */
        gc_scan_gcdata_bitmap(root->decl, root->gcdata,
                              root->ptrdata / sizeof(void *));
    }
    else
    {
        /* Fallback to conservative scan */
        gc_scan_range_conservative(root->decl, root->ptrdata);
    }
}

// Scan compiler-registered global roots
static void gc_scan_compiler_roots(void)
{
    int total_roots = 0;
    int total_lists = 0;

    for (gc_root_list_t *list = gc_global_roots; list != NULL; list = list->next)
    {
        total_lists++;
        for (size_t i = 0; i < list->count; i++)
        {
            gc_scan_root_variable(&list->roots[i]);
            total_roots++;
        }
    }

    // Always print root scan stats (critical for debugging)
}

/* allgs_iterate and allgs_get_count declared in goroutine.h */

/**
 * Scan a single goroutine's stack segments.
 *
 * Use saved SP from context for precise scanning.
 *
 * When a goroutine yields, its SP is saved in gp->context.sp. We should scan
 * from this SP to the top of the stack (stack grows DOWN on SH-4).
 *
 * BEFORE: Scanned entire stack segment including uninitialized memory.
 *         Uninitialized memory could contain garbage that looks like heap
 *         pointers, causing false positives in conservative scanning.
 *
 * AFTER:  Scan only the used portion (from saved SP to stack top).
 *         This is both faster AND safer.
 */
static void gc_scan_goroutine_stack(G *gp)
{
    if (gp == NULL)
        return;

    /* Get goroutine's current stack segment from G struct */
    stack_segment_t *seg = gp->stack;

    while (seg != NULL)
    {
        void *stack_lo = seg->base;
        void *stack_hi = (void *)((uintptr_t)seg->base + seg->size);

        /*
         * Validate segment structure. If these are corrupt, we have a
         * serious bug - either the allocator is broken or memory got
         * stomped. Don't try to scan corrupt data.
         */
        if (seg->base == NULL || seg->size == 0)
        {
            break;
        }

        /*
         * Determine the starting point for stack scan.
         *
         * Priority:
         * 1. sp_on_entry (from __morestack, if split-stack enabled)
         * 2. gp->context.sp (saved SP from swapcontext - normal yield)
         * 3. Entire segment (fallback, shouldn't happen)
         *
         * The saved SP tells us where the used portion of the stack starts.
         * We only scan from SP to stack_hi (used portion).
         */
        uintptr_t sp = 0;
        uintptr_t lo = (uintptr_t)seg->base;
        uintptr_t hi = (uintptr_t)stack_hi;

        if (seg->sp_on_entry != NULL)
        {
            /* Split-stack: use sp_on_entry */
            sp = (uintptr_t)seg->sp_on_entry;
        }
        else if (gp->context.sp != 0)
        {
            /*
             * Normal yield: use saved context SP.
             *
             * When a goroutine yields via swapcontext, its SP is saved in
             * gp->context.sp. This is the precise boundary between used and
             * unused stack memory.
             */
            sp = (uintptr_t)gp->context.sp;
        }

        if (sp != 0)
        {
            /*
             * Sanity check: SP must be within segment bounds.
             * Stack grows down, so SP should be between base and base+size.
             */
            if (sp < lo || sp > hi)
            {
                seg = seg->prev;
                continue;
            }

            /* Scan from SP to top of stack (used portion only) */
            size_t used = hi - sp;
            gc_scan_range_conservative((void *)sp, used);
        }
        else
        {
            /*
             * Fallback: No SP available.
             *
             * This shouldn't happen for yielded goroutines, but might for
             * goroutines that haven't started yet or are in unusual states.
             * Scan the entire segment to be safe.
             */
            gc_scan_range_conservative(stack_lo, seg->size);
        }

        seg = seg->prev;
    }
}

/*
 * Scan all goroutine stacks.
 *
 * Uses array-based iteration for cache-friendly access.
 * This is a significant win during GC - sequential memory access instead
 * of pointer chasing through scattered G structs.
 */
extern G *allgs_iterate(int index);
extern int allgs_get_count(void);

static void gc_scan_all_goroutine_stacks(void)
{
    // Verify GC invariant: we should be in a GC context
    if (!gc_heap.gc_in_progress)
    {
    }

    /*
     * Get current goroutine to skip it.
     *
     * The current goroutine's stack is already scanned by gc_scan_stack()
     * which uses the actual SP register. We must NOT also scan it here
     * with the stale saved SP from context, as that could cause issues.
     */
    G *current = getg();

    int count = 0;
    int total = allgs_get_count();

    /* Array-based iteration - cache-friendly sequential access */
    for (int i = 0; i < total; i++)
    {
        G *gp = allgs_iterate(i);
        if (!gp)
            continue;

        // Skip dead goroutines - their stacks may be invalid
        if (gp->atomicstatus == Gdead)
        {
            continue;
        }

        // Skip current goroutine - already scanned by gc_scan_stack()
        if (gp == current)
        {
            // Still scan the G struct itself for pointers
            gc_scan_range_conservative(gp, sizeof(G));
            continue;
        }

        // Scan this goroutine's stack
        gc_scan_goroutine_stack(gp);
        count++;

        // Also scan the G struct itself for pointers (defer chain, panic, etc.)
        gc_scan_range_conservative(gp, sizeof(G));
    }
}

/*
 * Root scanning uses compiler-provided gcdata (precise), not conservative
 * .data/.bss scan. gc_scan_globals() was removed - redundant with registerGCRoots().
 */
static void gc_scan_roots(void)
{

    // Scan explicit roots (gc_add_root)
    for (int i = 0; i < gc_root_table.count; i++)
    {
        void **root = gc_root_table.roots[i];
        if (root && *root)
        {
            gc_update_pointer_field(root);
        }
    }

    // Scan compiler-registered roots (registerGCRoots)
    // This provides PRECISE type information - no conservative fallback needed
    gc_scan_compiler_roots();

    // Scan current stack (main goroutine or current goroutine)
    gc_scan_stack();

    // Scan all goroutine stacks
    gc_scan_all_goroutine_stacks();
}

/*
 * NOTE: gc_scan_registers was removed. It couldn't update registers after
 * copying, so it was useless. Stack scanning catches spilled registers.
 */

static void gc_scan_stack(void)
{
    void *sp;
    __asm__ volatile("mov r15, %0" : "=r"(sp));

    /*
     * Stack bounds were saved at gc_collect() entry.
     * If invalid, getg() returned NULL - probably called from
     * wrong context (before scheduler init, or from interrupt).
     */
    if (!gc_stack_bounds_valid)
    {
        runtime_throw("gc_scan_stack: no valid goroutine context");
    }

    void *stack_hi = gc_saved_stack_hi;
    void *stack_lo = gc_saved_stack_lo;

    if (!stack_hi)
        runtime_throw("gc_scan_stack: stack_hi is NULL");

    /* Validate stack_lo is sane */
    if ((uintptr_t)stack_lo < DC_RAM_START ||
        (uintptr_t)stack_lo >= (uintptr_t)stack_hi)
    {
        runtime_throw("gc_scan_stack: invalid stack bounds");
    }

    /* Clamp to Dreamcast RAM upper bound */
    if ((uintptr_t)stack_hi > DC_RAM_END)
        stack_hi = (void *)DC_RAM_END;

    if ((uintptr_t)sp >= (uintptr_t)stack_hi)
    {
        return;
    }

    size_t stack_size = (uintptr_t)stack_hi - (uintptr_t)sp;

    if (stack_size > GC_STACK_SCAN_MAX)
    {
        stack_size = GC_STACK_SCAN_MAX;
    }

    gc_scan_range_conservative(sp, stack_size);
}
