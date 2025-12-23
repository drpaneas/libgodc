#define MAP_DREAMCAST_C_INCLUDED 1

#include "map_dreamcast.h"
#include "gc_semispace.h"
#include "runtime.h"
#include "copy.h"
#include <string.h>
#include <stdio.h>

#include <kos.h>
#include <arch/timer.h>

#define MAP_MIN_B_FOR_SAME_SIZE_GROW 2  /* Below this, same-size rehash is useless */

static inline uintptr_t fast32_hash(uint32_t key, uintptr_t seed);

/* --- GC Type Descriptor --- */

/*
 * GoMap GC bitmap: marks pointer fields for garbage collector.
 *
 * GoMap layout (28 bytes, 7 words):
 *   Word 0: count      (non-pointer)
 *   Word 1: flags/B/noverflow (non-pointer)
 *   Word 2: hash0      (non-pointer)
 *   Word 3: buckets    (POINTER) -> bit 3
 *   Word 4: oldbuckets (POINTER) -> bit 4
 *   Word 5: nevacuate  (non-pointer)
 *   Word 6: extra      (POINTER) -> bit 6
 *
 * Bitmap: bits 3,4,6 = 0b01011000 = 0x58
 *
 * The static asserts in map_dreamcast.h verify these offsets are correct.
 */
static const uint8_t __go_map_gcdata[] = {0x58};

static struct __go_type_descriptor __go_map_type = {
    .__size = sizeof(GoMap),
    .__ptrdata = 28,      // Through 'extra' field (offset 24 + 4)
    .__hash = 0x4D415030, // "MAP0"
    .__tflag = 0,
    .__align = __alignof__(GoMap),
    .__field_align = __alignof__(GoMap),
    .__code = GO_STRUCT,
    .__equalfn = NULL,
    .__gcdata = __go_map_gcdata,
    .__reflection = NULL,
    .__uncommon = NULL,
    .__pointer_to_this = NULL,
};

/* --- PRNG (xorshift32) --- */

static uint32_t g_fastrand_seed = 0;
static bool g_fastrand_initialized = false;

/*
 * Initialize PRNG with timer-based entropy.
 *
 * Called once from map_init(). Don't call from hot paths.
 */
static void fastrand_init(void)
{
    /* Already initialized - this is the fast path after startup */
    if (g_fastrand_initialized)
        return;

    /*
     * Use high-resolution timer for entropy.
     * Two reads to get more bits - they'll differ by a few microseconds.
     */
    uint32_t t1 = (uint32_t)timer_us_gettime64();
    uint32_t t2 = (uint32_t)(timer_us_gettime64() >> 32);
    g_fastrand_seed = t1 ^ t2;
    if (g_fastrand_seed == 0)
        g_fastrand_seed = 0xDEADBEEF;

    /*
     * M:1 scheduling: no synchronization needed.
     * Only one goroutine runs at a time, so there's no race.
     * If you're adding threading, you need a real lock here, not a barrier.
     */
    g_fastrand_initialized = true;
}

/**
 * Fast PRNG (xorshift32)
 */
uint32_t map_fastrand(void)
{
    if (!g_fastrand_initialized)
    {
        fastrand_init();
    }

    uint32_t x = g_fastrand_seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_fastrand_seed = x;
    return x;
}

/* Local alias for internal use */
static inline uint32_t fastrand(void)
{
    return map_fastrand();
}


/* --- Hash Functions (wyhash 32-bit variant for SH-4) --- */


/**
 * Mix function using 32x32→64 multiply
 * SH-4 has dmuls.l instruction for this
 */
MAP_INLINE uint32_t wymix32(uint32_t a, uint32_t b)
{
    uint64_t r = (uint64_t)a * (uint64_t)b;
    return (uint32_t)(r ^ (r >> 32));
}

/**
 * Read 32 bits (handles unaligned access safely)
 */
MAP_INLINE uint32_t wyread32(const uint8_t *p)
{
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}

/**
 * Hash arbitrary memory
 */
uintptr_t map_memhash(void *data, uintptr_t seed, uintptr_t size)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t h = (uint32_t)seed;

    // Process 4 bytes at a time
    while (size >= 4)
    {
        h = wymix32(h ^ wyread32(p), 0x9E3779B9);
        p += 4;
        size -= 4;
    }

    // Handle remaining bytes
    if (size > 0)
    {
        uint32_t tail = 0;
        memcpy(&tail, p, size);
        h = wymix32(h ^ tail, 0x85EBCA6B);
    }

    return (uintptr_t)wymix32(h, (uint32_t)size);
}

/**
 * Hash a Go string
 */
uintptr_t map_strhash(void *s, uintptr_t seed)
{
    GoString *str = (GoString *)s;
    if (str->str == NULL || str->len == 0)
    {
        return seed;
    }
    return map_memhash((void *)str->str, seed, (uintptr_t)str->len);
}

// NOTE: hash32/hash64 removed - use fast32_hash/fast64_hash instead.
// Having multiple hash functions for the same type causes lookup failures
// when assignment uses one hash and lookup uses another.
// The fast*_hash functions are the canonical implementations that match
// the gccgo runtime.memhash* functions.


/* --- Helper Functions (inlined for performance) --- */


/**
 * Get tophash from full hash (top 8 bits, clamped to valid range)
 */
MAP_INLINE uint8_t tophash(uintptr_t hash)
{
    uint8_t top = (uint8_t)(hash >> (sizeof(uintptr_t) * 8 - 8));
    if (top < MAP_MIN_TOPHASH)
    {
        top += MAP_MIN_TOPHASH;
    }
    return top;
}

/**
 * Check if tophash indicates empty slot
 */
MAP_INLINE bool isEmpty(uint8_t x)
{
    return x <= MAP_EMPTY_ONE;
}

/**
 * Check if map is currently growing
 */
MAP_INLINE bool isGrowing(GoMap *h)
{
    return h->oldbuckets != NULL;
}

/**
 * Check if load factor exceeded
 */
MAP_INLINE bool overLoadFactor(intptr_t count, uint8_t B)
{
    // count > 6.5 * bucketCount = count > 13 * bucketCount / 2
    uintptr_t bucketCount = (uintptr_t)1 << B;
    return count > (intptr_t)(bucketCount * MAP_LOAD_FACTOR_NUM / MAP_LOAD_FACTOR_DEN);
}

/**
 * Check if too many overflow buckets
 */
MAP_INLINE bool tooManyOverflowBuckets(uint16_t noverflow, uint8_t B)
{
    uint8_t threshold = B;
    if (threshold > 15)
        threshold = 15;
    return noverflow >= (uint16_t)(1 << threshold);
}

/**
 * Get number of buckets for given B
 */
MAP_INLINE uintptr_t bucketCount(uint8_t B)
{
    return (uintptr_t)1 << B;
}

/**
 * Get bucket mask for hash
 */
