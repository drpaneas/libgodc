#include "goroutine.h"
#include "runtime.h"
#include "type_descriptors.h"

#include <kos.h>
#include <arch/arch.h>
#include <arch/rtc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// External functions from our implementation
// Most are declared in runtime.h and gc_semispace.h now
#include "gc_semispace.h"

// Dreamcast-specific allocation limit
// Fail early with a clear message instead of exhausting memory
#define DC_MAX_ALLOC_SIZE (8 * 1024 * 1024) // 8MB - half of Dreamcast RAM

// Slice alias for local use (GoSlice is defined in runtime.h)
typedef GoSlice Slice;

// Interface types are now defined in runtime.h

// ===== Memory Allocation Entry Points =====

// Called by gccgo for new(T) - defined in gc_runtime.c
// This stub just provides an alternate symbol if needed

// General allocation with type info - wrapper for compatibility
void *runtime_mallocgc_typed(uintptr_t size, void *typ, uint32_t flag)
{
    (void)flag; // gc_alloc always zeros
    return gc_alloc(size, (struct __go_type_descriptor *)typ);
}

// Allocate without type info (used for some internal allocations)
// This is already defined in malloc_dreamcast.c
// void *runtime_malloc(uintptr_t size) {
//     return runtime_mallocgc(size, true);
// }

// ===== Slice Operations =====
// Note: Slice creation is handled by runtime_makeslice in gc_runtime.c

// Grow a slice - gccgo calls this as runtime.growslice
// Note: gccgo passes the ELEMENT type, not the slice type
//
// gccgo signature (from go/runtime/slice.go:180):
//   func growslice(et *_type, oldarray unsafe.Pointer, oldlen, oldcap, cap int) slice
//
// Parameters are unpacked: oldarray, oldlen, oldcap instead of Slice struct
Slice runtime_growslice(void *elem_type, void *oldarray, intptr_t oldlen, intptr_t oldcap, intptr_t cap) __asm__("_runtime.growslice");
Slice runtime_growslice(void *elem_type, void *oldarray, intptr_t oldlen, intptr_t oldcap, intptr_t cap)
{
    struct __go_type_descriptor *et = (struct __go_type_descriptor *)elem_type;
    size_t elem_size = et ? et->__size : 0;

    // Handle zero-size elements
    if (elem_size == 0)
    {
        elem_size = 1;
    }

    // Empty slice to return on error (if panic is recovered)
    Slice empty_slice = {NULL, 0, 0};

    // Validate cap (requested capacity)
    if (cap < 0)
    {
        runtime_panicstring("growslice: cap out of range");
        return empty_slice; // Return safe value if panic recovered
    }

    // Calculate new capacity using Go's growth algorithm
    // Dreamcast-optimized: lower threshold (64) and conservative growth
    // for 16MB RAM constraint
    intptr_t new_cap = oldcap;

    if (cap > new_cap)
    {
        const int DREAMCAST_THRESHOLD = 64; // Lower threshold for 16MB RAM

        if (new_cap < DREAMCAST_THRESHOLD)
        {
            // Small slices: double capacity (with overflow check)
            if (new_cap > INTPTR_MAX / 2)
            {
                new_cap = cap; // Can't double, use cap
            }
            else
            {
                new_cap = new_cap * 2;
            }
        }
        else
        {
            // Large slices: grow by 12.5% (more conservative than Go's 25%)
            // This reduces memory waste on memory-constrained Dreamcast
            // Overflow check: new_cap + new_cap/8
            intptr_t growth = new_cap / 8;
            if (new_cap > INTPTR_MAX - growth)
            {
                new_cap = cap; // Overflow, use cap
            }
            else
            {
                new_cap = new_cap + growth;
            }
        }

        if (new_cap < cap)
        {
            new_cap = cap;
        }
    }

    // Check for overflow: elem_size * new_cap
    if (new_cap > 0 && elem_size > SIZE_MAX / (size_t)new_cap)
    {
        runtime_panicstring("growslice: cap out of range");
        return empty_slice; // Return safe value if panic recovered
    }

    // Allocate new backing array using element type
    size_t total_size = elem_size * new_cap;

    // Dreamcast-specific: fail early instead of exhausting memory
    if (total_size > DC_MAX_ALLOC_SIZE)
    {
        runtime_panicstring("growslice: allocation too large for Dreamcast");
        return empty_slice; // Return safe value if panic recovered
    }

    void *new_array = gc_alloc(total_size, et);

    if (!new_array)
    {
        runtime_panicstring("growslice: allocation failed");
        return empty_slice; // Return safe value if panic recovered
    }

    // Copy old data
    if (oldarray && oldlen > 0)
    {
        memcpy(new_array, oldarray, oldlen * elem_size);
    }

    Slice new_slice;
    new_slice.__values = new_array;
    // Note: gccgo on SH-4 expects the NEW length, not oldlen.
    // The 'cap' parameter is the required minimum capacity which equals
    // oldlen + number_of_appended_elements, i.e., the new length.
    new_slice.__count = (int)cap;
    new_slice.__capacity = (int)new_cap;

    return new_slice;
}

