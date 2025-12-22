
#include <kos.h>

/*
 * KOS_INIT_FLAGS - Configure KOS initialization
 *
 * INIT_DEFAULT includes:
 *   - IRQ handling
 *   - Timer setup
 *   - Thread scheduler
 *
 * We explicitly don't use INIT_ROMDISK since Go programs typically
 * load assets differently.
 */
KOS_INIT_FLAGS(INIT_DEFAULT);

/*
 * Main thread stack size override.
 *
 * The default KOS main thread stack is 32KB which is insufficient for:
 *   - Deep call chains (GC scan -> object scan -> conservative range)
 *   - printf formatting with large buffers
 *   - Benchmark test harnesses with many local variables
 *
 * 128KB is needed for deep benchmark call chains.
 * The writebarrier and cache_patterns benchmarks have nested loops
 * with local arrays that exceed 64KB when combined with Go runtime
 * overhead.
 *
 * Note: This setting is picked up by KOS at boot time.
 * Can be overridden with -DKOS_MAIN_STACK_SIZE=N at compile time.
 */
#ifndef KOS_MAIN_STACK_SIZE
#define KOS_MAIN_STACK_SIZE (128 * 1024)
#endif

/*
 * Exported symbol for KOS to read at initialization.
 * This is the documented way to override main thread stack size.
 */
__attribute__((section(".data"), used))
uint32_t _main_thread_stack_size = KOS_MAIN_STACK_SIZE;


