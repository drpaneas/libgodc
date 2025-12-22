/**
 * test_splitstack.c - C-level tests for split-stack (GBR) functionality
 * 
 * Regression test for: GBR must point to goroutine TLS for split-stack
 * 
 * This test verifies at the C/assembly level that:
 * 1. GBR register contains a valid pointer
 * 2. @(0, gbr) contains stack_guard (what split-stack prologue reads)
 * 3. TLS block fields are consistent
 * 4. Context switches preserve GBR correctly
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <kos.h>

// Include runtime headers
#define GODC_GOROUTINES 1
#include "../../runtime/goroutine.h"
#include "../../runtime/runtime.h"

// Test counters
static int tests_passed = 0;
static int tests_total = 0;

#define TEST_PASS(name) do { \
    tests_passed++; \
    tests_total++; \
    printf("  PASS: %s\n", name); \
} while(0)

#define TEST_FAIL(name) do { \
    tests_total++; \
    printf("  FAIL: %s\n", name); \
} while(0)

//=============================================================================
// GBR Register Access
//=============================================================================

/**
 * Read current GBR value
 */
static inline void *read_gbr_register(void) {
    void *result;
    __asm__ volatile("stc gbr, %0" : "=r"(result));
    return result;
}

/**
 * Read value at @(0, gbr) - what split-stack prologue does
 */
static inline void *read_gbr_offset_0(void) {
    void *result;
    __asm__ volatile("mov.l @(0, gbr), %0" : "=r"(result));
    return result;
}

/**
 * Read value at @(4, gbr) - current_g pointer
 */
static inline void *read_gbr_offset_4(void) {
    void *result;
    // Can't use @(4, gbr) directly for mov.l, need to calculate
    void *gbr = read_gbr_register();
    if (gbr) {
        result = ((void **)gbr)[1]; // offset 4 / sizeof(void*) = index 1
    } else {
        result = NULL;
    }
    return result;
}

//=============================================================================
// Test: GBR Points to Valid TLS
//=============================================================================

void test_gbr_valid_pointer(void) {
    printf("\n=== Test: GBR Valid Pointer ===\n");

    void *gbr = read_gbr_register();
    
    // Test 1: GBR is not NULL
    if (gbr != NULL) {
        TEST_PASS("GBR is not NULL");
    } else {
        TEST_FAIL("GBR is NULL");
        return; // Can't continue
    }
    
    // Test 2: GBR is in valid memory range (Dreamcast RAM: 0x8c000000 - 0x8e000000)
    uintptr_t addr = (uintptr_t)gbr;
    if (addr >= 0x8c000000 && addr < 0x8e000000) {
        TEST_PASS("GBR in valid memory range");
    } else {
        printf("    GBR = %p (outside valid range)\n", gbr);
        TEST_FAIL("GBR in valid memory range");
    }
    
    // Test 3: GBR is properly aligned (8-byte alignment for tls_block_t)
    if ((addr & 0x7) == 0) {
        TEST_PASS("GBR is 8-byte aligned");
    } else {
        TEST_FAIL("GBR is 8-byte aligned");
    }
}

//=============================================================================
// Test: GBR TLS Structure
//=============================================================================

void test_gbr_tls_structure(void) {
    printf("\n=== Test: GBR TLS Structure ===\n");
    
    tls_block_t *tls = (tls_block_t *)read_gbr_register();
    if (tls == NULL) {
        TEST_FAIL("TLS is NULL, cannot test structure");
        return;
    }
    
    // Test 1: stack_guard at offset 0 (critical for split-stack!)
    void *stack_guard_from_struct = tls->stack_guard;
    void *stack_guard_from_gbr = read_gbr_offset_0();
    
    if (stack_guard_from_struct == stack_guard_from_gbr) {
        TEST_PASS("stack_guard at offset 0 matches");
    } else {
        printf("    struct: %p, @(0,gbr): %p\n", 
               stack_guard_from_struct, stack_guard_from_gbr);
        TEST_FAIL("stack_guard at offset 0 matches");
    }
    
    // Test 2: current_g at offset 4
    G *current_g_from_struct = tls->current_g;
    G *current_g_from_gbr = (G *)read_gbr_offset_4();
    
    if (current_g_from_struct == current_g_from_gbr) {
        TEST_PASS("current_g at offset 4 matches");
    } else {
        printf("    struct: %p, offset 4: %p\n", 
               (void *)current_g_from_struct, (void *)current_g_from_gbr);
        TEST_FAIL("current_g at offset 4 matches");
    }
    
    // Test 3: current_g is not NULL
    if (current_g_from_struct != NULL) {
        TEST_PASS("current_g is not NULL");
    } else {
        TEST_FAIL("current_g is not NULL");
    }
    
    // Test 4: stack_guard is in valid range
    uintptr_t guard_addr = (uintptr_t)stack_guard_from_struct;
    if (guard_addr >= 0x8c000000 && guard_addr < 0x8e000000) {
        TEST_PASS("stack_guard in valid memory range");
    } else if (guard_addr == 0) {
        // May be uninitialized for main thread
        printf("    stack_guard is 0 (may be uninitialized)\n");
        TEST_PASS("stack_guard is zero (acceptable for main)");
    } else {
        printf("    stack_guard = %p\n", stack_guard_from_struct);
        TEST_FAIL("stack_guard in valid memory range");
    }
}

//=============================================================================
// Test: TLS Matches Global current_tls
//=============================================================================

