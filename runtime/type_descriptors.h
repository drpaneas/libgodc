#ifndef TYPE_DESCRIPTORS_H
#define TYPE_DESCRIPTORS_H

#include <stdint.h>
#include <stddef.h>

// Type kinds matching Go's reflect.Kind values
#define GO_BOOL 1
#define GO_INT 2
#define GO_INT8 3
#define GO_INT16 4
#define GO_INT32 5
#define GO_INT64 6
#define GO_UINT 7
#define GO_UINT8 8
#define GO_UINT16 9
#define GO_UINT32 10
#define GO_UINT64 11
#define GO_UINTPTR 12
#define GO_FLOAT32 13
#define GO_FLOAT64 14
#define GO_COMPLEX64 15
#define GO_COMPLEX128 16
#define GO_ARRAY 17
#define GO_CHAN 18
#define GO_FUNC 19
#define GO_INTERFACE 20
#define GO_MAP 21
#define GO_PTR 22
#define GO_SLICE 23
#define GO_STRING 24
#define GO_STRUCT 25
#define GO_UNSAFE_POINTER 26

// String representation in gccgo
struct __go_string
{
    const uint8_t *__data;
    intptr_t __length;
};

// Method descriptor
// From gccgo runtime/type.go:
//   type method struct {
//       name    *string
//       pkgPath *string
//       mtyp    *_type
//       typ     *_type
//       tfn     unsafe.Pointer
//   }
struct __go_method
{
    const struct __go_string *__name;     // offset 0: POINTER to string
    const struct __go_string *__pkg_path; // offset 4: POINTER to string
    struct __go_type_descriptor *__mtyp;  // offset 8: method type (receiver signature)
    struct __go_type_descriptor *__typ;   // offset 12: function type (without receiver)
    void *__tfn;                          // offset 16: function pointer
}; // Total: 20 bytes on 32-bit

// Uncommon type info for named types and types with methods
// From gccgo runtime/type.go:
//   type uncommontype struct {
//       name    *string
//       pkgPath *string
//       methods []method  // Go slice = {ptr, len, cap}
//   }
struct __go_uncommon_type
{
    const struct __go_string *__name;     // offset 0: POINTER to name string
    const struct __go_string *__pkg_path; // offset 4: POINTER to pkg path string
    // Go slice: {pointer, length, capacity}
    struct __go_method *__methods; // offset 8: pointer to methods array
    intptr_t __methods_count;      // offset 12: length of methods
    intptr_t __methods_cap;        // offset 16: capacity (usually same as count)
}; // Total: 20 bytes on 32-bit

/* Base type descriptor (_type) - 36 bytes on 32-bit SH-4 */
struct __go_type_descriptor
{
    uintptr_t __size;                               // offset 0: size of type in bytes
    uintptr_t __ptrdata;                            // offset 4: size of memory prefix with pointers
    uint32_t __hash;                                // offset 8: hash of type
    uint8_t __tflag;                                // offset 12: extra type information flags
    uint8_t __align;                                // offset 13: alignment of variable
    uint8_t __field_align;                          // offset 14: alignment of struct field
    uint8_t __code;                                 // offset 15: kind (type category)
    void *__equalfn;                                // offset 16: pointer to FuncVal for equality
    const uint8_t *__gcdata;                        // offset 20: GC type data
    const struct __go_string *__reflection;         // offset 24: string form
    const struct __go_uncommon_type *__uncommon;    // offset 28: uncommon type info
    struct __go_type_descriptor *__pointer_to_this; // offset 32: type for *T
}; // Total: 36 bytes

// Verify _type layout at compile time
_Static_assert(sizeof(struct __go_type_descriptor) == 36,
               "_type size mismatch - expected 36 bytes on 32-bit");
_Static_assert(offsetof(struct __go_type_descriptor, __size) == 0,
               "_type.__size offset mismatch");
_Static_assert(offsetof(struct __go_type_descriptor, __ptrdata) == 4,
               "_type.__ptrdata offset mismatch");
