#include "runtime.h"

#include <stdint.h>

/* runtime_writeBarrier - Write barrier flag variable
 *
 * gccgo expects this to be a struct/variable that it can read to check
 * if write barriers are enabled. For bare-metal Dreamcast we keep it disabled (0).
 */
struct {
    uint32_t enabled;
    uint32_t pad1, pad2, pad3;
} runtime_writeBarrier __asm__("_runtime.writeBarrier") = {0, 0, 0, 0};

/* runtime_gcWriteBarrier - GC write barrier function
 *
 * This is called when writing a pointer from one object to another.
 * For bare-metal Dreamcast, this is a no-op that just performs the write.
 *
 * gccgo signature: func gcWriteBarrier(dst *uintptr, src uintptr)
 * The src parameter is a uintptr, not a pointer, per gccgo convention.
 */
void runtime_gcWriteBarrier(void *dst, uintptr_t src) __asm__("_runtime.gcWriteBarrier");
void runtime_gcWriteBarrier(void *dst, uintptr_t src)
{
    /* For bare-metal Dreamcast, just perform the write directly */
    if (dst) {
        *(uintptr_t *)dst = src;
    }
}


/* runtime_typedmemmove_writebarrier - Typed memory move with write barrier
 *
 * This is like memmove but also applies write barriers for GC.
 * For now, this is a no-op stub. A full implementation would:
 * 1. Perform typed memory move
 * 2. Apply write barriers for any moved pointers
 */
void runtime_typedmemmove_writebarrier(void *typ, void *dst, void *src)
{
    /* Simplified no-op for bare-metal
     * In practice, the code using this will have its own memory moves
     */
    (void) typ;
    (void) dst;
    (void) src;
}
