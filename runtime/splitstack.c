#include "goroutine.h"
#include <stdlib.h>
#include <string.h>

/**
 * Allocate a stack and create a context for it.
 * With fixed stacks, we just allocate GOROUTINE_STACK_SIZE.
 */
void *__splitstack_makecontext(size_t stack_size, void *context[10], size_t *size)
{
    (void)stack_size;  // Ignored - we use fixed size
    
    // Allocate fixed-size stack
    void *stack = memalign(32, GOROUTINE_STACK_SIZE);
    if (stack == NULL) {
        if (size) *size = 0;
        if (context) memset(context, 0, 10 * sizeof(void *));
        return NULL;
    }
    
    if (size) *size = GOROUTINE_STACK_SIZE;
    if (context) {
        memset(context, 0, 10 * sizeof(void *));
        context[0] = stack;  // Store base for releasecontext
    }
    
    return stack;
}

/**
 * Free a split-stack context.
 */
void __splitstack_releasecontext(void *context[10])
{
    if (context && context[0]) {
        free(context[0]);
        context[0] = NULL;
    }
}

// Additional stubs that may be required by some gccgo versions:

void *__splitstack_getcontext(void *context[10])
{
    if (context) memset(context, 0, 10 * sizeof(void *));
    return context;
}

void __splitstack_setcontext(void *context[10])
{
    (void)context;
}

void *__splitstack_find(void *segment_arg, void *sp, size_t *len,
                        void **next_segment, void **next_sp, void **initial_sp)
{
    (void)segment_arg; (void)sp;
    if (len) *len = 0;
    if (next_segment) *next_segment = NULL;
    if (next_sp) *next_sp = NULL;
    if (initial_sp) *initial_sp = NULL;
    return NULL;
}

void __splitstack_block_signals(int *new_value, int *old_value)
{
    (void)new_value;
    if (old_value) *old_value = 0;
}

void __splitstack_block_signals_context(void *context[10], int *new_value, int *old_value)
{
    (void)context; (void)new_value;
    if (old_value) *old_value = 0;
}
