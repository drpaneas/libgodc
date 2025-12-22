/* libgodc/runtime/godc_config.h - runtime configuration */
#ifndef GODC_CONFIG_H
#define GODC_CONFIG_H

/* GC heap size (each semispace) */
#ifndef GC_SEMISPACE_SIZE_KB
#define GC_SEMISPACE_SIZE_KB 2048
#endif

/* Large objects bypass GC heap */
#ifndef GC_LARGE_OBJECT_THRESHOLD_KB
#define GC_LARGE_OBJECT_THRESHOLD_KB 64
#endif

/* Goroutine stack size */
#ifndef GOROUTINE_STACK_SIZE
#define GOROUTINE_STACK_SIZE (64 * 1024)
#endif

/* Stack pool */
#ifndef STACK_SIZE_SMALL
#define STACK_SIZE_SMALL (8 * 1024)
#endif
#ifndef STACK_SIZE_MEDIUM
#define STACK_SIZE_MEDIUM (32 * 1024)
#endif
#ifndef STACK_SIZE_LARGE
#define STACK_SIZE_LARGE (64 * 1024)
#endif
#ifndef STACK_POOL_MAX_SEGMENTS
#define STACK_POOL_MAX_SEGMENTS 32
#endif
#ifndef STACK_GUARD_SIZE
#define STACK_GUARD_SIZE 256
#endif

/* Defer/panic limits */
#ifndef MAX_DEFER_DEPTH
#define MAX_DEFER_DEPTH 1000
#endif
#ifndef MAX_RECURSIVE_PANICS
#define MAX_RECURSIVE_PANICS 5
#endif

/* Sudog pool for channels */
#ifndef SUDOG_POOL_MAX
#define SUDOG_POOL_MAX 128
#endif

/* Timers */
#ifndef TIMER_PROCESS_MAX
#define TIMER_PROCESS_MAX 1000
#endif

/* Dead goroutine cleanup */
#ifndef DEAD_G_GRACE_GENERATIONS
#define DEAD_G_GRACE_GENERATIONS 2
#endif
#ifndef MAX_CLEANUP_PER_CALL
#define MAX_CLEANUP_PER_CALL 8
#endif

/* Fatal panic output delay */
#ifndef FATALPANIC_FLUSH_DELAY_MS
#define FATALPANIC_FLUSH_DELAY_MS 50
#endif

/* Max stack scan size for GC */
#ifndef GC_STACK_SCAN_MAX
#define GC_STACK_SCAN_MAX (64 * 1024)
#endif

/* Map configuration */
#ifndef MAP_ZERO_VALUE_MAX_SIZE
#define MAP_ZERO_VALUE_MAX_SIZE 256
#endif
#ifndef MAP_MAX_BUCKET_SHIFT
#define MAP_MAX_BUCKET_SHIFT 15
#endif
#ifndef MAP_EVACUATE_SAFETY_LIMIT
#define MAP_EVACUATE_SAFETY_LIMIT 1000000
#endif

/* Type recursion limit */
#ifndef TYPE_RECURSE_MAX_DEPTH
#define TYPE_RECURSE_MAX_DEPTH 32
#endif

/* Runtime assertions */
extern void runtime_throw(const char *msg) __attribute__((noreturn));

#ifndef GODC_RUNTIME_ASSERT
#define GODC_RUNTIME_ASSERT(cond, msg) \
    do { if (__builtin_expect(!(cond), 0)) runtime_throw(msg); } while (0)
#endif

#endif /* GODC_CONFIG_H */
