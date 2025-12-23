#include "goroutine.h"
#include "gc_semispace.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <arch/arch.h>

/* Forward declarations */
extern void registerGCRoots(gc_root_list_t *roots);
/* runtime_panicstring is defined in defer_dreamcast.c */
extern void runtime_panicstring(const char *msg);

/**
 * runtime.throw - Fatal runtime error
 *
 * This is called for unrecoverable errors. Prints the message and halts.
 */
void _runtime_throw_impl(const char *msg) __asm__("_runtime.throw");
void _runtime_throw_impl(const char *msg)
{
    runtime_throw(msg);
    /* runtime_throw is noreturn, but compiler doesn't know that for this wrapper */
    __builtin_unreachable();
}

/**
 * runtime.panicstring - Panic with a string message
 *
 * Unlike throw, panic can potentially be recovered.
 */
void _runtime_panicstring_impl(const char *msg) __asm__("_runtime.panicstring");
void _runtime_panicstring_impl(const char *msg)
{
    runtime_panicstring(msg);
}

/**
 * runtime.registerGCRoots - Register GC root descriptors
 *
 * Called by compiler-generated init code to register global variables
 * that contain pointers which need to be scanned by the GC.
 */
void _runtime_registerGCRoots_impl(gc_root_list_t *roots) __asm__("_runtime.registerGCRoots");
void _runtime_registerGCRoots_impl(gc_root_list_t *roots)
{
    registerGCRoots(roots);
}

/**
 * runtime.memequal0 - Memory equality for zero-size types
 *
 * Zero-size types (like struct{}) are always equal to each other.
 * This is used by the compiler for type descriptors of empty types.
 *
 * Uses _Bool (not bool) to match gccgo's ABI expectations exactly.
 */
_Bool _runtime_memequal0(void *p, void *q) __asm__("_runtime.memequal0");
_Bool _runtime_memequal0(void *p, void *q)
{
    (void)p;
    (void)q;
    return true;  /* Always equal for zero-size types */
}

/**
 * runtime.memequal0..f - Function pointer variant
 *
 * Same as memequal0, but with the ..f suffix for function pointer tables.
 */
_Bool _runtime_memequal0_f(void *p, void *q) __asm__("_runtime.memequal0..f");
_Bool _runtime_memequal0_f(void *p, void *q)
{
    (void)p;
    (void)q;
    return true;  /* Always equal for zero-size types */
}

/**
 * goexit_trampoline - Trampoline for goroutine exit
 *
 * This is set as the return address for new goroutines.
 * When a goroutine's entry function returns via rts, control comes here.
 * We then call the C runtime to clean up and schedule the next goroutine.
 *
 * NOTE: This must be a real function so we can take its address.
 * The implementation just calls the C cleanup function.
 */
void goexit_trampoline(void)
{
    runtime_goexit_internal();
    /* runtime_goexit_internal is noreturn */
    __builtin_unreachable();
}