_Static_assert(offsetof(struct __go_type_descriptor, __hash) == 8,
               "_type.__hash offset mismatch");
_Static_assert(offsetof(struct __go_type_descriptor, __tflag) == 12,
               "_type.__tflag offset mismatch");
_Static_assert(offsetof(struct __go_type_descriptor, __code) == 15,
               "_type.__code offset mismatch");
_Static_assert(offsetof(struct __go_type_descriptor, __equalfn) == 16,
               "_type.__equalfn offset mismatch");
_Static_assert(offsetof(struct __go_type_descriptor, __gcdata) == 20,
               "_type.__gcdata offset mismatch");
_Static_assert(offsetof(struct __go_type_descriptor, __reflection) == 24,
               "_type.__reflection offset mismatch");
_Static_assert(offsetof(struct __go_type_descriptor, __uncommon) == 28,
               "_type.__uncommon offset mismatch");
_Static_assert(offsetof(struct __go_type_descriptor, __pointer_to_this) == 32,
               "_type.__pointer_to_this offset mismatch");

// Slice of type descriptors
struct __go_open_array
{
    void *__values;
    intptr_t __count;
    intptr_t __capacity;
};

/* Map type descriptor - 60 bytes on 32-bit SH-4 */
struct __go_map_type
{
    struct __go_type_descriptor __common;       // offset 0, 36 bytes (embedded)
    struct __go_type_descriptor *__key_type;    // offset 36, 4 bytes
    struct __go_type_descriptor *__val_type;    // offset 40, 4 bytes (elem)
    struct __go_type_descriptor *__bucket_type; // offset 44, 4 bytes
    void *__hasher;                             // offset 48, 4 bytes (FuncVal*)
    uint8_t __keysize;                          // offset 52, 1 byte
    uint8_t __valuesize;                        // offset 53, 1 byte (NOT elemsize!)
    uint16_t __bucketsize;                      // offset 54, 2 bytes
    uint32_t __flags;                           // offset 56, 4 bytes
}; // Total: 60 bytes

// Verify maptype layout at compile time
_Static_assert(sizeof(struct __go_map_type) == 60,
               "maptype size mismatch - expected 60 bytes on 32-bit");
_Static_assert(offsetof(struct __go_map_type, __common) == 0,
               "maptype.__common offset mismatch");
_Static_assert(offsetof(struct __go_map_type, __key_type) == 36,
               "maptype.__key_type offset mismatch");
_Static_assert(offsetof(struct __go_map_type, __val_type) == 40,
               "maptype.__val_type offset mismatch");
_Static_assert(offsetof(struct __go_map_type, __bucket_type) == 44,
               "maptype.__bucket_type offset mismatch");
_Static_assert(offsetof(struct __go_map_type, __hasher) == 48,
               "maptype.__hasher offset mismatch");
_Static_assert(offsetof(struct __go_map_type, __keysize) == 52,
               "maptype.__keysize offset mismatch");
_Static_assert(offsetof(struct __go_map_type, __valuesize) == 53,
               "maptype.__valuesize offset mismatch");
_Static_assert(offsetof(struct __go_map_type, __bucketsize) == 54,
               "maptype.__bucketsize offset mismatch");
_Static_assert(offsetof(struct __go_map_type, __flags) == 56,
               "maptype.__flags offset mismatch");

/* Other type descriptors */

// Array type descriptor
struct __go_array_type
{
    struct __go_type_descriptor __common;
    struct __go_type_descriptor *__element_type;
    struct __go_type_descriptor *__slice_type;
    uintptr_t __len;
};

// Slice type descriptor
struct __go_slice_type
{
    struct __go_type_descriptor __common;
    struct __go_type_descriptor *__element_type;
};

// Channel type descriptor
struct __go_chan_type
{
    struct __go_type_descriptor __common;        // offset 0, 36 bytes
    struct __go_type_descriptor *__element_type; // offset 36, 4 bytes
    uintptr_t __dir;                             // offset 40, 4 bytes (send/recv direction)
};

