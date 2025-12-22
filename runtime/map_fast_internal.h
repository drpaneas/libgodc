/**
 * map_fast_internal.h - Internal macros for map fast-path code generation
 *
 * This header provides X-macros to reduce code duplication in the map
 * fast-path functions. Each fast-path operation (access, access2, assign,
 * delete) for each key type (32-bit, 64-bit, string) shares 90% of the
 * same logic.
 *
 * WARNING: This header is ONLY meant to be included from map_dreamcast.c.
 * It relies on static functions and variables defined there. Do not include
 * from other translation units.
 *
 * Usage: Include this header and use the MAP_FAST_* macros to generate
 * the fast-path functions.
 */

#ifndef MAP_FAST_INTERNAL_H
#define MAP_FAST_INTERNAL_H

#include "map_dreamcast.h"
#include "godc_config.h"
#include "runtime.h"   /* For GoString, runtime_throw */
#include <string.h>    /* For memset, memcmp */

/*
 * g_zero_value is declared static in map_dreamcast.c.
 * These macros MUST only be used in that file.
 */
#ifndef MAP_DREAMCAST_C_INCLUDED
#error "map_fast_internal.h must only be included from map_dreamcast.c"
#endif

/* Common preamble for all fast-path functions */
#define MAP_FAST_CHECK_NIL_MAP(h, zero_ret) \
	if ((h) == NULL || (h)->count == 0) return (zero_ret)

#define MAP_FAST_CHECK_CONCURRENT_READ(h) \
	if ((h)->flags & MAP_FLAG_WRITING) \
		runtime_throw("concurrent map read and map write")

#define MAP_FAST_CHECK_CONCURRENT_WRITE(h) \
	if ((h)->flags & MAP_FLAG_WRITING) \
		runtime_throw("concurrent map writes")

/*
 * MAP_FAST_ACCESS1 - Generate mapaccess1 fast-path function body
 *
 * Parameters:
 *   suffix    - Function name suffix (fast32, fast64, faststr)
 *   keytype   - C type of key (uint32_t, uint64_t, GoString)
 *   hashfn    - Hash function to use
 *   keycmp    - Key comparison expression (receives k, key as params)
 *   keycast   - Cast expression for bucket key pointer
 *
 * Note: The function declaration (with asm label) should precede this macro.
 */
#define MAP_FAST_ACCESS1(suffix, keytype, hashfn, keycmp, keycast)              \
void *runtime_mapaccess1_##suffix(MapType *t, GoMap *h, keytype key)            \
{                                                                               \
	MAP_FAST_CHECK_NIL_MAP(h, g_zero_value);                                \
	MAP_FAST_CHECK_CONCURRENT_READ(h);                                      \
	                                                                        \
	uintptr_t hash = hashfn(key, h->hash0);                                 \
	uint16_t bucketsize = MAPTYPE_BUCKETSIZE(t);                            \
	uintptr_t m = bucketMask(h->B);                                         \
	void *b = bucketAt(h->buckets, hash & m, bucketsize);                   \
	                                                                        \
	if (isGrowing(h)) {                                                     \
		uintptr_t oldbucketMask = nOldBucketsMask(h);                   \
		void *oldb = bucketAt(h->oldbuckets, hash & oldbucketMask,      \
		                      bucketsize);                              \
		uint8_t firstTh = bucketTophash(oldb)[0];                       \
		if (firstTh <= MAP_EMPTY_ONE || firstTh >= MAP_MIN_TOPHASH)     \
			b = oldb;                                               \
	}                                                                       \
	                                                                        \
	uint8_t top = tophash(hash);                                            \
	                                                                        \
	while (b != NULL) {                                                     \
		uint8_t *th = bucketTophash(b);                                 \
		for (int i = 0; i < MAP_BUCKET_COUNT; i++) {                    \
			if (th[i] != top) {                                     \
				if (th[i] == MAP_EMPTY_REST)                    \
					return g_zero_value;                    \
				continue;                                       \
			}                                                       \
			keytype *k = keycast(bucketKey(t, b, i));               \
			if (keycmp)                                             \
				return bucketValue(t, b, i);                    \
		}                                                               \
		b = *bucketOverflow(t, b);                                      \
	}                                                                       \
	return g_zero_value;                                                    \
}

