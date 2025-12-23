/* libgodc/runtime/go-construct-map.c - map literal construction */

#include "runtime.h"
#include "type_descriptors.h"
#include "map_dreamcast.h"
#include <string.h>

void *__go_construct_map(const struct __go_map_type *type,
                         uintptr_t count,
                         uintptr_t entry_size,
                         uintptr_t val_offset,
                         const void *entries)
{
    MapType *mt = (MapType *)type;
    uintptr_t val_size = MAPTYPE_ELEMSIZE(mt);

    GoMap *h = runtime_makemap(mt, (intptr_t)count, NULL);
    if (!h)
        return NULL;

    const uint8_t *entry_ptr = (const uint8_t *)entries;
    for (uintptr_t i = 0; i < count; i++)
    {
        const uint8_t *key = entry_ptr;
        const uint8_t *val = entry_ptr + val_offset;

        void *val_slot = runtime_mapassign(mt, h, (void *)key);
        if (val_slot)
            memcpy(val_slot, val, val_size);

        entry_ptr += entry_size;
    }

    return h;
}