MAP_INLINE uintptr_t bucketMask(uint8_t B)
{
    return bucketCount(B) - 1;
}

/**
 * Get old bucket count during growth
 * Handles both double-grow and same-size-grow cases
 *
 * BUG FIX: During same-size growth (defragmentation), h->B is NOT incremented,
 * so bucketCount(h->B - 1) would give the wrong count (half the actual).
 * We check the MAP_FLAG_SAME_SIZE_GROW flag to determine the correct count.
 */
MAP_INLINE uintptr_t nOldBuckets(GoMap *h)
{
    if (h->flags & MAP_FLAG_SAME_SIZE_GROW)
    {
        // Same-size grow: B was not incremented
        return bucketCount(h->B);
    }
    // Double grow: B was incremented, so old count is B-1
    return bucketCount(h->B - 1);
}

/**
 * Get old bucket mask during growth
 * This is the mask to use when looking up in oldbuckets
 */
MAP_INLINE uintptr_t nOldBucketsMask(GoMap *h)
{
    return nOldBuckets(h) - 1;
}


/* --- Bucket Access Helpers (inlined) --- */


/**
 * Get pointer to bucket at index
 */
MAP_INLINE void *bucketAt(void *buckets, uintptr_t index, uint16_t bucketsize)
{
    return (uint8_t *)buckets + index * bucketsize;
}

/**
 * Get pointer to tophash array in bucket
 */
MAP_INLINE uint8_t *bucketTophash(void *bucket)
{
    return (uint8_t *)bucket;
}

/**
 * Get pointer to key at slot i in bucket
 * Layout: tophash[8] | keys[8] | values[8] | overflow
 */
MAP_INLINE void *bucketKey(MapType *t, void *bucket, int i)
{
    return (uint8_t *)bucket + MAP_BUCKET_COUNT + (size_t)i * MAPTYPE_KEYSIZE(t);
}

/**
 * Get pointer to value at slot i in bucket
 */
MAP_INLINE void *bucketValue(MapType *t, void *bucket, int i)
{
    return (uint8_t *)bucket + MAP_BUCKET_COUNT +
           MAP_BUCKET_COUNT * MAPTYPE_KEYSIZE(t) + (size_t)i * MAPTYPE_ELEMSIZE(t);
}

/**
 * Get pointer to overflow bucket pointer (at end of bucket)
 */
MAP_INLINE void **bucketOverflow(MapType *t, void *bucket)
{
    return (void **)((uint8_t *)bucket + MAPTYPE_BUCKETSIZE(t) - sizeof(void *));
}


/* --- Key/Value Operations --- */


/*
 * keyEqual - Compare two keys for equality.
 *
 * Uses the type's equality function if available, otherwise memcmp.
 *
 * Note: __equalfn is stored as void* in the type descriptor (gccgo ABI).
 * Converting void* to function pointer is implementation-defined, but
 * works on all real platforms including SH-4/KOS. GCC generates correct
 * code for this pattern.
 */
static bool keyEqual(MapType *t, void *k1, void *k2)
{
	struct __go_type_descriptor *keytype = MAPTYPE_KEY(t);

	if (keytype->__equalfn != NULL) {
		_Bool (*fn)(void *, void *) = (_Bool (*)(void *, void *))keytype->__equalfn;
		return fn(k1, k2);
	}

	return memcmp(k1, k2, keytype->__size) == 0;
}

/**
 * Copy key from src to dst
 *
 * Uses fast_copy for small keys to avoid memcpy overhead.
 */
MAP_INLINE void keyCopy(MapType *t, void *dst, void *src)
{
    if (MAPTYPE_FLAGS(t) & MAPTYPE_INDIRECT_KEY)
    {
        *(void **)dst = *(void **)src;
    }
    else
    {
        fast_copy(dst, src, MAPTYPE_KEYSIZE(t));
    }
}

/**
 * Copy value from src to dst
 *
 * Uses fast_copy for small values to avoid memcpy overhead.
 */
MAP_INLINE void valueCopy(MapType *t, void *dst, void *src)
{
    if (MAPTYPE_FLAGS(t) & MAPTYPE_INDIRECT_VALUE)
    {
        *(void **)dst = *(void **)src;
    }
    else
    {
        fast_copy(dst, src, MAPTYPE_ELEMSIZE(t));
    }
}

/**
 * Get actual key pointer (handles indirect keys)
 */
MAP_INLINE void *keyPtr(MapType *t, void *slot)
{
    if (MAPTYPE_FLAGS(t) & MAPTYPE_INDIRECT_KEY)
    {
        return *(void **)slot;
    }
    return slot;
}

/**
 * Get actual value pointer (handles indirect values)
 */
MAP_INLINE void *valuePtr(MapType *t, void *slot)
{
    if (MAPTYPE_FLAGS(t) & MAPTYPE_INDIRECT_VALUE)
    {
        return *(void **)slot;
    }
    return slot;
}


/* --- Memory Allocation --- */



/*
 * allocBuckets - Allocate bucket array.
 *
 * ALL bucket allocations go through gc_alloc. Period.
 *
 * Previous versions used gc_external_alloc_aligned for "cache efficiency"
 * on large maps. That was wrong:
 *   1. External allocations LEAK when the map is collected
 *   2. On a 16MB system, leaks kill you fast
 *   3. The cache benefit is marginal - bucket access patterns are random anyway
 *
 * The GC heap is 8-byte aligned which is sufficient for SH-4.
 * If you need 32-byte cache line alignment, fix gc_alloc, don't work around it.
 */
static void *allocBuckets(MapType *t, uintptr_t count)
{
    uint16_t bsize = MAPTYPE_BUCKETSIZE(t);
    size_t size;

    /* Overflow check - don't trust count from potentially corrupted type */
    if (count > SIZE_MAX / bsize)
        runtime_throw("allocBuckets: size overflow");

    size = count * bsize;

    /* gc_alloc returns zeroed memory and throws on OOM */
    return gc_alloc(size, MAPTYPE_BUCKET(t));
}


/* --- Zero Value Buffer --- */


/* Pre-allocated zero buffer for failed map lookups */
static uint8_t g_zero_value[MAP_ZERO_VALUE_MAX_SIZE] __attribute__((aligned(8)));
static void *zeroValue(struct __go_type_descriptor *type)
{
	if (type->__size > sizeof(g_zero_value))
		runtime_throw("map value type too large - "
		              "increase MAP_ZERO_VALUE_MAX_SIZE");
	return g_zero_value;
}


/* --- Map Growth --- */


/*
 * hashGrow - Grow the map's bucket array.
 *
 * Takes GoMap** (pointer-to-pointer) for GC safety.
 * The allocation inside can trigger GC, which moves the map header.
 * By taking a pointer to the caller's pointer, we can reload after GC.
 */
