#include <dc/video.h>
#include <stdint.h>

uintptr_t __go_vram_s(void)
{
    return (uintptr_t)vram_s;
}