// ===== Map Operations =====
// Note: runtime_makemap is now implemented in map_dreamcast.c
// These stubs are kept for backward compatibility if needed

// runtime_makemap_small is defined in gc_runtime.c

// ===== Channel Operations =====
// runtime_makechan is defined in gc_runtime.c

// ===== Type Registration =====

void runtime_registerTypeDescriptors(int n, void *p) __asm__("_runtime.registerTypeDescriptors");
void runtime_registerTypeDescriptors(int n, void *p)
{
    // For now, we ignore type descriptors in this simple implementation
    // A full implementation would store these for reflection/GC
    (void)n;
    (void)p;
}

// ===== String Operations =====
// String operations are now in string_dreamcast.c
// These are kept for compatibility but should migrate to use GoString

GoString runtime_gostringnocopy(const char *str)
{
    GoString s;
    s.str = (const uint8_t *)str;
    s.len = str ? strlen(str) : 0;
    return s;
}

// ===== Interface Operations =====
// Interface operations are now in interface_dreamcast.c

// ===== Panic and Error Handling =====

// All panic functions are now implemented in defer_dreamcast.c:
// - runtime_panicstring
// - runtime_panic
// - runtime_gopanic
// - runtime_throw

// ===== Defer Support =====
// Implemented in defer_dreamcast.c

// ===== Goroutine Support =====

// getg() is implemented in tls_sh4.c

struct G *runtime_g(void)
{
    return getg();
}

/*
 * Gosched yields the processor, allowing other goroutines to run.
 *
 * This is THE function to call in long-running loops without channel ops.
 * If you have a loop like:
 *
 *   for {
 *       doWork()
 *   }
 *
 * And doWork() doesn't touch channels or call time.Sleep(), you MUST call
 * runtime.Gosched() or other goroutines will never run.
 *
 * This is not a bug - it's how cooperative scheduling works. You yield
 * when YOU decide it's appropriate, not when the scheduler forces you.
 */
void runtime_gosched(void)
{
#ifdef GODC_GOROUTINES
    extern void go_yield(void);
    go_yield();
#endif
}

// ===== Runtime Initialization =====
// Note: runtime_init is implemented in gc_runtime.c

void runtime_main(void)
{
    // Main goroutine entry
    // gccgo generates main.main with a dot
    extern void main_dot_main(void) __asm__("_main.main");
    main_dot_main();
}

// ===== Print Functions =====
// Print functions are defined in go-print.c to avoid duplication

// ===== Memory Stats =====
// NOTE: MemStats and runtime_ReadMemStats are defined in gc_runtime.c
// This stub file only provides runtime function implementations.

// ===== GC Control =====
// runtime_GC is defined in gc_runtime.c

int32_t runtime_NumCPU(void)
{
    return 1; // Dreamcast is single-core
}