static void hashGrow(MapType *t, GoMap *h)
{
    MAP_TRACE("hashGrow: B=%u, count=%lu", (unsigned)h->B, (unsigned long)h->count);

    // Check Dreamcast size limit
    if (h->B >= MAP_MAX_BUCKET_SHIFT)
    {
        runtime_panicstring("map too large for Dreamcast");
    }

    // Determine growth type
    bool sameSizeGrow = !overLoadFactor(h->count + 1, h->B);

    // With very few buckets, same-size rehash is pointless - force real grow
    if (sameSizeGrow && h->B < MAP_MIN_B_FOR_SAME_SIZE_GROW)
    {
        MAP_TRACE("hashGrow: forcing real grow (B=%u too small)", h->B);
        sameSizeGrow = false;
    }

    void *oldbuckets = h->buckets;
    uintptr_t newBucketCount;

    if (sameSizeGrow)
    {
        newBucketCount = bucketCount(h->B);
        h->flags |= MAP_FLAG_SAME_SIZE_GROW;
        MAP_TRACE("hashGrow: same-size reorganize, B=%u", h->B);
    }
    else
    {
        newBucketCount = bucketCount(h->B + 1);
        h->B++;
        MAP_TRACE("hashGrow: doubling buckets, new B=%u", h->B);
    }

    /*
     * GC is inhibited by caller, so this allocation won't trigger collection.
     */
    h->buckets = allocBuckets(t, newBucketCount);
    
    if (h->buckets == NULL) {
        runtime_throw("hashGrow: bucket allocation returned NULL");
    }
    
    h->oldbuckets = oldbuckets;
    h->nevacuate = 0;
    h->noverflow = 0;
}

/**
 * Advance evacuation progress counter and check for completion
 */
static void advanceEvacuationProgress(MapType *t, GoMap *h, uintptr_t oldCount)
{
    uint16_t bucketsize = MAPTYPE_BUCKETSIZE(t);

    h->nevacuate++;

    // Skip buckets that are already evacuated
    while (h->nevacuate < oldCount)
    {
        void *checkBucket = bucketAt(h->oldbuckets, h->nevacuate, bucketsize);
        uint8_t firstTh = bucketTophash(checkBucket)[0];
        if (firstTh > MAP_EMPTY_ONE && firstTh < MAP_MIN_TOPHASH)
        {
            h->nevacuate++;
        }
        else
        {
            break;
        }
    }

    // Check if growth is complete
    if (h->nevacuate >= oldCount)
    {
        h->oldbuckets = NULL;
        h->flags &= ~MAP_FLAG_SAME_SIZE_GROW;
        MAP_TRACE("hashGrow: evacuation complete");
    }
}

/**
 * Evacuate a single bucket during incremental growth
 *
 * BUG FIX: Uses nOldBuckets(h) instead of bucketCount(h->B - 1) to correctly
 * handle same-size growth where B is not incremented.
 *
 * BUG FIX: Updates evacuation progress even on early return when bucket
 * is already evacuated, preventing nevacuate from getting stuck.
 *
 * NOTE: GC is inhibited by caller during map operations.
 */
static void evacuate(MapType *t, GoMap *h, uintptr_t oldbucket)
{
    uint16_t bucketsize = MAPTYPE_BUCKETSIZE(t);
    void *b = bucketAt(h->oldbuckets, oldbucket, bucketsize);
    uintptr_t oldCount = nOldBuckets(h);

    // Check if already evacuated
    uint8_t *tophashArr = bucketTophash(b);
    if (tophashArr[0] > MAP_EMPTY_ONE && tophashArr[0] < MAP_MIN_TOPHASH)
    {
        // KEY FIX: Still update progress even on early return!
        if (oldbucket == h->nevacuate)
        {
            advanceEvacuationProgress(t, h, oldCount);
        }
        return; // Already evacuated
    }

    // Get growth parameters
    bool isSameSizeGrow = (h->flags & MAP_FLAG_SAME_SIZE_GROW) != 0;
    uintptr_t newbit = oldCount; // Bit that distinguishes X vs Y destination

    // Process bucket chain
    void *bucket = b;
    while (bucket != NULL)
    {
        uint8_t *th = bucketTophash(bucket);

        for (int i = 0; i < MAP_BUCKET_COUNT; i++)
        {
            if (isEmpty(th[i]))
            {
                th[i] = MAP_EVACUATED_EMPTY;
                continue;
            }
            if (th[i] < MAP_MIN_TOPHASH)
            {
                continue; // Already processed
            }

            // Get key and compute hash
            void *keySlot = bucketKey(t, bucket, i);
            void *key = keyPtr(t, keySlot);
            uintptr_t hash = MAPTYPE_HASHER(t)(key, h->hash0);

            // Determine destination bucket
            uintptr_t destBucket;
            bool useY;

            if (isSameSizeGrow)
            {
                // Same-size grow: entry stays in same bucket index
                destBucket = oldbucket;
                useY = false;
            }
            else
            {
                // Double grow: entry goes to X (same index) or Y (index + oldCount)
                useY = (hash & newbit) != 0;
                destBucket = hash & bucketMask(h->B);
            }

            // Find slot in destination bucket
            void *dest = bucketAt(h->buckets, destBucket, bucketsize);
            uint8_t *destTh = bucketTophash(dest);

            int destSlot = -1;
            int safetyCounter = 0;
            while (destSlot < 0)
            {
                safetyCounter++;
                if (safetyCounter > MAP_EVACUATE_SAFETY_LIMIT)
                {
                    dbglog(DBG_CRITICAL, "[map] INFINITE LOOP in evacuate!\n");
                    dbglog(DBG_CRITICAL, "  destBucket=%zu, h->B=%u, bucketsize=%u\n",
                           (size_t)destBucket, (unsigned)h->B, (unsigned)bucketsize);
                    dbglog(DBG_CRITICAL, "  tophash: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                           destTh[0], destTh[1], destTh[2], destTh[3],
                           destTh[4], destTh[5], destTh[6], destTh[7]);
                    dbglog(DBG_CRITICAL, "  dest bucket addr=%p\n", dest);
                    arch_exit();
                }

                for (int j = 0; j < MAP_BUCKET_COUNT; j++)
                {
                    if (isEmpty(destTh[j]))
                    {
                        destSlot = j;
                        break;
                    }
                }
                if (destSlot < 0)
                {
                    // Need overflow bucket (GC inhibited by caller)
                    void **overflow = bucketOverflow(t, dest);
                    if (*overflow == NULL)
                    {
                        *overflow = allocBuckets(t, 1);
                        h->noverflow++;
                    }
                    dest = *overflow;
                    destTh = bucketTophash(dest);
                }
            }

            // Copy entry to destination
            destTh[destSlot] = tophash(hash);
            keyCopy(t, bucketKey(t, dest, destSlot), keySlot);
            valueCopy(t, bucketValue(t, dest, destSlot), bucketValue(t, bucket, i));

            // Mark source as evacuated
            th[i] = useY ? MAP_EVACUATED_Y : MAP_EVACUATED_X;
        }

        bucket = *bucketOverflow(t, bucket);
    }

    // Update evacuation progress
    if (oldbucket == h->nevacuate)
    {
        advanceEvacuationProgress(t, h, oldCount);
    }
}

