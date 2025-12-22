#include "type_descriptors.h"
#include "gc_semispace.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Global storage for registered type descriptors
static struct __go_type_descriptor **registered_types = NULL;
static size_t registered_types_count = 0;
static size_t registered_types_capacity = 0;

// Type descriptor list passed by compiler
struct type_descriptor_list {
    struct type_descriptor_list *next;
    struct __go_type_descriptor **types;
    int count;
};

static struct type_descriptor_list *type_lists = NULL;

// Called by gccgo during startup to register all type descriptors
void _runtime_registerTypeDescriptors(int n, void *p) {
    
    // Cast to array of type descriptor pointers
    struct __go_type_descriptor **descriptors = (struct __go_type_descriptor **)p;
    
    // Ensure we have capacity
    if (registered_types_count + n > registered_types_capacity) {
        size_t new_capacity = (registered_types_capacity + n) * 2;
        if (new_capacity < 64) new_capacity = 64;
        
        struct __go_type_descriptor **new_array = 
            realloc(registered_types, new_capacity * sizeof(struct __go_type_descriptor *));
        if (!new_array) {
            return;
        }
        registered_types = new_array;
        registered_types_capacity = new_capacity;
    }
    
    // Copy type descriptors
    memcpy(&registered_types[registered_types_count], descriptors, 
           n * sizeof(struct __go_type_descriptor *));
    registered_types_count += n;
    
    // Also maintain linked list for compatibility
    struct type_descriptor_list *list = malloc(sizeof(struct type_descriptor_list));
    if (list) {
        list->next = type_lists;
        list->types = descriptors;
        list->count = n;
        type_lists = list;
    }
}

// Find a type descriptor by comparing with a sample
struct __go_type_descriptor *find_type_descriptor(void *sample_type) {
    struct __go_type_descriptor *sample = (struct __go_type_descriptor *)sample_type;
    
    for (size_t i = 0; i < registered_types_count; i++) {
        if (registered_types[i] == sample) {
            return registered_types[i];
        }
        // Could also compare by hash or other fields
        if (registered_types[i]->__hash == sample->__hash &&
            registered_types[i]->__size == sample->__size &&
            registered_types[i]->__code == sample->__code) {
            return registered_types[i];
        }
    }
    return NULL;
}

// Get type descriptor by index (for debugging)
struct __go_type_descriptor *get_type_by_index(size_t index) {
    if (index < registered_types_count) {
        return registered_types[index];
    }
    return NULL;
}

// Get total number of registered types
size_t get_registered_type_count(void) {
    return registered_types_count;
}

/*
 * Check if a type contains pointers (for GC).
 *
 * Internal recursive implementation with depth tracking. We limit recursion
 * to TYPE_RECURSE_MAX_DEPTH to catch:
 *   - Circular type references (should never happen, but compiler bugs exist)
 *   - Pathologically nested types (why would you nest 100 structs deep?)
 *   - Memory corruption causing infinite loops
 *
 * On hitting the limit, we return 1 (conservative: assume it has pointers).
 * This is safe - worst case we scan some non-pointer memory.
 */
static int type_has_pointers_depth(struct __go_type_descriptor *td, int depth)
{
    if (!td)
        return 0;

    if (depth > TYPE_RECURSE_MAX_DEPTH) {
        /*
         * Something is wrong. Return conservative answer and don't crash.
         * The GC might do extra work but won't miss pointers.
         */
        return 1;
    }

    switch (td->__code) {
        /* Scalar types - definitely no pointers */
        case GO_BOOL:
        case GO_INT:
        case GO_INT8:
        case GO_INT16:
        case GO_INT32:
        case GO_INT64:
        case GO_UINT:
        case GO_UINT8:
        case GO_UINT16:
        case GO_UINT32:
        case GO_UINT64:
        case GO_UINTPTR:
        case GO_FLOAT32:
        case GO_FLOAT64:
        case GO_COMPLEX64:
        case GO_COMPLEX128:
            return 0;

        /* Pointer-containing types - definitely have pointers */
        case GO_PTR:
        case GO_UNSAFE_POINTER:
        case GO_SLICE:
        case GO_STRING:
        case GO_MAP:
        case GO_CHAN:
        case GO_FUNC:
        case GO_INTERFACE:
            return 1;

        /* Compound types - recurse with depth tracking */
        case GO_ARRAY: {
            struct __go_array_type *at = (struct __go_array_type *)td;
            return type_has_pointers_depth(at->__element_type, depth + 1);
        }

        case GO_STRUCT: {
            struct __go_struct_type *st = (struct __go_struct_type *)td;
            for (size_t i = 0; i < st->__fields_count; i++) {
                if (type_has_pointers_depth(st->__fields[i].__typ, depth + 1))
                    return 1;
            }
            return 0;
        }
    }

    /* Unknown type code - be conservative */
    return 1;
}

/* Public wrapper - starts recursion at depth 0 */
int type_has_pointers(struct __go_type_descriptor *td)
{
    return type_has_pointers_depth(td, 0);
}

/*
 * Get pointer offsets within a type (for precise GC).
 *
 * Internal recursive implementation. Depth-limited like type_has_pointers.
 * Returns number of pointer offsets found.
 */