int32_t runtime_GOMAXPROCS(int32_t n)
{
    return 1; // Always 1 on Dreamcast
}

// ===== Select Support =====
// NOTE: runtime.block is now implemented in select.c

// ===== Time Support =====

int64_t runtime_nanotime(void) __asm__("_runtime.nanotime");
int64_t runtime_nanotime(void)
{
    // timer_ns_gettime64() returns nanoseconds since boot - no conversion needed
    return (int64_t)timer_ns_gettime64();
}

int64_t runtime_walltime(void) __asm__("_runtime.walltime");
int64_t runtime_walltime(void)
{
    // Use cached boot time + timer delta for better performance
    // (avoids slow G2 bus access to RTC on every call)
    // rtc_boot_time() returns Unix timestamp when system booted
    // timer_ns_gettime64() returns nanoseconds since boot
    time_t boot_secs = rtc_boot_time();
    uint64_t elapsed_ns = timer_ns_gettime64();
    return (int64_t)boot_secs * 1000000000LL + (int64_t)elapsed_ns;
}

// ===== OS Interface =====

void runtime_osinit(void)
{
    // Dreamcast-specific OS initialization
}

void runtime_schedinit(void)
{
    // Scheduler initialization (single-threaded for now)
}

// ===== Exit =====

void runtime_exit(int32_t code)
{
    (void)code;
    arch_exit(); // Use KOS arch_exit() for clean shutdown
}

// ===== Type Equality Functions =====
// These are needed for type descriptors of float and complex types
// gccgo generates references to these in type descriptor structures
// Use proper assembly names to match gccgo's name mangling
// These functions take POINTERS to values (like gccgo's runtime/alg.go)

// Note: gccgo uses bool return type which is _Bool (1 byte)
// Note: Symbol names need leading underscore for linker

// Float32 equality
_Bool runtime_f32equal(void *p, void *q) __asm__("_runtime.f32equal..f");
_Bool runtime_f32equal(void *p, void *q)
{
    float *a = (float *)p;
    float *b = (float *)q;
    return *a == *b;
}

// Float64 equality
_Bool runtime_f64equal(void *p, void *q) __asm__("_runtime.f64equal..f");
_Bool runtime_f64equal(void *p, void *q)
{
    double *a = (double *)p;
    double *b = (double *)q;
    return *a == *b;
}

// Complex64 equality
_Bool runtime_c64equal(void *p, void *q) __asm__("_runtime.c64equal..f");
_Bool runtime_c64equal(void *p, void *q)
{
    float _Complex *a = (float _Complex *)p;
    float _Complex *b = (float _Complex *)q;
    return *a == *b;
}

// Complex128 equality
_Bool runtime_c128equal(void *p, void *q) __asm__("_runtime.c128equal..f");
_Bool runtime_c128equal(void *p, void *q)
{
    double _Complex *a = (double _Complex *)p;
    double _Complex *b = (double _Complex *)q;
    return *a == *b;
}

// String equality for type descriptors
_Bool runtime_strequal(void *p, void *q) __asm__("_runtime.strequal..f");
_Bool runtime_strequal(void *p, void *q)
{
    GoString *a = (GoString *)p;
    GoString *b = (GoString *)q;
    if (a->len != b->len)
        return false;
    if (a->len == 0)
        return true;
    return memcmp(a->str, b->str, a->len) == 0;
}

// String hash for map key hashing
// gccgo signature: uintptr_t strhash(void *key, uintptr_t seed)
uintptr_t runtime_strhash(void *key, uintptr_t seed) __asm__("_runtime.strhash..f");
uintptr_t runtime_strhash(void *key, uintptr_t seed)
{
    GoString *str = (GoString *)key;
    uintptr_t h = seed;
    intptr_t i;
    for (i = 0; i < str->len; i++)
    {
        h = h * 31 + str->str[i];
    }
    // Final mixing
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    return h;
}

// Helper: mix hash value
static inline uintptr_t hash_mix(uintptr_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    return h;
}