/*
 * MAP_FAST_ACCESS2 - Generate mapaccess2 fast-path (with ok flag)
 *
 * Note: The function declaration (with asm label) and result_type struct
 * should precede this macro.
 */
#define MAP_FAST_ACCESS2(suffix, keytype, hashfn, keycmp, keycast, result_type) \
result_type runtime_mapaccess2_##suffix(MapType *t, GoMap *h, keytype key)      \
{                                                                               \
	result_type result = {g_zero_value, false};                             \
	MAP_FAST_CHECK_NIL_MAP(h, result);                                      \
	MAP_FAST_CHECK_CONCURRENT_READ(h);                                      \
	                                                                        \
	uintptr_t hash = hashfn(key, h->hash0);                                 \
	uint16_t bucketsize = MAPTYPE_BUCKETSIZE(t);                            \
	uintptr_t m = bucketMask(h->B);                                         \
	void *b = bucketAt(h->buckets, hash & m, bucketsize);                   \
	                                                                        \
	if (isGrowing(h)) {                                                     \
		uintptr_t oldbucketMask = nOldBucketsMask(h);                   \
		void *oldb = bucketAt(h->oldbuckets, hash & oldbucketMask,      \
		                      bucketsize);                              \
		uint8_t firstTh = bucketTophash(oldb)[0];                       \
		if (firstTh <= MAP_EMPTY_ONE || firstTh >= MAP_MIN_TOPHASH)     \
			b = oldb;                                               \
	}                                                                       \
	                                                                        \
	uint8_t top = tophash(hash);                                            \
	                                                                        \
	while (b != NULL) {                                                     \
		uint8_t *th = bucketTophash(b);                                 \
		for (int i = 0; i < MAP_BUCKET_COUNT; i++) {                    \
			if (th[i] != top) {                                     \
				if (th[i] == MAP_EMPTY_REST)                    \
					return result;                          \
				continue;                                       \
			}                                                       \
			keytype *k = keycast(bucketKey(t, b, i));               \
			if (keycmp) {                                           \
				result.val = bucketValue(t, b, i);              \
				result.ok = true;                               \
				return result;                                  \
			}                                                       \
		}                                                               \
		b = *bucketOverflow(t, b);                                      \
	}                                                                       \
	return result;                                                          \
}

/*
 * MAP_FAST_DELETE - Generate mapdelete fast-path
 *
 * Note: The function declaration (with asm label) should precede this macro.
 */