// Pointer type descriptor
struct __go_ptr_type
{
    struct __go_type_descriptor __common;
    struct __go_type_descriptor *__element_type;
};

// Struct field descriptor
struct __go_struct_field
{
    struct __go_string __name;     // embedded string
    struct __go_string __pkg_path; // embedded string
    struct __go_type_descriptor *__typ;
    struct __go_string __tag; // embedded string
    uintptr_t __offset;
};

// Struct type descriptor
struct __go_struct_type
{
    struct __go_type_descriptor __common;
    struct __go_struct_field *__fields; // pointer FIRST (like interface_type)
    uintptr_t __fields_count;           // count SECOND
};

// Interface method descriptor
// From gccgo runtime/type.go:
//   type imethod struct {
//       name    *string
//       pkgPath *string
//       typ     *_type
//   }
struct __go_interface_method
{
    const struct __go_string *__name;     // offset 0: POINTER to name string
    const struct __go_string *__pkg_path; // offset 4: POINTER to pkg path string
    struct __go_type_descriptor *__typ;   // offset 8: method type
}; // Total: 12 bytes on 32-bit

// Interface type descriptor
// From gccgo runtime/type.go:
//   type interfacetype struct {
//       typ     _type
//       methods []imethod  // Go slice = {ptr, len, cap}
//   }
struct __go_interface_type
{
    struct __go_type_descriptor __common; // offset 0: embedded _type (36 bytes)
    // Go slice: {pointer, length, capacity}
    struct __go_interface_method *__methods; // offset 36: pointer to methods array
    intptr_t __methods_count;                // offset 40: length of methods
    intptr_t __methods_cap;                  // offset 44: capacity
}; // Total: 48 bytes on 32-bit

// Channel type descriptor
struct __go_channel_type
{
    struct __go_type_descriptor __common;
    struct __go_type_descriptor *__element_type;
    uintptr_t __dir;
};

// Function type descriptor
struct __go_func_type
{
    struct __go_type_descriptor __common;
    uint8_t __dotdotdot;
    // Padding to align pointers
    uint8_t __pad[3];
    struct
    {
        struct __go_type_descriptor **__values;
        intptr_t __count;
        intptr_t __capacity;
    } __in; // slice of input types
    struct
    {
        struct __go_type_descriptor **__values;
        intptr_t __count;
        intptr_t __capacity;
    } __out; // slice of output types
};

/**
 * DEFINE_GO_TYPE_DESC - Define a static type descriptor for a C struct.
 *
 * This macro reduces boilerplate when defining type descriptors for runtime
 * structures that need GC tracing. It handles the common case where:
 * - __hash, __tflag are 0
 * - __equalfn, __reflection, __uncommon, __pointer_to_this are NULL
 *
 * @param name       Name for the type descriptor variable
 * @param type_struct The C struct type (e.g., hchan, sudog)
 * @param kind       GO_* constant for the type kind (usually GO_STRUCT)
 * @param ptr_data   Size of memory prefix containing pointers (for GC scanning)
 * @param gc_data    Pointer to gcdata bitmap (NULL for conservative scan)
 *
 * Example:
 *   DEFINE_GO_TYPE_DESC(__hchan_type, hchan, GO_STRUCT, sizeof(hchan), NULL);
 */
#define DEFINE_GO_TYPE_DESC(name, type_struct, kind, ptr_data, gc_data) \
    static struct __go_type_descriptor name = {                         \
        .__size = sizeof(type_struct),                                  \
        .__ptrdata = (ptr_data),                                        \
        .__hash = 0,                                                    \
        .__tflag = 0,                                                   \
        .__align = _Alignof(type_struct),                               \
        .__field_align = _Alignof(type_struct),                         \
        .__code = (kind),                                               \
        .__equalfn = NULL,                                              \
        .__gcdata = (gc_data),                                          \
        .__reflection = NULL,                                           \
        .__uncommon = NULL,                                             \
        .__pointer_to_this = NULL}

#endif // TYPE_DESCRIPTORS_H
