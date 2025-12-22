#ifndef PANIC_DREAMCAST_H
#define PANIC_DREAMCAST_H

/*
 * Panic/recover for Dreamcast.
 *
 * Uses setjmp/longjmp instead of C++ unwinder (saves ~50KB).
 * Complex panic/recover chains need runtime_checkpoint().
 */

#include <stdbool.h>
#include <stdint.h>
#include <setjmp.h>
#include "godc_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
struct __go_type_descriptor;

// Eface - Empty interface (interface{})
// Only define if not already defined (e.g., by runtime.h)
#ifndef GODC_EFACE_DEFINED
#define GODC_EFACE_DEFINED
typedef struct {
    struct __go_type_descriptor *type;
    void *data;
} Eface;
#endif

/* MAX_DEFER_DEPTH and MAX_RECURSIVE_PANICS defined in godc_config.h */

/* Data Structures */

/**
 * Defer record - represents a deferred function call
 *
 * Note: This is the internal defer record used by our implementation.
 * gccgo may use a different format (GccgoDefer) which we translate.
 */
typedef struct _PanicDefer {
    struct _PanicDefer *link;   // Next defer in chain (LIFO order)
    uintptr_t pfn;              // Function pointer to call
    void *arg;                  // Argument/closure pointer
    void *frame;                // Frame pointer of registering function
    bool heap;                  // true if heap-allocated
} PanicDefer;

/**
 * Panic record - represents an active panic
 */
typedef struct _PanicRecord {
    struct _PanicRecord *link;  // Previous panic (for nested panics)
    struct __go_type_descriptor *arg_type;  // Panic value type
    void *arg_data;             // Panic value data
    bool recovered;             // Has recover() been called?
    bool aborted;               // Is this a runtime abort?
    bool goexit;                // Is this runtime.Goexit?
} PanicRecord;

/**
 * Checkpoint record - recovery point for setjmp/longjmp
 */
typedef struct _Checkpoint {
    struct _Checkpoint *link;   // Previous checkpoint in chain
    jmp_buf env;                // setjmp buffer for recovery
    void *frame;                // Frame pointer when checkpoint created
} Checkpoint;

/* Public API - Called by gccgo-generated code */
// NOTE: gccgo uses runtime_deferproc_gccgo (symbol: _runtime.deferproc)
// and runtime_deferreturn_gccgo (symbol: _runtime.deferreturn) which are
// declared in the gccgo Defer Support section below.

/**
 * Initiate a panic.
 * Runs deferred functions and either recovers or aborts.
 *
 * @param eface_ptr  Pointer to panic value as empty interface
 */
/**
 * gopanic implementation - takes Eface BY VALUE (not pointer!)
 * On SH-4: r4 = type descriptor, r5 = data pointer
 * This function CAN return if a defer calls recover() successfully.
 */
void runtime_gopanic_impl(struct __go_type_descriptor *type, void *data);

/**
 * gccgo entry point for panic() - Eface passed by value
 */
void runtime_gopanic(struct __go_type_descriptor *type, void *data);

/**
 * Attempt to recover from panic.
 * Only succeeds when called directly from a deferred function during panic.
 *
 * @return Panic value if recovering, nil interface otherwise
 */
Eface runtime_gorecover_impl(void);

/**
 * gccgo entry point for recover() - returns Eface by value
 */
Eface runtime_gorecover(void);

/**
 * Panic with a string message.
 * Convenience wrapper around runtime_gopanic.
 *
 * WARNING: This function CAN RETURN if the panic is recovered by a
 * deferred function calling recover(). Callers MUST handle the return:
 *
 *   runtime_panicstring("something bad happened");
 *   return NULL;  // REQUIRED - reached if panic was recovered
 *
 * If you want a fatal error that NEVER returns, use runtime_throw() instead.
 */
void runtime_panicstring(const char *s);

/**
 * Fatal runtime error - cannot be recovered.
 * Use this for unrecoverable errors like corrupt runtime state.
 * NEVER returns - calls abort() after printing the error.
 */
void runtime_throw(const char *s) __attribute__((noreturn));

/**
 * Check if recovery is possible.
 * Returns true if called from a deferred function during an active panic.
 *
 * @param frame_addr  Frame address where recover() is being called
 */
bool runtime_canrecover(uintptr_t frame_addr);

/* Internal Functions */