/**
 * Do incremental growth work
 *
 * BUG FIX: Uses nOldBuckets(h) instead of bucketCount(h->B - 1) to correctly
 * handle same-size growth where B is not incremented.
 *
 * NOTE: GC is inhibited by caller during map operations.
 */
static void growWork(MapType *t, GoMap *h, uintptr_t bucket)
{
    uintptr_t oldCount = nOldBuckets(h);

    // Evacuate bucket we're about to use
    evacuate(t, h, bucket & (oldCount - 1));

    // Evacuate one more to make progress
    if (isGrowing(h))
    {
        evacuate(t, h, h->nevacuate);
    }
}


/* --- Map Creation --- */


/**
 * Create a new map with size hint
 */
GoMap *runtime_makemap(MapType *t, intptr_t hint, GoMap *h)
{
    // Type validation
    if (t == NULL)
    {
        runtime_throw("makemap: nil type");
    }

    uint16_t bucketsize = MAPTYPE_BUCKETSIZE(t);

    if (bucketsize == 0)
    {
        runtime_throw("makemap: zero bucket size");
    }

    MAP_TRACE("makemap: hint=%ld, keysize=%u, elemsize=%u, bucketsize=%u",
              (long)hint, MAPTYPE_KEYSIZE(t), MAPTYPE_ELEMSIZE(t), MAPTYPE_BUCKETSIZE(t));

    // Initialize PRNG if needed
    if (!g_fastrand_initialized)
    {
        fastrand_init();
    }

    // Validate hint
    if (hint < 0)
    {
        hint = 0;
    }

    // Allocate map header if not provided
    if (h == NULL)
    {
        h = (GoMap *)gc_alloc(sizeof(GoMap), &__go_map_type);
        if (h == NULL)
        {
            runtime_throw("map header allocation failed");
        }
    }

    // Initialize map header
    memset(h, 0, sizeof(GoMap));
    h->hash0 = fastrand();

    // Calculate bucket count from hint
    uint8_t B = 0;
    while (overLoadFactor(hint, B))
    {
        B++;
        if (B > MAP_MAX_BUCKET_SHIFT)
        {
            runtime_panicstring("map size hint too large for Dreamcast");
        }
    }
    h->B = B;

    // Allocate initial buckets (inhibit GC to prevent pointer invalidation)
    if (B > 0)
    {
        gc_inhibit_collection();
        h->buckets = allocBuckets(t, bucketCount(B));
        gc_allow_collection();
    }

    MAP_TRACE("makemap: created map %p, B=%u, buckets=%lu", h, B, (unsigned long)bucketCount(B));

    return h;
}

/**
 * Create a new map with 64-bit size hint
 * gccgo signature: func makemap64(t *maptype, hint int64, h *hmap) *hmap
 *
 * This is called when the hint is larger than intptr_t max on 32-bit systems.
 * On Dreamcast (32-bit), we validate the hint fits and delegate to makemap.
 */
GoMap *runtime_makemap64(MapType *t, int64_t hint, GoMap *h) __asm__("_runtime.makemap64");
GoMap *runtime_makemap64(MapType *t, int64_t hint, GoMap *h)
{
    // On 32-bit systems, check if hint exceeds addressable range
    if (hint > INTPTR_MAX)
    {
        runtime_panicstring("makemap: size out of range");
    }
    return runtime_makemap(t, (intptr_t)hint, h);
}

/**
 * Create a new small map (no hint)
 *
 * IMPORTANT: gccgo's runtime.makemap__small takes NO parameters!
 * It just creates an empty map header. Buckets are allocated lazily
 * when the first item is inserted via mapassign.
 *
 * See libgo/go/runtime/map.go makemap_small()
 */
GoMap *runtime_makemap_small(void)
{
    // Allocate just the map header
    GoMap *h = (GoMap *)gc_alloc(sizeof(GoMap), &__go_map_type);
    if (h == NULL)
    {
        runtime_throw("runtime: cannot allocate map header");
    }

    // Initialize with empty state
    memset(h, 0, sizeof(GoMap));
    h->hash0 = map_fastrand(); // Random hash seed

    // B=0, count=0, buckets=NULL, etc - all zeroed
    // Buckets will be allocated lazily by mapassign when first item is added

    MAP_TRACE("makemap_small: h=%p, hash0=0x%lx", (void *)h, (unsigned long)h->hash0);

    return h;
}


/* --- Map Access --- */


/**
 * Look up key, return pointer to value or zero value
 */
void *runtime_mapaccess1(MapType *t, GoMap *h, void *key)
{
    // Type validation
    if (t == NULL)
    {
        runtime_throw("mapaccess1: nil type");
    }

    // Check hasher is valid (non-comparable types have NULL hasher)
    if (MAPTYPE_HASHER(t) == NULL)
    {
        runtime_panicstring("map key type is not comparable");
    }

    // Handle nil or empty map
    if (h == NULL || h->count == 0)
    {
        return zeroValue(MAPTYPE_ELEM(t));
    }

    // Concurrent write check
    if (h->flags & MAP_FLAG_WRITING)
    {
        runtime_throw("concurrent map read and map write");
    }

    // Compute hash
    uintptr_t hash = MAPTYPE_HASHER(t)(key, h->hash0);
    uintptr_t m = bucketMask(h->B);
    uint16_t bucketsize = MAPTYPE_BUCKETSIZE(t);

    // Get bucket
    void *b = bucketAt(h->buckets, hash & m, bucketsize);

    // Check old buckets during growth
    // BUG FIX: Use nOldBucketsMask(h) instead of bucketMask(h->B - 1)
    // to correctly handle same-size growth where B is not incremented
    if (isGrowing(h))
    {
        uintptr_t oldbucketMask = nOldBucketsMask(h);
        uintptr_t oldbucket = hash & oldbucketMask;
        void *oldb = bucketAt(h->oldbuckets, oldbucket, bucketsize);

        uint8_t firstTh = bucketTophash(oldb)[0];
        if (firstTh <= MAP_EMPTY_ONE || firstTh >= MAP_MIN_TOPHASH)
        {
            b = oldb; // Not evacuated, search old bucket
        }
    }

    uint8_t top = tophash(hash);

    // Search bucket chain
    while (b != NULL)
    {
        // Prefetch next bucket for cache efficiency
        void *overflow = *bucketOverflow(t, b);
        if (overflow != NULL)
        {
            PREFETCH(overflow);
        }

        uint8_t *th = bucketTophash(b);

        for (int i = 0; i < MAP_BUCKET_COUNT; i++)
        {
            if (th[i] != top)
            {
                if (th[i] == MAP_EMPTY_REST)
                {
                    return zeroValue(MAPTYPE_ELEM(t));
                }
                continue;
            }

            // Tophash matches, compare keys
            void *k = bucketKey(t, b, i);
            if (keyEqual(t, key, keyPtr(t, k)))
            {
                return valuePtr(t, bucketValue(t, b, i));
            }
        }

        b = overflow;
    }

    return zeroValue(MAPTYPE_ELEM(t));
}

