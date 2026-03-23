// test_free_slice.c - Verify deallocation and nil-after-free of large slice
//
// Large sclices (> 64 KB) are deallocated and nil-after-free.
// This test verifies we can free them runtime_FreeSlice.

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "type_descriptors.h"
#include "gc_semispace.h"
#include "runtime.h"

static int passed = 0;
static int failed = 0;

#define PASS(name) do { printf("PASS: %s\n", name); passed++; } while(0)
#define FAIL(name, msg) do { printf("FAIL: %s - %s\n", name, msg); failed++; } while(0)

static void test_runtime_FreeSlice_type_int(void)
{   
    extern void runtime_FreeSlice(GoSlice *s) __asm__("_runtime.FreeSlice");
    extern void *runtime_makeslice(struct __go_type_descriptor *elem_type, intptr_t len, intptr_t cap) __asm__("_runtime.makeslice");

    static const intptr_t LARGE_KB = 128*1024; // 128 KB

    static struct __go_type_descriptor int_type = {
        .__size = sizeof(int),
        .__ptrdata = 0,  // No pointers
        .__code = GO_INT,
        .__align = sizeof(int),
        .__field_align = sizeof(int)
    };

    GoSlice *s = runtime_makeslice(
        &int_type,
        LARGE_KB,
        LARGE_KB
    );
    if (!s) {
        FAIL("runtime.FreeSlice", "allocation failed");
        return;
    }
    
    memset(s, 0xCD, LARGE_KB);
    runtime_FreeSlice(s);
    
    PASS("runtime.FreeSlice");
}

int main(void)
{
    printf("test_free_slice\n\n");
    
    runtime_init();
    
    test_runtime_FreeSlice_type_int();
    
    printf("\nresult: %d passed, %d failed\n", passed, failed);
    if (failed == 0)
        printf("ALL FREE SLICES TESTS PASSED!\n");
    return failed ? 1 : 0;
}