#define MAP_FAST_DELETE(suffix, keytype, hashfn, keycmp, keycast)               \
void runtime_mapdelete_##suffix(MapType *t, GoMap *h, keytype key)              \
{                                                                               \
	if (h == NULL || h->count == 0)                                         \
		return;                                                         \
	MAP_FAST_CHECK_CONCURRENT_WRITE(h);                                     \
	                                                                        \
	gc_inhibit_collection();                                                \
	h->flags |= MAP_FLAG_WRITING;                                           \
	                                                                        \
	uintptr_t hash = hashfn(key, h->hash0);                                 \
	uint16_t bucketsize = MAPTYPE_BUCKETSIZE(t);                            \
	                                                                        \
	if (isGrowing(h))                                                       \
		growWork(t, h, hash & bucketMask(h->B));                        \
	                                                                        \
	uintptr_t bucket = hash & bucketMask(h->B);                             \
	void *b = bucketAt(h->buckets, bucket, bucketsize);                     \
	                                                                        \
	if (isGrowing(h)) {                                                     \
		uintptr_t oldbucketMask = nOldBucketsMask(h);                   \
		void *oldb = bucketAt(h->oldbuckets, hash & oldbucketMask,      \
		                      bucketsize);                              \
		uint8_t firstTh = bucketTophash(oldb)[0];                       \
		if (firstTh <= MAP_EMPTY_ONE || firstTh >= MAP_MIN_TOPHASH)     \
			b = oldb;                                               \
	}                                                                       \
	                                                                        \
	uint8_t top = tophash(hash);                                            \
	                                                                        \
	while (b != NULL) {                                                     \
		uint8_t *th = bucketTophash(b);                                 \
		for (int i = 0; i < MAP_BUCKET_COUNT; i++) {                    \
			if (th[i] != top) {                                     \
				if (th[i] == MAP_EMPTY_REST)                    \
					goto done;                              \
				continue;                                       \
			}                                                       \
			keytype *k = keycast(bucketKey(t, b, i));               \
			if (!(keycmp))                                          \
				continue;                                       \
			                                                        \
			memset(bucketKey(t, b, i), 0, MAPTYPE_KEYSIZE(t));      \
			if (MAPTYPE_ELEM(t)->__ptrdata > 0)                     \
				memset(bucketValue(t, b, i), 0,                 \
				       MAPTYPE_ELEMSIZE(t));                    \
			                                                        \
			th[i] = MAP_EMPTY_ONE;                                  \
			                                                        \
			/* Check if rest of bucket is empty */                  \
			bool restEmpty = true;                                  \
			for (int j = i + 1; j < MAP_BUCKET_COUNT; j++) {        \
				if (!isEmpty(th[j])) {                          \
					restEmpty = false;                      \
					break;                                  \
				}                                               \
			}                                                       \
			if (restEmpty && *bucketOverflow(t, b) == NULL)         \
				th[i] = MAP_EMPTY_REST;                         \
			                                                        \
			h->count--;                                             \
			goto done;                                              \
		}                                                               \
		b = *bucketOverflow(t, b);                                      \
	}                                                                       \
done:                                                                           \
	h->flags &= ~MAP_FLAG_WRITING;                                          \
	gc_allow_collection();                                                  \
}

/*
 * MAP_FAST_ASSIGN - Generate mapassign fast-path
 *
 * This is the most complex fast-path due to the retry-after-growth logic.
 * The macro generates the entire function body.
 *
 * Parameters:
 *   suffix     - Function name suffix (fast32, fast64, faststr)
 *   keytype    - C type of key (uint32_t, uint64_t, GoString)
 *   hashfn     - Hash function to use
 *   keycmp     - Key comparison expression (receives k, key as params)
 *   keycast    - Cast expression for bucket key pointer
 *   keyassign  - Key assignment expression (receives dst, key)
 */