// 0-byte memory hash (empty key)
// gccgo signature: uintptr_t memhash0(void *key, uintptr_t seed)
uintptr_t runtime_memhash0(void *key, uintptr_t seed) __asm__("_runtime.memhash0..f");
uintptr_t runtime_memhash0(void *key, uintptr_t seed)
{
    (void)key;
    return hash_mix(seed);
}

// 8-bit memory hash for map key hashing
// gccgo signature: uintptr_t memhash8(void *key, uintptr_t seed)
uintptr_t runtime_memhash8(void *key, uintptr_t seed) __asm__("_runtime.memhash8..f");
uintptr_t runtime_memhash8(void *key, uintptr_t seed)
{
    uint8_t val = *(uint8_t *)key;
    uintptr_t h = seed ^ val;
    return hash_mix(h);
}

// 16-bit memory hash for map key hashing
// gccgo signature: uintptr_t memhash16(void *key, uintptr_t seed)
uintptr_t runtime_memhash16(void *key, uintptr_t seed) __asm__("_runtime.memhash16..f");
uintptr_t runtime_memhash16(void *key, uintptr_t seed)
{
    uint16_t val = *(uint16_t *)key;
    uintptr_t h = seed ^ val;
    return hash_mix(h);
}

// 32-bit memory hash for map key hashing
// gccgo signature: uintptr_t memhash32(void *key, uintptr_t seed)
uintptr_t runtime_memhash32(void *key, uintptr_t seed) __asm__("_runtime.memhash32..f");
uintptr_t runtime_memhash32(void *key, uintptr_t seed)
{
    uint32_t val = *(uint32_t *)key;
    uintptr_t h = seed ^ val;
    return hash_mix(h);
}

// 64-bit memory hash for map key hashing
// gccgo signature: uintptr_t memhash64(void *key, uintptr_t seed)
uintptr_t runtime_memhash64(void *key, uintptr_t seed) __asm__("_runtime.memhash64..f");
uintptr_t runtime_memhash64(void *key, uintptr_t seed)
{
    uint32_t *p = (uint32_t *)key;
    uintptr_t h = seed ^ p[0];
    h = hash_mix(h);
    h ^= p[1];
    return hash_mix(h);
}

// 128-bit memory hash for map key hashing
// gccgo signature: uintptr_t memhash128(void *key, uintptr_t seed)
uintptr_t runtime_memhash128(void *key, uintptr_t seed) __asm__("_runtime.memhash128..f");
uintptr_t runtime_memhash128(void *key, uintptr_t seed)
{
    uint32_t *p = (uint32_t *)key;
    uintptr_t h = seed;
    for (int i = 0; i < 4; i++)
    {
        h ^= p[i];
        h = hash_mix(h);
    }
    return h;
}

// Float32 hash for map key hashing
// gccgo signature: uintptr_t f32hash(void *key, uintptr_t seed)
uintptr_t runtime_f32hash(void *key, uintptr_t seed) __asm__("_runtime.f32hash..f");
uintptr_t runtime_f32hash(void *key, uintptr_t seed)
{
    float f = *(float *)key;
    // Handle special cases: +0 and -0 should hash the same, NaN should be consistent
    if (f == 0)
    {
        return hash_mix(seed);
    }
    uint32_t bits = *(uint32_t *)key;
    return hash_mix(seed ^ bits);
}

// Float64 hash for map key hashing
// gccgo signature: uintptr_t f64hash(void *key, uintptr_t seed)
uintptr_t runtime_f64hash(void *key, uintptr_t seed) __asm__("_runtime.f64hash..f");
uintptr_t runtime_f64hash(void *key, uintptr_t seed)
{
    double d = *(double *)key;
    // Handle special cases: +0 and -0 should hash the same
    if (d == 0)
    {
        return hash_mix(seed);
    }
    uint32_t *p = (uint32_t *)key;
    uintptr_t h = seed ^ p[0];
    h = hash_mix(h);
    h ^= p[1];
    return hash_mix(h);
}

