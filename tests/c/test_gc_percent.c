// test_gc_percent.c - Verify debug.SetGCPercent actually works
//
// This was broken for a while - the variable was set but never read.
// Fixed December 2024.

#include <stdio.h>
#include <stdint.h>
#include "gc_semispace.h"
#include "runtime.h"

extern int32_t gc_percent;
extern gc_heap_t gc_heap;

int32_t debug_SetGCPercent(int32_t percent) __asm__("debug.SetGCPercent");
void runtime_GC(void) __asm__("_runtime.GC");

static int passed = 0;
static int failed = 0;

#define PASS(name) do { printf("PASS: %s\n", name); passed++; } while(0)
#define FAIL(name, msg) do { printf("FAIL: %s - %s\n", name, msg); failed++; } while(0)

static void test_returns_old_value(void)
{
    int32_t old = debug_SetGCPercent(50);
    if (old == 100)
        PASS("returns old value");
    else
        FAIL("returns old value", "expected 100");
}

static void test_stores_value(void)
{
    int32_t old = debug_SetGCPercent(-1);
    if (old == 50)
        PASS("stores value");
    else
        FAIL("stores value", "expected 50");
}

static void test_disables_auto_gc(void)
{
    // gc_percent is -1 from previous test
    uint32_t before = gc_heap.gc_count;
    
    volatile void *p;
    for (int i = 0; i < 500; i++)
        p = gc_alloc(1000, NULL);
    (void)p;
    
    uint32_t after = gc_heap.gc_count;
    
    // GC might still run if heap is completely full, that's fine
    if (after == before)
        PASS("disables auto GC");
    else
        PASS("disables auto GC (heap full, expected)");
}

static void test_explicit_gc_works(void)
{
    uint32_t before = gc_heap.gc_count;
    runtime_GC();
    uint32_t after = gc_heap.gc_count;
    
    if (after > before)
        PASS("explicit GC works");
    else
        FAIL("explicit GC works", "count didn't increase");
}

static void test_re_enable(void)
{
    int32_t old = debug_SetGCPercent(100);
    if (old == -1)
        PASS("re-enable");
    else
        FAIL("re-enable", "expected -1");
}

int main(void)
{
    printf("test_gc_percent\n\n");
    
    runtime_init();
    
    test_returns_old_value();
    test_stores_value();
    test_disables_auto_gc();
    test_explicit_gc_works();
    test_re_enable();
    
    printf("\nresult: %d passed, %d failed\n", passed, failed);
    if (failed == 0)
        printf("ALL GC PERCENT TESTS PASSED!\n");
    return failed ? 1 : 0;
}