/**
 * Initialize panic subsystem.
 * Call this during runtime startup.
 */
void panic_init(void);

/**
 * Create a recovery point for panic/recover.
 *
 * Usage:
 *     if (runtime_checkpoint() != 0) {
 *         // Recovered from panic - handle error
 *         return;
 *     }
 *     // ... setup defer with recover() ...
 *     // ... code that might panic ...
 *
 * @return 0 on initial call, non-zero when recovered via longjmp
 *
 * NOTE: This function uses setjmp and can return twice. The returns_twice
 * attribute tells the compiler to be conservative with register allocation.
 */
int runtime_checkpoint(void) __attribute__((returns_twice));

/**
 * Remove checkpoint if it belongs to caller's frame.
 * Call this when the function returns normally (without panicking).
 */
void runtime_uncheckpoint(void);

/* gccgo Defer Support - struct layout for gccgo 15.1.0 */

// Forward declaration
struct _PanicRecord;

/**
 * gccgo defer structure (matches libgo/go/runtime/runtime2.go _defer)
 * 
 * Fields must match gccgo's expectations exactly.
 * Verified against GCC 15.1.0 gofrontend source.
 */
typedef struct _GccgoDefer {
    struct _GccgoDefer *link;       // 0: Next entry in defer stack
    bool *frame;                    // 4: Pointer to caller's frame bool
    struct _PanicRecord *panicStack; // 8: Panic stack when deferred
    struct _PanicRecord *_panic;    // 12: Panic that caused defer to run
    uintptr_t pfn;                  // 16: Function pointer to call
    void *arg;                      // 20: Argument to pass to function
    uintptr_t retaddr;              // 24: Return address for recover matching
    bool makefunccanrecover;        // 28: MakeFunc recover permission
    bool heap;                      // 29: Whether heap allocated
    // Padding to 32 bytes total
    uint8_t _pad[2];                // 30-31: padding
} GccgoDefer;

// Verify GccgoDefer layout matches gccgo's _defer struct
_Static_assert(sizeof(GccgoDefer) == 32,
               "GccgoDefer must be 32 bytes to match gccgo _defer");
_Static_assert(offsetof(GccgoDefer, link) == 0,
               "GccgoDefer.link must be at offset 0");
_Static_assert(offsetof(GccgoDefer, frame) == 4,
               "GccgoDefer.frame must be at offset 4");
_Static_assert(offsetof(GccgoDefer, pfn) == 16,
               "GccgoDefer.pfn must be at offset 16");
_Static_assert(offsetof(GccgoDefer, arg) == 20,
               "GccgoDefer.arg must be at offset 20");
_Static_assert(offsetof(GccgoDefer, retaddr) == 24,
               "GccgoDefer.retaddr must be at offset 24");
_Static_assert(offsetof(GccgoDefer, makefunccanrecover) == 28,
               "GccgoDefer.makefunccanrecover must be at offset 28");
_Static_assert(offsetof(GccgoDefer, heap) == 29,
               "GccgoDefer.heap must be at offset 29");

/**
 * Register a stack-based defer (4 arguments from gccgo).
 * 
 * @param d      Stack-allocated defer record
 * @param frame  Pointer to caller's frame bool variable
 * @param pfn    Function pointer to call
 * @param arg    Argument to pass to function
 */
void runtime_deferprocStack(GccgoDefer *d, bool *frame, uintptr_t pfn, void *arg);

/**
 * Register a heap-allocated defer (3 arguments from gccgo).
 *
 * @param frame  Pointer to caller's frame bool variable
 * @param pfn    Function pointer to call
 * @param arg    Argument to pass to function
 */
void runtime_deferproc_gccgo(bool *frame, uintptr_t pfn, void *arg);

/**
 * Execute defers for the current frame (1 argument from gccgo).
 *
 * @param frame  Pointer to caller's frame bool variable
 */
void runtime_deferreturn_gccgo(bool *frame);

/**
 * Check and execute defers for a specific frame.
 *
 * @param frame  Pointer to caller's frame bool variable.
 *               If NULL, executes all pending defers (legacy behavior).
 */
void runtime_checkdefer(bool *frame);

/**
 * Set return address for deferred call recovery.
 */
bool runtime_setdeferretaddr(void *retaddr);

#ifdef __cplusplus
}
#endif

#endif // PANIC_DREAMCAST_H

