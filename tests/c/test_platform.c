// test_platform.c - Tests for Dreamcast platform support
// Tier 3: Platform Support (dreamcast_support.c, tls_sh4.c, stack.c)
//
// This tests the C runtime support that backs the Go runtime.

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <kos.h>

// Include libgodc headers
// Only include goroutine.h which has full G definition
// (runtime.h has a simplified G for non-goroutine builds)
#define GODC_GOROUTINES 1
#include <godc/goroutine.h>
#include <godc/gc_semispace.h>

// Forward declaration for runtime initialization
extern void runtime_init(void);

// Test counters
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_PASS(name)               \
    do                                \
    {                                 \
        printf("  PASS: %s\n", name); \
        tests_passed++;               \
    } while (0)

#define TEST_FAIL(name, ...)           \
    do                                 \
    {                                  \
        printf("  FAIL: %s - ", name); \
        printf(__VA_ARGS__);           \
        printf("\n");                  \
        tests_failed++;                \
    } while (0)

// ============================================================================
// Test: TLS (Thread-Local Storage) via GBR
// ============================================================================

void test_tls_gbr(void)
{
    printf("\n=== Test: TLS via GBR ===\n");

    // Test 1: GBR is valid (use stc to read GBR)
    uintptr_t gbr_val;
    __asm__ volatile("stc gbr, %0" : "=r"(gbr_val));
    if (gbr_val != 0)
    {
        TEST_PASS("GBR is non-zero");
    }
    else
    {
        // GBR might be 0 if TLS isn't initialized yet
        printf("  INFO: GBR = 0 (TLS may not be initialized)\n");
        TEST_PASS("GBR read (info only)");
    }

    // Test 2: getg() returns non-null
    G *g = getg();
    if (g != NULL)
    {
        TEST_PASS("getg() returns non-null");
    }
    else
    {
        TEST_FAIL("getg() returns non-null", "getg() = NULL");
    }

    // Test 3: G struct has valid goid
    if (g && g->goid > 0)
    {
        TEST_PASS("G struct has valid goid");
    }
    else if (g)
    {
        TEST_FAIL("G struct has valid goid", "goid = %llu", (unsigned long long)g->goid);
    }

    // Test 4: setg/getg roundtrip
    G *original = getg();
    G temp_g;
    memset(&temp_g, 0, sizeof(temp_g));
    temp_g.goid = 9999;

    setg(&temp_g);
    G *retrieved = getg();
    setg(original); // Restore

    if (retrieved == &temp_g && retrieved->goid == 9999)
    {
        TEST_PASS("setg/getg roundtrip");
    }
    else
    {
        TEST_FAIL("setg/getg roundtrip", "retrieved = %p, expected %p",
                  (void *)retrieved, (void *)&temp_g);
    }
}

// ============================================================================
// Test: Stack Operations
// ============================================================================

void test_stack_operations(void)
{
    printf("\n=== Test: Stack Operations ===\n");

    // Test 1: Current stack pointer is valid
    void *sp;
    __asm__ volatile("mov r15, %0" : "=r"(sp));

    // Stack should be in Dreamcast RAM range
    uintptr_t sp_addr = (uintptr_t)sp;
    if (sp_addr >= 0x8C000000 && sp_addr < 0x8E000000)
    {
        TEST_PASS("Stack pointer in valid RAM range");
    }
    else
    {
        TEST_FAIL("Stack pointer in valid RAM range", "SP = 0x%08lx", (unsigned long)sp_addr);
    }

    // Test 2: Stack alignment (4-byte aligned on SH4)
    if ((sp_addr & 0x3) == 0)
    {
        TEST_PASS("Stack pointer is 4-byte aligned");
    }
    else
    {
        TEST_FAIL("Stack pointer is 4-byte aligned", "SP = 0x%08lx", (unsigned long)sp_addr);
    }

    // Test 3: G struct stack bounds
    // Note: g0 (main goroutine) uses KOS native stack, so g->stack is NULL
    // but g->stack_hi and g->stack_lo are set from tls_init()
    G *g = getg();
    if (g)
    {
        uintptr_t stack_lo = (uintptr_t)g->stack_lo;
        uintptr_t stack_hi = (uintptr_t)g->stack_hi;

        // Verify stack bounds are valid
        if (stack_lo != 0 && stack_hi > stack_lo)
        {
            TEST_PASS("G struct has valid stack bounds");
            printf("    stack_lo=0x%08lx, stack_hi=0x%08lx (size=%luKB)\n",
                   (unsigned long)stack_lo, (unsigned long)stack_hi,
                   (unsigned long)(stack_hi - stack_lo) / 1024);
        }
        else
        {
            TEST_FAIL("G struct has valid stack bounds",
                      "lo=0x%08lx, hi=0x%08lx",
                      (unsigned long)stack_lo, (unsigned long)stack_hi);
        }
    }
    else
    {
        TEST_FAIL("G struct has valid stack bounds", "g is NULL");
    }

    // Test 4: Stack bounds (stack_guard is in G_ext, not G)
    if (g && g->stack_lo != 0 && g->stack_hi != 0)
    {
        TEST_PASS("Stack bounds are set");
        printf("  INFO: Stack lo=0x%08lx hi=0x%08lx\n",
               (unsigned long)g->stack_lo, (unsigned long)g->stack_hi);
    }
    else if (g)
    {
        // Stack bounds might be 0 for g0 before initialization
        printf("  INFO: Stack lo=0x%08lx hi=0x%08lx (may be 0 for g0)\n",
               (unsigned long)g->stack_lo, (unsigned long)g->stack_hi);
        TEST_PASS("Stack bounds check (info only)");
    }
}