#define MAP_FAST_ASSIGN(suffix, keytype, hashfn, keycmp, keycast, keyassign)     \
void *runtime_mapassign_##suffix(MapType *t, GoMap *h, keytype key)              \
{                                                                                \
    uintptr_t hash, bucket;                                                      \
    uint16_t bucketsize;                                                         \
    void *b, *insertBucket, *searchBucket, *result;                              \
    uint8_t top, *th;                                                            \
    int insertSlot, i;                                                           \
    bool found_slot;                                                             \
                                                                                 \
    if (h == NULL) {                                                             \
        runtime_panicstring("assignment to entry in nil map");                   \
        return g_zero_value;                                                     \
    }                                                                            \
    MAP_FAST_CHECK_CONCURRENT_WRITE(h);                                          \
                                                                                 \
    gc_inhibit_collection();                                                     \
                                                                                 \
    if (h->buckets == NULL) {                                                    \
        h->buckets = allocBuckets(t, 1);                                         \
        if (h->buckets == NULL) {                                                \
            gc_allow_collection();                                               \
            runtime_throw("mapassign_fast: bucket allocation returned NULL");    \
        }                                                                        \
        h->B = 0;                                                                \
    }                                                                            \
                                                                                 \
    h->flags |= MAP_FLAG_WRITING;                                                \
                                                                                 \
    hash = hashfn(key, h->hash0);                                                \
    bucketsize = MAPTYPE_BUCKETSIZE(t);                                          \
                                                                                 \
    for (;;) {                                                                   \
        if (isGrowing(h))                                                        \
            growWork(t, h, hash & bucketMask(h->B));                             \
                                                                                 \
        bucket = hash & bucketMask(h->B);                                        \
        b = bucketAt(h->buckets, bucket, bucketsize);                            \
        top = tophash(hash);                                                     \
                                                                                 \
        insertBucket = NULL;                                                     \
        insertSlot = -1;                                                         \
        found_slot = false;                                                      \
                                                                                 \
        for (searchBucket = b; searchBucket != NULL;                             \
             searchBucket = *bucketOverflow(t, searchBucket)) {                  \
            th = bucketTophash(searchBucket);                                    \
                                                                                 \
            for (i = 0; i < MAP_BUCKET_COUNT; i++) {                             \
                if (th[i] != top) {                                              \
                    if (isEmpty(th[i]) && insertSlot < 0) {                      \
                        insertBucket = searchBucket;                             \
                        insertSlot = i;                                          \
                    }                                                            \
                    if (th[i] == MAP_EMPTY_REST) {                               \
                        found_slot = true;                                       \
                        break;                                                   \
                    }                                                            \
                    continue;                                                    \
                }                                                                \
                                                                                 \
                keytype *k = keycast(bucketKey(t, searchBucket, i));             \
                if (keycmp) {                                                    \
                    h->flags &= ~MAP_FLAG_WRITING;                               \
                    result = bucketValue(t, searchBucket, i);                    \
                    gc_allow_collection();                                       \
                    return result;                                               \
                }                                                                \
            }                                                                    \
            if (found_slot)                                                      \
                break;                                                           \
        }                                                                        \
                                                                                 \
        if (!isGrowing(h) &&                                                     \
            (overLoadFactor(h->count + 1, h->B) ||                               \
             tooManyOverflowBuckets(h->noverflow, h->B))) {                      \
            hashGrow(t, h);                                                      \
            continue;                                                            \
        }                                                                        \
        break;                                                                   \
    }                                                                            \
                                                                                 \
    if (insertSlot < 0) {                                                        \
        void *chainEnd = b;                                                      \
        while (*bucketOverflow(t, chainEnd) != NULL)                             \
            chainEnd = *bucketOverflow(t, chainEnd);                             \
        void *newb = allocBuckets(t, 1);                                         \
        *bucketOverflow(t, chainEnd) = newb;                                     \
        insertBucket = newb;                                                     \
        insertSlot = 0;                                                          \
        h->noverflow++;                                                          \
    }                                                                            \
                                                                                 \
    th = bucketTophash(insertBucket);                                            \
    th[insertSlot] = top;                                                        \
    keyassign(bucketKey(t, insertBucket, insertSlot), key);                      \
    h->count++;                                                                  \
                                                                                 \
    h->flags &= ~MAP_FLAG_WRITING;                                               \
    result = bucketValue(t, insertBucket, insertSlot);                           \
    gc_allow_collection();                                                       \
    return result;                                                               \
}

/* Key comparison macros */
#define KEY_CMP_UINT32(k, key) (*k == key)
#define KEY_CMP_UINT64(k, key) (*k == key)
#define KEY_CMP_STRING(k, key) \
    (k->len == key.len && (key.len == 0 || memcmp(k->str, key.str, key.len) == 0))

/* Key cast macros */
#define KEY_CAST_UINT32(ptr) ((uint32_t *)(ptr))
#define KEY_CAST_UINT64(ptr) ((uint64_t *)(ptr))
#define KEY_CAST_STRING(ptr) ((GoString *)(ptr))

/* Key assignment macros */
#define KEY_ASSIGN_UINT32(dst, key) (*(uint32_t *)(dst) = (key))
#define KEY_ASSIGN_UINT64(dst, key) (*(uint64_t *)(dst) = (key))
#define KEY_ASSIGN_STRING(dst, key) (*(GoString *)(dst) = (key))

#endif /* MAP_FAST_INTERNAL_H */

