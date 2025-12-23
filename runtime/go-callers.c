/* libgodc/runtime/go-callers.c - stack trace stubs */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    uintptr_t pc;
    const char *filename;
    const char *function;
    intptr_t lineno;
} Location;

int32_t runtime_callers(int32_t skip, Location *loc, int32_t max, bool keep_callers)
{
    (void)skip;
    (void)keep_callers;
    if (max > 0 && loc)
    {
        loc[0].pc = 0;
        loc[0].filename = "unknown";
        loc[0].function = "main";
        loc[0].lineno = 0;
        return 1;
    }
    return 0;
}

void runtime_printstack(void)
{
    printf("goroutine:\n  [stack trace not available]\n");
}

void runtime_dopanic(int32_t code)
{
    printf("panic: code %d\n", (int)code);
    runtime_printstack();
    extern void arch_exit(void);
    arch_exit();
}

uint32_t __go_runtime_in_callers = 0;