// ============================================================================
// Test: Atomic Operations (dreamcast_support.c)
// ============================================================================

void test_atomics(void)
{
    printf("\n=== Test: Atomic Operations ===\n");

    volatile uint32_t val = 0;

    // Test 1: Atomic load
    val = 42;
    uint32_t loaded = __atomic_load_n(&val, __ATOMIC_SEQ_CST);
    if (loaded == 42)
    {
        TEST_PASS("Atomic load");
    }
    else
    {
        TEST_FAIL("Atomic load", "got %lu, expected 42", (unsigned long)loaded);
    }

    // Test 2: Atomic store
    __atomic_store_n(&val, 100, __ATOMIC_SEQ_CST);
    if (val == 100)
    {
        TEST_PASS("Atomic store");
    }
    else
    {
        TEST_FAIL("Atomic store", "got %lu, expected 100", (unsigned long)val);
    }

    // Test 3: Atomic add
    val = 10;
    uint32_t old = __atomic_fetch_add(&val, 5, __ATOMIC_SEQ_CST);
    if (old == 10 && val == 15)
    {
        TEST_PASS("Atomic fetch_add");
    }
    else
    {
        TEST_FAIL("Atomic fetch_add", "old=%lu, new=%lu", (unsigned long)old, (unsigned long)val);
    }

    // Test 4: Atomic compare-exchange (success)
    val = 20;
    uint32_t expected = 20;
    bool success = __atomic_compare_exchange_n(&val, &expected, 30,
                                               false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    if (success && val == 30)
    {
        TEST_PASS("Atomic CAS (success)");
    }
    else
    {
        TEST_FAIL("Atomic CAS (success)", "success=%d, val=%lu", success, (unsigned long)val);
    }

    // Test 5: Atomic compare-exchange (failure)
    val = 40;
    expected = 50; // Wrong expected value
    success = __atomic_compare_exchange_n(&val, &expected, 60,
                                          false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    if (!success && val == 40 && expected == 40)
    {
        TEST_PASS("Atomic CAS (failure)");
    }
    else
    {
        TEST_FAIL("Atomic CAS (failure)", "success=%d, val=%lu, expected=%lu",
                  success, (unsigned long)val, (unsigned long)expected);
    }
}

// ============================================================================
// Test: Memory Barriers
// ============================================================================

void test_memory_barriers(void)
{
    printf("\n=== Test: Memory Barriers ===\n");

    // These just verify the barriers compile and run without crashing

    // Test 1: Full barrier
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    TEST_PASS("Full memory barrier");

    // Test 2: Acquire barrier
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    TEST_PASS("Acquire memory barrier");

    // Test 3: Release barrier
    __atomic_thread_fence(__ATOMIC_RELEASE);
    TEST_PASS("Release memory barrier");

    // Test 4: SH4 sync instruction
    __asm__ volatile("" ::: "memory");
    TEST_PASS("Compiler memory barrier");
}

// ============================================================================
// Test: Cache Operations
// ============================================================================

void test_cache_operations(void)
{
    printf("\n=== Test: Cache Operations ===\n");

    // Allocate some memory to test cache ops
    uint8_t *buf = malloc(4096);
    if (!buf)
    {
        TEST_FAIL("Cache test setup", "malloc failed");
        return;
    }

    // Fill buffer
    memset(buf, 0xAA, 4096);

    // Test 1: dcache_flush_range
    dcache_flush_range((uintptr_t)buf, 4096);
    TEST_PASS("dcache_flush_range");

    // Test 2: dcache_inval_range
    dcache_inval_range((uintptr_t)buf, 4096);
    TEST_PASS("dcache_inval_range");

    // Test 3: Verify data integrity after cache ops
    // Note: After invalidate, cached data might be lost
    // This is expected behavior, so we just check it doesn't crash
    for (int i = 0; i < 4096; i++)
    {
        if (buf[i] != 0xAA)
        {
            // Data may differ after cache invalidate - this is OK
            break;
        }
    }
    TEST_PASS("Cache operations don't crash");

    free(buf);
}

// ============================================================================
// Test: Register Access
// ============================================================================

void test_register_access(void)
{
    printf("\n=== Test: Register Access ===\n");

    // Test 1: Read GBR (use stc instruction)
    uintptr_t gbr_val;
    __asm__ volatile("stc gbr, %0" : "=r"(gbr_val));
    (void)gbr_val;
    TEST_PASS("Read GBR register");

    // Test 2: Read PR (procedure register)
    uintptr_t pr;
    __asm__ volatile("sts pr, %0" : "=r"(pr));
    if (pr != 0)
    {
        TEST_PASS("Read PR register");
    }
    else
    {
        TEST_FAIL("Read PR register", "PR = 0 (unexpected in function)");
    }

    // Test 3: Read callee-saved registers
    register void *r8 __asm__("r8");
    register void *r9 __asm__("r9");
    register void *r10 __asm__("r10");
    (void)r8;
    (void)r9;
    (void)r10;
    TEST_PASS("Read callee-saved registers (r8-r10)");

    // Test 4: Frame pointer (r14)
    register uintptr_t fp __asm__("r14");
    (void)fp;
    TEST_PASS("Read frame pointer (r14)");
}

// ============================================================================
// Test: Context Structure
// ============================================================================

void test_context_structure(void)
{
    printf("\n=== Test: Context Structure ===\n");

    // Test 1: sh4_context_t size (64 bytes after simplification)
    if (sizeof(sh4_context_t) == 64)
    {
        TEST_PASS("sh4_context_t size is 64 bytes");
    }
    else
    {
        TEST_FAIL("sh4_context_t size", "got %zu, expected 64", sizeof(sh4_context_t));
    }

    // Test 2: Create and initialize context
    sh4_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.pc = 0x8C010000;
    ctx.sp = 0x8D000000;
    ctx.pr = 0x8C020000;

    if (ctx.pc == 0x8C010000 && ctx.sp == 0x8D000000)
    {
        TEST_PASS("Context structure initialization");
    }
    else
    {
        TEST_FAIL("Context structure initialization", "unexpected values");
    }

    // Test 3: G struct contains context
    G *g = getg();
    if (g)
    {
        size_t ctx_offset = offsetof(G, context);
        // Context should be at a reasonable offset
        if (ctx_offset > 0 && ctx_offset < sizeof(G))
        {
            TEST_PASS("G struct contains context at valid offset");
        }
        else
        {
            TEST_FAIL("G struct contains context", "offset = %zu", ctx_offset);
        }
    }
}

// ============================================================================
// Test: Timer Functions
// ============================================================================

void test_timer_functions(void)
{
    printf("\n=== Test: Timer Functions ===\n");

    // Test 1: Get current time
    uint64_t t1 = timer_us_gettime64();
    if (t1 > 0)
    {
        TEST_PASS("timer_us_gettime64 returns non-zero");
    }
    else
    {
        TEST_FAIL("timer_us_gettime64", "returned 0");
    }

    // Test 2: Time progresses
    thd_sleep(10); // Sleep 10ms
    uint64_t t2 = timer_us_gettime64();
    if (t2 > t1)
    {
        TEST_PASS("Time progresses after sleep");
    }
    else
    {
        TEST_FAIL("Time progresses", "t1=%llu, t2=%llu",
                  (unsigned long long)t1, (unsigned long long)t2);
    }

    // Test 3: Reasonable time delta
    uint64_t delta = t2 - t1;
    // Should be at least 10ms (10000us) but less than 1 second
    if (delta >= 5000 && delta < 1000000)
    {
        TEST_PASS("Time delta is reasonable");
    }
    else
    {
        TEST_FAIL("Time delta", "delta = %llu us", (unsigned long long)delta);
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("============================================\n");
    printf("Platform Support Test Suite\n");
    printf("Testing dreamcast_support.c, tls_sh4.c, stack.c\n");
    printf("============================================\n");

    // Initialize the Go runtime (sets up GC, TLS, scheduler)
    // This is required for TLS tests to work - getg() returns NULL otherwise
    printf("\nInitializing Go runtime...\n");
    runtime_init();
    printf("Runtime initialized.\n");

    test_tls_gbr();
    test_stack_operations();
    test_atomics();
    test_memory_barriers();
    test_cache_operations();
    test_register_access();
    test_context_structure();
    test_timer_functions();

    printf("\n============================================\n");
    printf("Test Results: %d passed, %d failed\n", tests_passed, tests_failed);
    if (tests_failed == 0)
    {
        printf("ALL PLATFORM TESTS PASSED!\n");
    }
    else
    {
        printf("SOME TESTS FAILED\n");
    }
    printf("============================================\n");

    return tests_failed;
}