/**
 * Look up key with existence check
 * Returns (value_ptr, ok) for SH-4 struct return
 */
MapAccess2Result runtime_mapaccess2(MapType *t, GoMap *h, void *key)
{
    MapAccess2Result result = {NULL, false};

    // Type validation
    if (t == NULL)
    {
        runtime_throw("mapaccess2: nil type");
    }

    // Check hasher is valid (non-comparable types have NULL hasher)
    if (MAPTYPE_HASHER(t) == NULL)
    {
        runtime_panicstring("map key type is not comparable");
    }

    // Handle nil or empty map
    if (h == NULL || h->count == 0)
    {
        result.value = zeroValue(MAPTYPE_ELEM(t));
        return result;
    }

    // Concurrent write check
    if (h->flags & MAP_FLAG_WRITING)
    {
        runtime_throw("concurrent map read and map write");
    }

    // Compute hash
    uintptr_t hash = MAPTYPE_HASHER(t)(key, h->hash0);
    uintptr_t m = bucketMask(h->B);
    uint16_t bucketsize = MAPTYPE_BUCKETSIZE(t);

    void *b = bucketAt(h->buckets, hash & m, bucketsize);

    // Check old buckets during growth
    // BUG FIX: Use nOldBucketsMask(h) instead of bucketMask(h->B - 1)
    // to correctly handle same-size growth where B is not incremented
    if (isGrowing(h))
    {
        uintptr_t oldbucketMask = nOldBucketsMask(h);
        uintptr_t oldbucket = hash & oldbucketMask;
        void *oldb = bucketAt(h->oldbuckets, oldbucket, bucketsize);

        uint8_t firstTh = bucketTophash(oldb)[0];
        if (firstTh <= MAP_EMPTY_ONE || firstTh >= MAP_MIN_TOPHASH)
        {
            b = oldb;
        }
    }

    uint8_t top = tophash(hash);

    while (b != NULL)
    {
        void *overflow = *bucketOverflow(t, b);
        if (overflow != NULL)
        {
            PREFETCH(overflow);
        }

        uint8_t *th = bucketTophash(b);

        for (int i = 0; i < MAP_BUCKET_COUNT; i++)
        {
            if (th[i] != top)
            {
                if (th[i] == MAP_EMPTY_REST)
                {
                    result.value = zeroValue(MAPTYPE_ELEM(t));
                    return result;
                }
                continue;
            }

            void *k = bucketKey(t, b, i);
            if (keyEqual(t, key, keyPtr(t, k)))
            {
                result.value = valuePtr(t, bucketValue(t, b, i));
                result.ok = true;
                return result;
            }
        }

        b = overflow;
    }

    result.value = zeroValue(MAPTYPE_ELEM(t));
    return result;
}


/* --- Map Assignment --- */


/*
 * runtime_mapassign - Get slot for assignment.
 *
 * Creates entry if needed, may trigger growth.
 * Returns pointer to value slot for caller to write into.
 *
 * GC SAFETY: We inhibit GC during map operations because our copying GC
 * moves heap objects. Map operations derive pointers from h and h->buckets
 * that would become stale if GC ran and moved those objects.
 */
void *runtime_mapassign(MapType *t, GoMap *h, void *key)
{
    uintptr_t hash, bucket;
    uint16_t bucketsize;
    void *b, *insertBucket, *searchBucket, *overflow, *k;
    uint8_t top, *th;
    int insertSlot, i;
    bool found_slot;
    void *result;

    /*
     * Nil map assignment is a panic (Go spec).
     */
    if (h == NULL) {
        runtime_panicstring("assignment to entry in nil map");
        return g_zero_value;
    }

    if (t == NULL || MAPTYPE_BUCKETSIZE(t) == 0)
        runtime_throw("mapassign: invalid type");

    if (MAPTYPE_HASHER(t) == NULL) {
        runtime_panicstring("map key type is not comparable");
        return g_zero_value;
    }

    if (h->flags & MAP_FLAG_WRITING)
        runtime_throw("concurrent map writes");

    hash = MAPTYPE_HASHER(t)(key, h->hash0);
    bucketsize = MAPTYPE_BUCKETSIZE(t);

    /*
     * Inhibit GC for the duration of this operation.
     * This prevents the copying GC from moving objects while we hold
     * derived pointers into h->buckets.
     */
    gc_inhibit_collection();

    if (h->buckets == NULL) {
        h->buckets = allocBuckets(t, 1);
        if (h->buckets == NULL) {
            gc_allow_collection();
            runtime_throw("mapassign: bucket allocation returned NULL");
        }
        h->B = 0;
    }

    h->flags |= MAP_FLAG_WRITING;

    /*
     * Main assignment loop. We may need to restart after growing.
     */
    for (;;) {
        if (isGrowing(h))
            growWork(t, h, hash & bucketMask(h->B));

        bucket = hash & bucketMask(h->B);
        b = bucketAt(h->buckets, bucket, bucketsize);
        top = tophash(hash);

        insertBucket = NULL;
        insertSlot = -1;
        found_slot = false;

        /* Search bucket chain for existing key or empty slot */
        for (searchBucket = b; searchBucket != NULL; searchBucket = overflow) {
            overflow = *bucketOverflow(t, searchBucket);
            if (overflow != NULL)
                PREFETCH(overflow);

            th = bucketTophash(searchBucket);

            for (i = 0; i < MAP_BUCKET_COUNT; i++) {
                if (th[i] != top) {
                    if (isEmpty(th[i]) && insertSlot < 0) {
                        insertBucket = searchBucket;
                        insertSlot = i;
                    }
                    if (th[i] == MAP_EMPTY_REST) {
                        found_slot = true;
                        break;
                    }
                    continue;
                }

                /* Tophash matches - compare actual key */
                k = bucketKey(t, searchBucket, i);
                if (!keyEqual(t, key, keyPtr(t, k)))
                    continue;

                /* Found existing key - update in place */
                if (MAPTYPE_FLAGS(t) & MAPTYPE_NEED_KEY_UPDATE)
                    keyCopy(t, k, key);

                h->flags &= ~MAP_FLAG_WRITING;
                result = valuePtr(t, bucketValue(t, searchBucket, i));
                gc_allow_collection();
                return result;
            }

            if (found_slot)
                break;
        }

        /* Key not found. Check if we need to grow first. */
        if (!isGrowing(h) &&
            (overLoadFactor(h->count + 1, h->B) ||
             tooManyOverflowBuckets(h->noverflow, h->B))) {
            hashGrow(t, h);
            continue;  /* Restart search after growing */
        }

        break;  /* Ready to insert */
    }

    /* Allocate overflow bucket if no empty slot found */
    if (insertSlot < 0) {
        void *chainEnd = b;
        while (*bucketOverflow(t, chainEnd) != NULL)
            chainEnd = *bucketOverflow(t, chainEnd);

        void *newb = allocBuckets(t, 1);
        *bucketOverflow(t, chainEnd) = newb;
        insertBucket = newb;
        insertSlot = 0;
        h->noverflow++;
    }

    /* Insert new entry */
    th = bucketTophash(insertBucket);
    th[insertSlot] = top;
    keyCopy(t, bucketKey(t, insertBucket, insertSlot), key);
    h->count++;

    h->flags &= ~MAP_FLAG_WRITING;
    result = valuePtr(t, bucketValue(t, insertBucket, insertSlot));
    gc_allow_collection();
    return result;
}


