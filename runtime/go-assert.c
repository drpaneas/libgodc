/* libgodc/runtime/go-assert.c - assertion and runtime errors */

#include <stdio.h>
#include <stdlib.h>
#include <arch/arch.h>

void __go_assert_fail(const char *file, unsigned int line)
{
    fprintf(stderr, "libgodc: assertion failed at %s:%u\n", file, line);
    arch_exit();
}

void __go_runtime_error(int code)
{
    const char *msg;
    switch (code)
    {
    case 0: msg = "division by zero"; break;
    case 1: msg = "integer overflow"; break;
    case 2: msg = "index out of range"; break;
    case 3: msg = "slice bounds out of range"; break;
    case 4: msg = "nil pointer dereference"; break;
    case 5: msg = "memory address not aligned"; break;
    default: msg = "unknown runtime error"; break;
    }
    fprintf(stderr, "libgodc: %s\n", msg);
    arch_exit();
}
