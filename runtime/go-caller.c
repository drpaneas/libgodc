/* libgodc/runtime/go-caller.c - caller info for stack traces */

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uintptr_t pc;
    const char *file;
    int32_t line;
    const char *function;
} CallerInfo;

uintptr_t runtime_getcallerpc(void)
{
    return (uintptr_t)__builtin_return_address(0);
}

uintptr_t runtime_getcallersp(void)
{
    return (uintptr_t)__builtin_frame_address(0);
}

bool runtime_callers(int32_t skip, uintptr_t *pcbuf, int32_t n)
{
    (void)skip;
    (void)pcbuf;
    (void)n;
    return false;
}

const char *runtime_funcname_go(uintptr_t pc)
{
    (void)pc;
    return "unknown";
}

bool runtime_funcfileline(uintptr_t pc, const char **file, int32_t *line)
{
    (void)pc;
    *file = "unknown";
    *line = 0;
    return false;
}