/* --- Map Deletion --- */


/**
 * Delete key from map
 *
 * GC SAFETY: We inhibit GC because growWork can allocate overflow buckets.
 */
void runtime_mapdelete(MapType *t, GoMap *h, void *key)
{
    // Nil or empty map - nothing to delete
    if (h == NULL || h->count == 0)
    {
        return;
    }

    // Type validation
    if (t == NULL)
    {
        runtime_throw("mapdelete: nil type");
    }

    // Check hasher is valid (non-comparable types have NULL hasher)
    if (MAPTYPE_HASHER(t) == NULL)
    {
        runtime_panicstring("map key type is not comparable");
    }

    // Concurrent write check
    if (h->flags & MAP_FLAG_WRITING)
    {
        runtime_throw("concurrent map writes");
    }

    // Compute hash
    uintptr_t hash = MAPTYPE_HASHER(t)(key, h->hash0);
    uint16_t bucketsize = MAPTYPE_BUCKETSIZE(t);

    // Inhibit GC during map operation
    gc_inhibit_collection();

    // Set writing flag
    h->flags |= MAP_FLAG_WRITING;

    // Handle ongoing growth
    if (isGrowing(h))
    {
        growWork(t, h, hash & bucketMask(h->B));
    }

    uintptr_t bucket = hash & bucketMask(h->B);
    void *b = bucketAt(h->buckets, bucket, bucketsize);

    // Check old buckets during growth (key might be there)
    // BUG FIX: Use nOldBucketsMask(h) instead of bucketMask(h->B - 1)
    // to correctly handle same-size growth where B is not incremented
    if (isGrowing(h))
    {
        uintptr_t oldbucketMask = nOldBucketsMask(h);
        uintptr_t oldbucket = hash & oldbucketMask;
        void *oldb = bucketAt(h->oldbuckets, oldbucket, bucketsize);

        uint8_t firstTh = bucketTophash(oldb)[0];
        if (firstTh <= MAP_EMPTY_ONE || firstTh >= MAP_MIN_TOPHASH)
        {
            b = oldb; // Not evacuated, search old bucket
        }
    }

    uint8_t top = tophash(hash);

    // Search for key
    while (b != NULL)
    {
        void *overflow = *bucketOverflow(t, b);
        if (overflow != NULL)
        {
            PREFETCH(overflow);
        }

        uint8_t *th = bucketTophash(b);

        for (int i = 0; i < MAP_BUCKET_COUNT; i++)
        {
            if (th[i] != top)
            {
                if (th[i] == MAP_EMPTY_REST)
                {
                    goto done;
                }
                continue;
            }

            void *k = bucketKey(t, b, i);
            if (!keyEqual(t, key, keyPtr(t, k)))
            {
                continue;
            }

            // Found key - clear the slot

            // Clear key if contains pointers (for GC)
            if (MAPTYPE_KEY(t)->__ptrdata > 0)
            {
                memset(bucketKey(t, b, i), 0, MAPTYPE_KEYSIZE(t));
            }

            // Clear value if contains pointers (for GC)
            if (MAPTYPE_ELEM(t)->__ptrdata > 0)
            {
                memset(bucketValue(t, b, i), 0, MAPTYPE_ELEMSIZE(t));
            }

            // Mark slot as empty
            th[i] = MAP_EMPTY_ONE;

            // Optimize: set EMPTY_REST if rest is empty
            bool restEmpty = true;
            for (int j = i + 1; j < MAP_BUCKET_COUNT; j++)
            {
                if (!isEmpty(th[j]))
                {
                    restEmpty = false;
                    break;
                }
            }
            if (restEmpty && *bucketOverflow(t, b) == NULL)
            {
                th[i] = MAP_EMPTY_REST;
            }

            h->count--;
            MAP_TRACE("mapdelete: deleted key, count now %lu", (unsigned long)h->count);
            goto done;
        }

        b = overflow;
    }

done:
    h->flags &= ~MAP_FLAG_WRITING;
    gc_allow_collection();
}


/* --- Map Length --- */


/**
 * Get number of entries in map
 */
intptr_t runtime_maplen(GoMap *h)
{
    if (h == NULL)
    {
        return 0;
    }
    return (intptr_t)h->count;
}


/* --- Map Iteration (Stub for Phase 2) --- */


/**
 * Initialize map iterator
 * Full implementation in Phase 2
 */
void runtime_mapiterinit(MapType *t, GoMap *h, MapIter *it)
{
    memset(it, 0, sizeof(MapIter));
    it->t = t;
    it->h = h;

    if (h == NULL || h->count == 0)
    {
        return;
    }

    // Set iterator flag
    if (h->flags & MAP_FLAG_WRITING)
    {
        runtime_throw("concurrent map iteration and map write");
    }

    it->B = h->B;
    it->buckets = h->buckets;

    // Randomize starting position
    uint32_t r = fastrand();
    it->startBucket = r & bucketMask(h->B);
    it->offset = (uint8_t)(r >> h->B) & (MAP_BUCKET_COUNT - 1);
    it->bucket = it->startBucket;

    // Mark iterator active
    h->flags |= MAP_FLAG_ITERATOR;
    if (h->oldbuckets != NULL)
    {
        h->flags |= MAP_FLAG_OLD_ITERATOR;
    }

    // Move to first valid entry
    runtime_mapiternext(it);
}

/**
 * Advance iterator to next entry
 *
 * KEY ALGORITHM: Uses offset as a rotation for slot indices, NOT a start index.
 * This matches Go runtime's map.go:
 *   offi := (i + it.offset) & (bucketCnt - 1)
 *
 * The offset rotates the order in which slots are visited within each bucket.
 * When i goes 0..7, offi cycles through all 8 slots but starting at offset.
 * This ensures all entries are visited exactly once regardless of starting position.
 */
