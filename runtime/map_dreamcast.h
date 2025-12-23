#ifndef MAP_DREAMCAST_H
#define MAP_DREAMCAST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "godc_config.h"
#include "type_descriptors.h"

#ifdef __cplusplus
extern "C"
{
#endif

// GoString structure (must match runtime.h definition)
// This is duplicated here to avoid circular includes
#ifndef GODC_GOSTRING_DEFINED
#define GODC_GOSTRING_DEFINED
    typedef struct
    {
        const uint8_t *str;
        intptr_t len;
    } GoString;
#endif

/* Constants */
#define MAP_BUCKET_COUNT 8      // Entries per bucket
#define MAP_MAX_KEY_SIZE 128    // Max inline key size
#define MAP_MAX_VALUE_SIZE 128  // Max inline value size

/* MAP_MAX_BUCKET_SHIFT defined in godc_config.h */

#define MAP_LOAD_FACTOR_NUM 13  // Load factor 6.5 = 13/2
#define MAP_LOAD_FACTOR_DEN 2

// tophash values (must match Go runtime)
#define MAP_EMPTY_REST 0      // Empty, rest of bucket empty too
#define MAP_EMPTY_ONE 1       // Empty slot
#define MAP_EVACUATED_X 2     // Evacuated to first half
#define MAP_EVACUATED_Y 3     // Evacuated to second half
#define MAP_EVACUATED_EMPTY 4 // Was empty, bucket evacuated
#define MAP_MIN_TOPHASH 5     // Minimum valid tophash

// Map header flags
#define MAP_FLAG_ITERATOR 0x01       // Iterator active
#define MAP_FLAG_OLD_ITERATOR 0x02   // Iterator on old buckets
#define MAP_FLAG_WRITING 0x04        // Write in progress
#define MAP_FLAG_SAME_SIZE_GROW 0x08 // Same-size grow (reorganize only)

// MapType flags
#define MAPTYPE_INDIRECT_KEY (1 << 0)     // Key stored as pointer
#define MAPTYPE_INDIRECT_VALUE (1 << 1)   // Value stored as pointer
#define MAPTYPE_REFLEXIVE_KEY (1 << 2)    // Key == key always (no NaN)
#define MAPTYPE_NEED_KEY_UPDATE (1 << 3)  // Update key on overwrite
#define MAPTYPE_HASH_MIGHT_PANIC (1 << 4) // Hash function may panic

/* Platform-specific macros */
#include <kos.h>
#include <arch/timer.h>

// SH-4 prefetch instruction for cache optimization
#define PREFETCH(addr) __asm__ volatile("pref @%0" : : "r"(addr))

// Force inline for hot path functions
#define MAP_INLINE static __always_inline

/* MAP_TRACE - no-op, debugging removed */
#define MAP_TRACE(fmt, ...) ((void)0)

/* Data Structures */

    /**
     * Map header (hmap in Go runtime)
     *
     * Layout on 32-bit SH-4:
     *   Offset 0:  count (4 bytes)      - Word 0
     *   Offset 4:  flags (1 byte)
     *   Offset 5:  B (1 byte)
     *   Offset 6:  noverflow (2 bytes)
     *   Offset 8:  hash0 (4 bytes)      - Word 2
     *   Offset 12: buckets (4 bytes)    - Word 3 - POINTER
     *   Offset 16: oldbuckets (4 bytes) - Word 4 - POINTER
     *   Offset 20: nevacuate (4 bytes)  - Word 5
     *   Offset 24: extra (4 bytes)      - Word 6 - POINTER
     *   Total: 28 bytes
     */
    typedef struct
    {
        uintptr_t count;     // Number of live entries
        uint8_t flags;       // State flags
        uint8_t B;           // log2(bucket count), max 255
        uint16_t noverflow;  // Approximate overflow bucket count
        uint32_t hash0;      // Hash seed (random per map instance)
        void *buckets;       // Current bucket array (2^B buckets)
        void *oldbuckets;    // Previous buckets during growth
        uintptr_t nevacuate; // Evacuation progress counter
        void *extra;         // Overflow bucket pool (optional)
    } GoMap;

    // Verify layout at compile time
    _Static_assert(sizeof(GoMap) == 28, "GoMap size mismatch - expected 28 bytes");
    _Static_assert(offsetof(GoMap, buckets) == 12, "GoMap.buckets offset mismatch");
    _Static_assert(offsetof(GoMap, oldbuckets) == 16, "GoMap.oldbuckets offset mismatch");
    _Static_assert(offsetof(GoMap, extra) == 24, "GoMap.extra offset mismatch");

    /**
     * Map type descriptor (uses gccgo's __go_map_type from type_descriptors.h)
     *
     * Layout verified from gcc/go/gofrontend/types.cc in GCC 15.1.0:
     *   offset 0:  embedded _type (36 bytes)
     *   offset 36: key *_type
     *   offset 40: elem *_type
     *   offset 44: bucket *_type
     *   offset 48: hasher FuncVal*
     *   offset 52: keysize uint8
     *   offset 53: valuesize uint8 (NOTE: gccgo calls it "valuesize" not "elemsize")
     *   offset 54: bucketsize uint16
     *   offset 56: flags uint32
     *   Total: 60 bytes
     */
    typedef struct __go_map_type MapType;

// Accessor macros for MapType (direct field access - layout verified from gccgo source)
#define MAPTYPE_KEY(t) ((t)->__key_type)
#define MAPTYPE_ELEM(t) ((t)->__val_type)
#define MAPTYPE_BUCKET(t) ((t)->__bucket_type)
// gccgo hasher signature: uintptr_t hasher(void *key, uintptr_t seed)
#define MAPTYPE_HASHER(t) ((uintptr_t (*)(void *, uintptr_t))((t)->__hasher))
#define MAPTYPE_KEYSIZE(t) ((t)->__keysize)
#define MAPTYPE_ELEMSIZE(t) ((t)->__valuesize) // gccgo uses "valuesize"
#define MAPTYPE_BUCKETSIZE(t) ((t)->__bucketsize)
#define MAPTYPE_FLAGS(t) ((t)->__flags)

    /**
     * Map iterator state (for range loops)
     *
     * CRITICAL: Field order MUST match gccgo's hiter struct from types.cc!
     * The compiler accesses key/elem directly by offset, not by field name.
     *
     * From gcc/go/gofrontend/types.cc Map_type::hiter_type():
     *   make_builtin_struct_type(15,
     *     "key", key_ptr_type,         // offset 0  - MUST BE FIRST
     *     "val", val_ptr_type,         // offset 4  - MUST BE SECOND
     *     "t", uint8_ptr_type,         // offset 8
     *     "h", hmap_ptr_type,          // offset 12
     *     "buckets", bucket_ptr_type,  // offset 16
     *     "bptr", bucket_ptr_type,     // offset 20
     *     "overflow", void_ptr_type,   // offset 24
     *     "oldoverflow", void_ptr_type,// offset 28
     *     "startBucket", uintptr_type, // offset 32
     *     "offset", uint8_type,        // offset 36
     *     "wrapped", bool_type,        // offset 37
     *     "B", uint8_type,             // offset 38
     *     "i", uint8_type,             // offset 39
     *     "bucket", uintptr_type,      // offset 40
     *     "checkBucket", uintptr_type);// offset 44
     */
    typedef struct
    {
        void *key;             // offset 0:  Current key pointer (MUST BE FIRST!)
        void *elem;            // offset 4:  Current value pointer (MUST BE SECOND!)
        MapType *t;            // offset 8:  Map type
        GoMap *h;              // offset 12: Map header
        void *buckets;         // offset 16: Bucket array at iteration start
        void *bptr;            // offset 20: Current bucket pointer
        void *overflow;        // offset 24: Overflow bucket pointer (for GC)
        void *oldoverflow;     // offset 28: Old overflow pointer (during growth, for GC)
        uintptr_t startBucket; // offset 32: Starting bucket (randomized)
        uint8_t offset;        // offset 36: Offset within bucket (randomized start)
        bool wrapped;          // offset 37: Have we wrapped around?
        uint8_t B;             // offset 38: B value at iteration start
        uint8_t i;             // offset 39: Current slot in bucket (0-7)
        uintptr_t bucket;      // offset 40: Current bucket index
        uintptr_t checkBucket; // offset 44: For growth tracking
    } MapIter;

    // Verify layout matches gccgo expectations
    _Static_assert(sizeof(MapIter) == 48, "MapIter size mismatch");
    _Static_assert(offsetof(MapIter, key) == 0, "MapIter.key must be at offset 0");
    _Static_assert(offsetof(MapIter, elem) == 4, "MapIter.elem must be at offset 4");

    /**
     * Return type for mapaccess2 (value, ok)
     * SH-4 returns small structs in r0/r1
     */
    typedef struct
    {
        void *value; // Pointer to value or zero value
        bool ok;     // true if key was found
    } MapAccess2Result;

/* Map Creation */

    /**
     * Initialize the map subsystem (call during runtime init)
     */
    void map_init(void);

    /**
     * Create a new map with size hint
     * @param t     Map type descriptor
     * @param hint  Expected number of entries (0 for default)
     * @param h     Optional pre-allocated header (usually NULL)
     * @return      Initialized map pointer
     */
    GoMap *runtime_makemap(MapType *t, intptr_t hint, GoMap *h) __asm__("_runtime.makemap");

    /**
     * Create a new small map (no size hint)
     * gccgo uses double underscore: _runtime.makemap__small
     *
     * IMPORTANT: Takes NO parameters! Creates empty map header only.
     * Buckets are allocated lazily when items are inserted via mapassign.
     *
     * @return Initialized empty map pointer
     */
    GoMap *runtime_makemap_small(void) __asm__("_runtime.makemap__small");

/* Map Access */

    /**
     * Look up key, return pointer to value (or zero value)
     * @param t   Map type
     * @param h   Map header (may be NULL)
     * @param key Pointer to key
     * @return    Pointer to value or zero value buffer
     */
    void *runtime_mapaccess1(MapType *t, GoMap *h, void *key) __asm__("_runtime.mapaccess1");

    /**
     * Look up key with existence check
     * Returns (value_ptr, ok) in r0, r1 on SH-4
     */
    MapAccess2Result runtime_mapaccess2(MapType *t, GoMap *h, void *key) __asm__("_runtime.mapaccess2");

/* Map Modification */

    /**
     * Get slot for assignment (creates entry if needed)
     * @param t   Map type
     * @param h   Map header (panics if NULL)
     * @param key Pointer to key
     * @return    Pointer to value slot for assignment
     */
    void *runtime_mapassign(MapType *t, GoMap *h, void *key) __asm__("_runtime.mapassign");

    /**
     * Delete key from map
     * @param t   Map type
     * @param h   Map header (no-op if NULL)
     * @param key Pointer to key to delete
     */
    void runtime_mapdelete(MapType *t, GoMap *h, void *key) __asm__("_runtime.mapdelete");

/* Map Info */

    /**
     * Get number of entries in map
     * @param h Map header (may be NULL)
     * @return  Number of entries (0 if nil)
     */
    intptr_t runtime_maplen(GoMap *h) __asm__("_runtime.maplen");

/* Map Iteration */

    void runtime_mapiterinit(MapType *t, GoMap *h, MapIter *it) __asm__("_runtime.mapiterinit");

    void runtime_mapiternext(MapIter *it) __asm__("_runtime.mapiternext");

/* Hash Functions */

    /**
     * Fast pseudo-random number generator
     * Uses xorshift32 seeded from timer
     */
    uint32_t map_fastrand(void);

    /**
     * Hash arbitrary memory (wyhash 32-bit variant)
     * Optimized for SH-4 with 32x32→64 multiply
     */
    uintptr_t map_memhash(void *data, uintptr_t seed, uintptr_t size);

    /**
     * Hash a Go string
     */
    uintptr_t map_strhash(void *s, uintptr_t seed);

/* Panic functions (from runtime) */
    void runtime_throw(const char *msg);
    void runtime_panicstring(const char *msg);

#ifdef __cplusplus
}
#endif

#endif // MAP_DREAMCAST_H
