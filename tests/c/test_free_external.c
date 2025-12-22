// test_free_external.c - Verify large object allocation and freeing
//
// Large objects (>64KB) bypass the GC and use malloc directly.
// This test verifies we can free them with gc_external_free.

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "gc_semispace.h"
#include "runtime.h"

static int passed = 0;
static int failed = 0;

#define PASS(name) do { printf("PASS: %s\n", name); passed++; } while(0)
#define FAIL(name, msg) do { printf("FAIL: %s - %s\n", name, msg); failed++; } while(0)

// Large object threshold from config
extern gc_heap_t gc_heap;

static void test_large_alloc_bypasses_gc(void)
{
    uint32_t gc_before = gc_heap.total_alloc_count;
    
    // Allocate something larger than 64KB threshold
    void *large = gc_alloc(100 * 1024, NULL);
    
    uint32_t gc_after = gc_heap.total_alloc_count;
    
    // Large allocs should NOT increase gc heap alloc count
    // (they go through gc_external_alloc instead)
    if (gc_after == gc_before && large != NULL)
        PASS("large alloc bypasses GC heap");
    else
        FAIL("large alloc bypasses GC heap", "alloc count changed or NULL");
    
    // Don't leak - free it
    gc_external_free(large);
}

static void test_free_external_works(void)
{
    // Allocate and free a large object
    void *ptr = gc_alloc(128 * 1024, NULL);
    if (!ptr) {
        FAIL("free external works", "allocation failed");
        return;
    }
    
    // Write to it to make sure it's real memory
    memset(ptr, 0xAB, 128 * 1024);
    
    // Free it - this should not crash
    gc_external_free(ptr);
    
    PASS("free external works");
}

static void test_free_null_safe(void)
{
    // Freeing NULL should be safe
    gc_external_free(NULL);
    PASS("free NULL is safe");
}

static void test_multiple_alloc_free(void)
{
    // Allocate and free multiple times
    for (int i = 0; i < 10; i++) {
        void *ptr = gc_alloc(80 * 1024, NULL);
        if (!ptr) {
            FAIL("multiple alloc/free", "allocation failed");
            return;
        }
        memset(ptr, i, 80 * 1024);
        gc_external_free(ptr);
    }
    PASS("multiple alloc/free cycles");
}

static void test_runtime_FreeExternal(void)
{
    // Test the Go-callable version
    extern void runtime_FreeExternal(void *ptr) __asm__("_runtime.FreeExternal");
    
    void *ptr = gc_alloc(100 * 1024, NULL);
    if (!ptr) {
        FAIL("runtime.FreeExternal", "allocation failed");
        return;
    }
    
    memset(ptr, 0xCD, 100 * 1024);
    runtime_FreeExternal(ptr);
    
    PASS("runtime.FreeExternal");
}

int main(void)
{
    printf("test_free_external\n\n");
    
    runtime_init();
    
    test_large_alloc_bypasses_gc();
    test_free_external_works();
    test_free_null_safe();
    test_multiple_alloc_free();
    test_runtime_FreeExternal();
    
    printf("\nresult: %d passed, %d failed\n", passed, failed);
    if (failed == 0)
        printf("ALL FREE EXTERNAL TESTS PASSED!\n");
    return failed ? 1 : 0;
}