void runtime_mapiternext(MapIter *it)
{
    GoMap *h = it->h;
    MapType *t = it->t;

    if (h == NULL || t == NULL)
    {
        return;
    }

    // Consistency check: if map started growing after iteration began,
    // we need to be careful. The iterator's B and buckets snapshot may be stale.
    // Go's runtime handles this with more complex logic; we take the simpler
    // approach of checking if growth started and refreshing our view.
    if (h->oldbuckets != NULL && it->buckets == h->oldbuckets)
    {
        // Map grew and our snapshot is now oldbuckets - refresh to new buckets
        // This may cause us to revisit some entries, but won't miss any.
        it->B = h->B;
        it->buckets = h->buckets;
        // Reset iteration to start (simpler than trying to map old position)
        it->bucket = 0;
        it->startBucket = 0;
        it->wrapped = false;
        it->bptr = NULL;
        it->i = 0;
    }

    uint16_t bucketsize = MAPTYPE_BUCKETSIZE(t);
    void *b = it->bptr;
    uintptr_t bucket = it->bucket;
    uint8_t i = it->i;

next:
    // If no current bucket, get the bucket at current index
    if (b == NULL)
    {
        // Check termination condition BEFORE entering bucket
        if (bucket == it->startBucket && it->wrapped)
        {
            // Completed full iteration
            it->key = NULL;
            it->elem = NULL;
            return;
        }

        b = bucketAt(it->buckets, bucket, bucketsize);
        i = 0; // Always start from 0 - offset provides rotation
    }

    // Search current bucket using offset rotation
    for (; i < MAP_BUCKET_COUNT; i++)
    {
        // KEY FIX: Use offset as rotation, not start index
        // This ensures all 8 slots are visited in each bucket
        uint8_t offi = (i + it->offset) & (MAP_BUCKET_COUNT - 1);

        uint8_t *th = bucketTophash(b);

        if (isEmpty(th[offi]) || th[offi] == MAP_EVACUATED_EMPTY)
        {
            continue;
        }

        // During growth, check if entry moved
        if (th[offi] == MAP_EVACUATED_X || th[offi] == MAP_EVACUATED_Y)
        {
            continue;
        }

        // Found valid entry - use offi for key/value access
        void *k = bucketKey(t, b, offi);
        it->key = keyPtr(t, k);
        it->elem = valuePtr(t, bucketValue(t, b, offi));
        it->bucket = bucket;
        it->i = i + 1; // Store i (not offi) for next iteration
        it->bptr = b;
        return;
    }

    // Move to overflow bucket
    b = *bucketOverflow(t, b);
    if (b != NULL)
    {
        i = 0;
        goto next;
    }

    // Move to next bucket
    bucket++;
    if (bucket == bucketCount(it->B))
    {
        bucket = 0;
        it->wrapped = true;
    }

    // Set b to NULL so we check termination at top of loop
    b = NULL;
    i = 0;
    it->bucket = bucket;
    it->bptr = NULL;
    goto next;
}


// Fast-Path Functions for gccgo


/* --- Fast-Path Functions for gccgo --- */
// common key types like int32 and string.

// Fast-path functions use the shared g_zero_value buffer defined above.

/*
 * Hash function wrappers for fast-path map operations.
 *
 * These MUST use the same algorithm as runtime_memhash32..f etc in
 * runtime_stubs.c. Rather than duplicate the algorithm (and risk
 * divergence), we call the canonical implementations directly.
 *
 * The canonical functions take a pointer; we take value and pass &tmp.
 * The compiler will optimize this to a single register load.
 */
/* Import hash functions with gccgo symbol names */
extern uintptr_t runtime_memhash32(void *key, uintptr_t seed) __asm__("_runtime.memhash32..f");
extern uintptr_t runtime_memhash64(void *key, uintptr_t seed) __asm__("_runtime.memhash64..f");
extern uintptr_t runtime_strhash(void *key, uintptr_t seed) __asm__("_runtime.strhash..f");

MAP_INLINE uintptr_t fast32_hash(uint32_t key, uintptr_t seed)
{
    return runtime_memhash32(&key, seed);
}

MAP_INLINE uintptr_t faststr_hash(GoString key, uintptr_t seed)
{
    return runtime_strhash(&key, seed);
}

// Note: runtime_makemap_small is defined in the main map creation section
// and provides the runtime.makemap_small symbol needed by gccgo.

// Fast-Path Functions Generated Using Macros (map_fast_internal.h)

//
// The access and delete fast-paths are generated using X-macros to reduce
// code duplication. The assign fast-paths are kept as manual implementations
// due to their `goto again` loops which don't macro-ize cleanly.
//
// See map_fast_internal.h for the macro definitions.

#include "map_fast_internal.h"

// Result types for access2 functions
struct mapaccess2_fast32_result { void *val; bool ok; };
struct mapaccess2_fast64_result { void *val; bool ok; };

// _runtime.mapassign__fast32 - Fast path for int32 key assignment (generated)
void *runtime_mapassign_fast32(MapType *t, GoMap *h, uint32_t key) __asm__("_runtime.mapassign__fast32");
MAP_FAST_ASSIGN(fast32, uint32_t, fast32_hash, KEY_CMP_UINT32(k, key), KEY_CAST_UINT32, KEY_ASSIGN_UINT32)

// _runtime.mapaccess1__fast32 - Generated using MAP_FAST_ACCESS1
void *runtime_mapaccess1_fast32(MapType *t, GoMap *h, uint32_t key) __asm__("_runtime.mapaccess1__fast32");
MAP_FAST_ACCESS1(fast32, uint32_t, fast32_hash, KEY_CMP_UINT32(k, key), KEY_CAST_UINT32)

// _runtime.mapaccess2__fast32 - Generated using MAP_FAST_ACCESS2
struct mapaccess2_fast32_result runtime_mapaccess2_fast32(MapType *t, GoMap *h, uint32_t key) __asm__("_runtime.mapaccess2__fast32");
MAP_FAST_ACCESS2(fast32, uint32_t, fast32_hash, KEY_CMP_UINT32(k, key), KEY_CAST_UINT32, struct mapaccess2_fast32_result)

// _runtime.mapdelete__fast32 - Generated using MAP_FAST_DELETE
void runtime_mapdelete_fast32(MapType *t, GoMap *h, uint32_t key) __asm__("_runtime.mapdelete__fast32");
MAP_FAST_DELETE(fast32, uint32_t, fast32_hash, KEY_CMP_UINT32(k, key), KEY_CAST_UINT32)

// _runtime.mapassign__faststr - Fast path for string key assignment (generated)
void *runtime_mapassign_faststr(MapType *t, GoMap *h, GoString key) __asm__("_runtime.mapassign__faststr");
MAP_FAST_ASSIGN(faststr, GoString, faststr_hash, KEY_CMP_STRING(k, key), KEY_CAST_STRING, KEY_ASSIGN_STRING)