static int get_pointer_offsets_depth(struct __go_type_descriptor *td,
                                     uintptr_t *offsets, int max_offsets,
                                     int depth)
{
    if (!td || !offsets || max_offsets <= 0)
        return 0;

    if (depth > TYPE_RECURSE_MAX_DEPTH) {
        /* Depth exceeded - return 0 (no offsets). Caller will fall back. */
        return 0;
    }

    int count = 0;

    switch (td->__code) {
        case GO_PTR:
        case GO_UNSAFE_POINTER:
        case GO_CHAN:
        case GO_FUNC:
        case GO_MAP:
            /* Single pointer at offset 0 */
            if (count < max_offsets)
                offsets[count++] = 0;
            break;

        case GO_SLICE:
        case GO_STRING:
            /* Data pointer at offset 0 */
            if (count < max_offsets)
                offsets[count++] = 0;
            break;

        case GO_INTERFACE:
            /* Two pointers: type (0) and data (sizeof(void*)) */
            if (count < max_offsets)
                offsets[count++] = 0;
            if (count < max_offsets)
                offsets[count++] = sizeof(void *);
            break;

        case GO_STRUCT: {
            struct __go_struct_type *st = (struct __go_struct_type *)td;
            for (size_t i = 0; i < st->__fields_count && count < max_offsets; i++) {
                struct __go_struct_field *field = &st->__fields[i];

                uintptr_t field_offsets[16];
                int field_count = get_pointer_offsets_depth(
                    field->__typ, field_offsets, 16, depth + 1);

                for (int j = 0; j < field_count && count < max_offsets; j++)
                    offsets[count++] = field->__offset + field_offsets[j];
            }
            break;
        }

        case GO_ARRAY: {
            struct __go_array_type *at = (struct __go_array_type *)td;
            uintptr_t elem_size = at->__element_type->__size;

            uintptr_t elem_offsets[16];
            int elem_count = get_pointer_offsets_depth(
                at->__element_type, elem_offsets, 16, depth + 1);

            for (size_t i = 0; i < at->__len && count < max_offsets; i++) {
                for (int j = 0; j < elem_count && count < max_offsets; j++)
                    offsets[count++] = i * elem_size + elem_offsets[j];
            }
            break;
        }
    }

    return count;
}

/* Public wrapper - starts recursion at depth 0 */
int get_pointer_offsets(struct __go_type_descriptor *td,
                        uintptr_t *offsets, int max_offsets)
{
    return get_pointer_offsets_depth(td, offsets, max_offsets, 0);
}

// Use GC bitmap if available (for newer gccgo versions)
int scan_gcdata_bitmap(struct __go_type_descriptor *td, void *obj,
                      void (*mark_ptr)(void *)) {
    if (!td || !td->__gcdata || td->__ptrdata == 0) {
        return 0;
    }
    
    // GC bitmap: 1 bit per pointer-sized word
    const uint8_t *bitmap = td->__gcdata;
    uintptr_t words = td->__ptrdata / sizeof(void*);
    
    for (uintptr_t i = 0; i < words; i++) {
        // Check if this word is a pointer
        if (bitmap[i/8] & (1 << (i%8))) {
            void **ptr_location = (void **)((char *)obj + i * sizeof(void*));
            void *ptr = *ptr_location;
            if (ptr) {
                mark_ptr(ptr);
            }
        }
    }
    
    return 1;  // Successfully used bitmap
}

// unsafe.Pointer type descriptor needed by gccgo
// Equal function for unsafe.Pointer (gccgo 15.1.0 style)
static _Bool unsafe_pointer_equal(void *p, void *q) {
    return *(void **)p == *(void **)q;
}

static const struct __go_string unsafe_pointer_name = {
    (const uint8_t *)"unsafe.Pointer",
    14
};

// The actual type descriptor with proper gccgo symbol name
struct __go_type_descriptor unsafe_Pointer_descriptor __asm__("_unsafe.Pointer..d") = {
    .__size = sizeof(void *),
    .__ptrdata = sizeof(void *),
    .__hash = 0x78501e83,  // Hash for unsafe.Pointer
    .__tflag = 0,
    .__align = sizeof(void *),
    .__field_align = sizeof(void *),
    .__code = GO_UNSAFE_POINTER,
    .__equalfn = unsafe_pointer_equal,
    .__gcdata = NULL,
    .__reflection = &unsafe_pointer_name,
    .__uncommon = NULL,
    .__pointer_to_this = NULL
};

/* String type descriptor (needed by panic) */

// Equal function for string
static _Bool string_equal(void *p, void *q) {
    struct __go_string *s1 = (struct __go_string *)p;
    struct __go_string *s2 = (struct __go_string *)q;
    if (s1->__length != s2->__length) return 0;
    if (s1->__length == 0) return 1;
    return memcmp(s1->__data, s2->__data, (size_t)s1->__length) == 0;
}

static const struct __go_string string_type_name = {
    (const uint8_t *)"string",
    6
};

// String type descriptor with gccgo symbol name
// Used by panic to create string interface values
struct __go_type_descriptor __go_tdn_string = {
    .__size = sizeof(struct __go_string),
    .__ptrdata = sizeof(void *),  // String has one pointer (data)
    .__hash = 0x0f2bf5bb,  // Standard hash for string type
    .__tflag = 0,
    .__align = sizeof(void *),
    .__field_align = sizeof(void *),
    .__code = GO_STRING,
    .__equalfn = string_equal,
    .__gcdata = NULL,
    .__reflection = &string_type_name,
    .__uncommon = NULL,
    .__pointer_to_this = NULL
};