// Memory equality functions are defined in go-memequal.c

// ===== Additional C stub implementations =====

// NOTE: Interface equality functions are properly implemented in interface_dreamcast.c:
// - runtime_interequal_f (symbol: _runtime.interequal..f)
// - runtime_nilinterequal_f (symbol: _runtime.nilinterequal..f)

// Typed memory move (with GC awareness)
void _runtime_typedmemmove(void *typ, void *dst, void *src) __asm__("_runtime_typedmemmove");
void _runtime_typedmemmove(void *typ, void *dst, void *src)
{
    if (!typ || !dst || !src)
        return;

    struct __go_type_descriptor *td = (struct __go_type_descriptor *)typ;
    if (td->__size > 0)
    {
        memmove(dst, src, td->__size);
    }
}

// Clear memory (no heap pointers)
void _runtime_memclrNoHeapPointers(void *ptr,
                                   uintptr_t size) __asm__("_runtime_memclrNoHeapPointers");
void _runtime_memclrNoHeapPointers(void *ptr, uintptr_t size)
{
    if (ptr && size > 0)
    {
        memset(ptr, 0, size);
    }
}

// String equality
_Bool _runtime_strequal(GoString s1, GoString s2) __asm__("_runtime_strequal");
_Bool _runtime_strequal(GoString s1, GoString s2)
{
    if (s1.len != s2.len)
        return false;
    if (s1.len == 0)
        return true;
    return memcmp(s1.str, s2.str, s1.len) == 0;
}

// All defer functions are now implemented in defer_dreamcast.c:
// - runtime_deferproc / runtime_deferproc_impl
// - runtime_deferreturn / runtime_deferreturn_impl
// - runtime_deferprocStack
// - runtime_checkdefer
// - runtime_setdeferretaddr

// Panic helpers are now in go-panic.c:
// - runtime_panicdivide
// - runtime_goPanicIndex
// - runtime_goPanicSlice
// - runtime_goPanicSliceAcap
// - runtime_goPanicSliceAlen
// - runtime_goPanicSliceB

// Print lock/unlock are defined in go-print.c

// Register GC roots - implemented in gc_heap.c
// The registerGCRoots function is defined there with proper gc_root_list_t handling

// Struct init
void _runtime_structinit(void *dst, void *src, void *typ) __asm__("_runtime_structinit");
void _runtime_structinit(void *dst, void *src, void *typ)
{
    if (!dst || !src || !typ)
        return;

    struct __go_type_descriptor *td = (struct __go_type_descriptor *)typ;
    if (td->__size > 0)
    {
        memcpy(dst, src, td->__size);
    }
}

// GCC exception handling - required by gccgo
typedef enum
{
    _URC_NO_REASON = 0,
    _URC_CONTINUE_UNWIND = 8
} _Unwind_Reason_Code;

_Unwind_Reason_Code __gccgo_personality_v0(int version, int actions,
                                           uint64_t exception_class, void *ue_header, void *context)
{
    (void)version;
    (void)actions;
    (void)exception_class;
    (void)ue_header;
    (void)context;
    return _URC_CONTINUE_UNWIND;
}

// Field tracking callback - called by gccgo when -fgo-debug-fieldtrack is enabled
// This is a no-op stub; field tracking is not used on Dreamcast
void __go_fieldtrack(void *field) __asm__("___go_fieldtrack");
void __go_fieldtrack(void *field)
{
    (void)field;
}

// ===== Runtime Configuration Accessors =====
// These expose compile-time constants to Go code for accurate reporting

#include "godc_config.h"

int32_t runtime_goroutineStackSize(void) __asm__("_runtime.goroutineStackSize");
int32_t runtime_goroutineStackSize(void)
{
    return GOROUTINE_STACK_SIZE;
}

int32_t runtime_largeObjectThreshold(void) __asm__("_runtime.largeObjectThreshold");
int32_t runtime_largeObjectThreshold(void)
{
    return GC_LARGE_OBJECT_THRESHOLD_KB * 1024;
}