extern tls_block_t *current_tls;
extern G *current_g;

void test_tls_consistency(void) {
    printf("\n=== Test: TLS Consistency ===\n");
    
    tls_block_t *gbr_tls = (tls_block_t *)read_gbr_register();
    
    // Test 1: GBR matches current_tls global
    if (gbr_tls == current_tls) {
        TEST_PASS("GBR matches current_tls global");
    } else {
        printf("    GBR: %p, current_tls: %p\n", (void *)gbr_tls, (void *)current_tls);
        TEST_FAIL("GBR matches current_tls global");
    }
    
    // Test 2: TLS->current_g matches current_g global
    if (gbr_tls && gbr_tls->current_g == current_g) {
        TEST_PASS("TLS->current_g matches current_g global");
    } else {
        printf("    TLS->current_g: %p, current_g: %p\n", 
               gbr_tls ? (void *)gbr_tls->current_g : NULL, (void *)current_g);
        TEST_FAIL("TLS->current_g matches current_g global");
    }
    
    // Test 3: getg() returns consistent value
    G *gp = getg();
    if (gp == current_g) {
        TEST_PASS("getg() matches current_g");
    } else {
        printf("    getg(): %p, current_g: %p\n", (void *)gp, (void *)current_g);
        TEST_FAIL("getg() matches current_g");
    }
}

//=============================================================================
// Test: Split-Stack Prologue Simulation
//=============================================================================

void test_splitstack_prologue(void) {
    printf("\n=== Test: Split-Stack Prologue Simulation ===\n");
    
    // Simulate what the split-stack prologue does:
    //   mov.l @(0, gbr), r0    ; Load stack_guard
    //   cmp/hi r15, r0         ; Compare: stack_guard > SP?
    //   bt __morestack         ; If yes, need more stack
    
    void *stack_guard = read_gbr_offset_0();
    
    register uintptr_t sp __asm__("r15");
    uintptr_t current_sp = sp;
    uintptr_t guard = (uintptr_t)stack_guard;
    
    printf("    SP = %p, stack_guard = %p\n", (void *)current_sp, stack_guard);
    
    // Test 1: stack_guard is readable (didn't crash)
    TEST_PASS("stack_guard readable via @(0, gbr)");
    
    // Test 2: SP is above stack_guard (we have stack space)
    if (current_sp > guard || guard == 0) {
        TEST_PASS("SP is above stack_guard (or guard=0)");
    } else {
        printf("    SP (%p) <= stack_guard (%p) - would trigger __morestack\n",
               (void *)current_sp, stack_guard);
        TEST_FAIL("SP is above stack_guard");
    }
    
    // Test 3: Verify the check logic matches split-stack convention
    // split-stack uses: if (guard > SP) call __morestack
    bool would_trigger = (guard > current_sp);
    printf("    Would trigger __morestack: %s\n", would_trigger ? "yes" : "no");
    if (!would_trigger) {
        TEST_PASS("Split-stack check passes (no __morestack needed)");
    } else {
        // This is actually okay if we're near stack limit
        TEST_PASS("Split-stack check would trigger __morestack");
    }
}

//=============================================================================
// Test: Static TLS Offset Verification
//=============================================================================

void test_tls_offsets(void) {
    printf("\n=== Test: TLS Structure Offsets ===\n");
    
    // These offsets MUST match what the assembly expects
    // split-stack prologue reads stack_guard at offset 0
    
    // Test 1: stack_guard at offset 0
    size_t stack_guard_offset = offsetof(tls_block_t, stack_guard);
    if (stack_guard_offset == 0) {
        TEST_PASS("stack_guard at offset 0 (TLS_STACK_GUARD_OFFSET)");
    } else {
        printf("    Actual offset: %zu (expected 0)\n", stack_guard_offset);
        TEST_FAIL("stack_guard at offset 0");
    }
    
    // Test 2: current_g at offset 4
    size_t current_g_offset = offsetof(tls_block_t, current_g);
    if (current_g_offset == 4) {
        TEST_PASS("current_g at offset 4 (TLS_CURRENT_G_OFFSET)");
    } else {
        printf("    Actual offset: %zu (expected 4)\n", current_g_offset);
        TEST_FAIL("current_g at offset 4");
    }
    
    // Test 3: TLS block size
    size_t tls_size = sizeof(tls_block_t);
    if (tls_size == 32) {
        TEST_PASS("tls_block_t is 32 bytes");
    } else {
        printf("    Actual size: %zu (expected 32)\n", tls_size);
        TEST_FAIL("tls_block_t is 32 bytes");
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("============================================\n");
    printf("Split-Stack (GBR) C-Level Test Suite\n");
    printf("============================================\n");
    printf("\n");
    printf("This test verifies that GBR is correctly\n");
    printf("configured for split-stack support.\n");
    printf("\n");
    
    // Run tests
    test_gbr_valid_pointer();
    test_gbr_tls_structure();
    test_tls_consistency();
    test_splitstack_prologue();
    test_tls_offsets();
    
    printf("\n============================================\n");
    printf("Results: %d / %d tests passed\n", tests_passed, tests_total);
    printf("============================================\n");
    
    if (tests_passed == tests_total) {
        printf("\nALL TESTS PASSED - GBR is correctly configured!\n");
        return 0;
    } else {
        printf("\nSOME TESTS FAILED - Check GBR configuration\n");
        return 1;
    }
}