// _runtime.mapaccess1__faststr - Generated using MAP_FAST_ACCESS1
void *runtime_mapaccess1_faststr(MapType *t, GoMap *h, GoString key) __asm__("_runtime.mapaccess1__faststr");
MAP_FAST_ACCESS1(faststr, GoString, faststr_hash, KEY_CMP_STRING(k, key), KEY_CAST_STRING)


/* --- Map Fast Path Stubs --- */


MAP_INLINE uintptr_t fast64_hash(uint64_t key, uintptr_t seed)
{
    return runtime_memhash64(&key, seed);
}

// _runtime.mapaccess1__fast64 - Generated using MAP_FAST_ACCESS1
void *runtime_mapaccess1_fast64(MapType *t, GoMap *h, uint64_t key) __asm__("_runtime.mapaccess1__fast64");
MAP_FAST_ACCESS1(fast64, uint64_t, fast64_hash, KEY_CMP_UINT64(k, key), KEY_CAST_UINT64)

// _runtime.mapaccess2__fast64 - Generated using MAP_FAST_ACCESS2
struct mapaccess2_fast64_result runtime_mapaccess2_fast64(MapType *t, GoMap *h, uint64_t key) __asm__("_runtime.mapaccess2__fast64");
MAP_FAST_ACCESS2(fast64, uint64_t, fast64_hash, KEY_CMP_UINT64(k, key), KEY_CAST_UINT64, struct mapaccess2_fast64_result)

// _runtime.mapassign__fast64 - Fast path for int64 key assignment (generated)
void *runtime_mapassign_fast64(MapType *t, GoMap *h, uint64_t key) __asm__("_runtime.mapassign__fast64");
MAP_FAST_ASSIGN(fast64, uint64_t, fast64_hash, KEY_CMP_UINT64(k, key), KEY_CAST_UINT64, KEY_ASSIGN_UINT64)

// _runtime.mapdelete__fast64 - Generated using MAP_FAST_DELETE
void runtime_mapdelete_fast64(MapType *t, GoMap *h, uint64_t key) __asm__("_runtime.mapdelete__fast64");
MAP_FAST_DELETE(fast64, uint64_t, fast64_hash, KEY_CMP_UINT64(k, key), KEY_CAST_UINT64)

// _runtime.mapdelete__faststr - Generated using MAP_FAST_DELETE
void runtime_mapdelete_faststr(MapType *t, GoMap *h, GoString key) __asm__("_runtime.mapdelete__faststr");
MAP_FAST_DELETE(faststr, GoString, faststr_hash, KEY_CMP_STRING(k, key), KEY_CAST_STRING)

// mapaccess2_faststr - fast path for string key maps with existence check
MapAccess2Result runtime_mapaccess2_faststr(MapType *t, GoMap *h, GoString key) __asm__("_runtime.mapaccess2__faststr");
MapAccess2Result runtime_mapaccess2_faststr(MapType *t, GoMap *h, GoString key)
{
    MapAccess2Result result = {NULL, false};
    if (h == NULL || h->count == 0)
    {
        result.value = zeroValue(MAPTYPE_ELEM(t));
        return result;
    }
    // Forward to generic implementation
    return runtime_mapaccess2(t, h, &key);
}


/* --- Fat Map Access Functions --- */


/*
 * runtime_mapaccess1_fat - Access with caller-provided zero value.
 *
 * Used for large value types where the built-in zero buffer is insufficient.
 */
void *runtime_mapaccess1_fat(MapType *t, GoMap *h, void *key, void *zero)
	__asm__("_runtime.mapaccess1__fat");
void *runtime_mapaccess1_fat(MapType *t, GoMap *h, void *key, void *zero)
{
	MapAccess2Result result;

	if (zero == NULL)
		runtime_throw("mapaccess1_fat: nil zero value");

	result = runtime_mapaccess2(t, h, key);
	if (!result.ok)
		return zero;
	return result.value;
}

/*
 * runtime_mapaccess2_fat - Access with ok flag and caller-provided zero value.
 */
MapAccess2Result runtime_mapaccess2_fat(MapType *t, GoMap *h, void *key, void *zero)
	__asm__("_runtime.mapaccess2__fat");
MapAccess2Result runtime_mapaccess2_fat(MapType *t, GoMap *h, void *key, void *zero)
{
	MapAccess2Result result;

	if (zero == NULL)
		runtime_throw("mapaccess2_fat: nil zero value");

	result = runtime_mapaccess2(t, h, key);
	if (!result.ok)
		result.value = zero;
	return result;
}


// Pointer-Key Map Fast Paths


// _runtime.mapassign__fast32ptr - Fast path for 32-bit pointer key assignment
/* Pointer keys are hashed by their address value */
void *runtime_mapassign_fast32ptr(MapType *t, GoMap *h, void *key) __asm__("_runtime.mapassign__fast32ptr");
void *runtime_mapassign_fast32ptr(MapType *t, GoMap *h, void *key)
{
    // Use the pointer value as a 32-bit key
    return runtime_mapassign_fast32(t, h, (uint32_t)(uintptr_t)key);
}

// _runtime.mapassign__fast64ptr - Fast path for 64-bit pointer key assignment
// On 32-bit systems like Dreamcast, this is the same as fast32ptr
void *runtime_mapassign_fast64ptr(MapType *t, GoMap *h, void *key) __asm__("_runtime.mapassign__fast64ptr");
void *runtime_mapassign_fast64ptr(MapType *t, GoMap *h, void *key)
{
#if UINTPTR_MAX == 0xFFFFFFFF
    // 32-bit system: use 32-bit fast path
    return runtime_mapassign_fast32(t, h, (uint32_t)(uintptr_t)key);
#else
    // 64-bit system: use 64-bit fast path
    return runtime_mapassign_fast64(t, h, (uint64_t)(uintptr_t)key);
#endif
}

// mapclear - clear all entries from map
void runtime_mapclear(MapType *t, GoMap *h) __asm__("_runtime.mapclear");
void runtime_mapclear(MapType *t, GoMap *h)
{
    if (h == NULL || h->count == 0)
        return;

    // Reset count
    h->count = 0;
    h->noverflow = 0;
    h->nevacuate = 0;

    // Clear all buckets
    if (h->buckets != NULL)
    {
        uint16_t bucketsize = MAPTYPE_BUCKETSIZE(t);
        size_t num_buckets = (size_t)1 << h->B;
        memset(h->buckets, 0, bucketsize * num_buckets);
    }

    // Clear old buckets if growing
    h->oldbuckets = NULL;
    h->flags &= ~(MAP_FLAG_SAME_SIZE_GROW);
}


/* --- Initialization --- */


/**
 * Initialize map subsystem
 */
void map_init(void)
{
    // Zero the zero-value buffer
    memset(g_zero_value, 0, sizeof(g_zero_value));

    // Initialize PRNG
    fastrand_init();

    MAP_TRACE("map subsystem initialized");
}
