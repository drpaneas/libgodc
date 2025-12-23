#include "runtime.h"
#include "gc_semispace.h"
#include "type_descriptors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Symbol prefix for gccgo linknames.
#define _STRINGIFY2_(x) #x
#define _STRINGIFY_(x) _STRINGIFY2_(x)
#define GOSYM_PREFIX _STRINGIFY_(__USER_LABEL_PREFIX__)

// --- Interface type definitions ---

// Eface and Iface are defined in runtime.h

// Interface table (for non-empty interfaces)
// This matches gccgo's layout where the itab contains:
// - inter: the interface type
// - methods[]: variable-size array where methods[0] = concrete type, methods[1+] = function pointers
//
// The Iface.itab field points to &methods[0], so the compiler can do:
//   itab[0] to get the type
//   itab[m+1] to get method m's function pointer
typedef struct
{
    struct __go_type_descriptor *inter; // Interface type (for cache lookup)
    void *methods[];                    // C99 flexible array member: methods[0] = type, methods[1+] = function pointers
} Itab;

// ===== Helper functions =====

// Simple itab cache (linear search for now)
#define MAX_CACHED_ITABS 32
static struct
{
    Itab *itabs[MAX_CACHED_ITABS];
    int count;
} itab_cache = {.count = 0};

// Type equality check
// In gccgo, types are compared by pointer equality.
// Each distinct type has a unique type descriptor pointer.
static inline bool eqtype(struct __go_type_descriptor *t1, struct __go_type_descriptor *t2)
{
    return t1 == t2;
}

// String comparison for method names
// In gccgo, method names are stored as pointers to __go_string
static bool strings_equal_ptr(const struct __go_string *s1, const struct __go_string *s2)
{
    // First check if they're the same pointer (common case for interned strings)
    if (s1 == s2)
        return true;
    if (!s1 || !s2)
        return false;
    if (s1->__length != s2->__length)
        return false;
    if (s1->__length == 0)
        return true;
    return memcmp(s1->__data, s2->__data, s1->__length) == 0;
}

// Check if type implements interface
static bool implements_interface(struct __go_type_descriptor *itype,
                                 struct __go_type_descriptor *ctype)
{
    if (!itype || !ctype)
        return false;

    // Empty interface is implemented by all types
    struct __go_interface_type *ityp = (struct __go_interface_type *)itype;
    if (!ityp->__methods || ityp->__methods_count == 0)
    {
        return true;
    }

    // For non-empty interface, check that concrete type has all required methods
    if (!ctype->__uncommon)
    {
        // No methods means can't implement non-empty interface
        return false;
    }

    struct __go_uncommon_type *uncommon = (struct __go_uncommon_type *)ctype->__uncommon;
    if (!uncommon->__methods || uncommon->__methods_count == 0)
    {
        return false;
    }

    // For each method in the interface, find it in the concrete type
    // This matches gccgo's algorithm in iface.go (m *itab) init()
    intptr_t ri = 0;
    for (intptr_t li = 0; li < ityp->__methods_count; li++)
    {
        struct __go_interface_method *lhsMethod = &ityp->__methods[li];
        struct __go_method *rhsMethod;

        // Find matching method in concrete type
        while (1)
        {
            if (ri >= uncommon->__methods_count)
            {
                // No more methods in concrete type - doesn't implement
                return false;
            }

            rhsMethod = &uncommon->__methods[ri];

            // Check if names match (both are pointers to strings)
            if (strings_equal_ptr(lhsMethod->__name, rhsMethod->__name))
            {
                // NOTE: For complete Go semantics, we should also check pkgPath
                // for unexported methods. Skipped because Dreamcast games typically
                // use exported interfaces and gccgo validates this at compile time.
                break;
            }

            ri++;
        }

        // NOTE: For complete Go semantics, we should verify method types match
        // (lhsMethod->__typ == rhsMethod->__mtyp). Skipped because gccgo validates
        // method signatures at compile time for correct Go code.
        ri++;
    }

    return true;
}

// Helper to get the type from an itab
static inline struct __go_type_descriptor *itab_type(Itab *itab)
{
    return (struct __go_type_descriptor *)itab->methods[0];
}

// Helper to get Itab* from Iface.itab (which points to &itab->methods[0])
// Iface.itab = &itab->methods[0], so we need to subtract offsetof(methods)
static inline Itab *itab_from_iface(void *iface_itab)
{
    if (!iface_itab)
        return NULL;
    // iface_itab points to methods[0], subtract offset of methods within Itab
    return (Itab *)((uint8_t *)iface_itab - offsetof(Itab, methods));
}

// Helper to get methods array pointer for storing in Iface.itab
static inline void *itab_methods_ptr(Itab *itab)
{
    return itab ? &itab->methods[0] : NULL;
}

// Get or create interface table
// Returns pointer to the methods array (methods[0] = type, methods[1+] = functions)
static Itab *get_itab(struct __go_type_descriptor *inter, struct __go_type_descriptor *type)
{
    if (!inter || !type)
        return NULL;

    // Check cache first
    for (int i = 0; i < itab_cache.count; i++)
    {
        Itab *cached = itab_cache.itabs[i];
        if (cached && cached->inter == inter && itab_type(cached) == type)
        {
            return cached;
        }
    }

    // Check if type implements interface
    if (!implements_interface(inter, type))
    {
        return NULL;
    }

    // Calculate size needed for itab:
    // - sizeof(Itab) = base struct (inter pointer only, flexible array has size 0)
    // - methods[0] = type pointer
    // - methods[1..n] = function pointers (n = method_count)
    struct __go_interface_type *ityp = (struct __go_interface_type *)inter;
    intptr_t num_methods = ityp->__methods_count;
    // Allocate base struct plus (1 + num_methods) method slots
    size_t total_size = sizeof(Itab) + (1 + num_methods) * sizeof(void *);

    Itab *itab = (Itab *)runtime_malloc(total_size);
    if (!itab)
        return NULL;

    itab->inter = inter;
    itab->methods[0] = type; // methods[0] = concrete type

    // Build method table (methods[1+] = function pointers)
    // Match the algorithm from gccgo iface.go (m *itab) init()
    struct __go_uncommon_type *uncommon = (struct __go_uncommon_type *)type->__uncommon;
    intptr_t ri = 0;

    for (intptr_t li = 0; li < num_methods; li++)
    {
        struct __go_interface_method *lhsMethod = &ityp->__methods[li];

        // Find matching method in concrete type
        while (ri < uncommon->__methods_count)
        {
            struct __go_method *rhsMethod = &uncommon->__methods[ri];

            if (strings_equal_ptr(lhsMethod->__name, rhsMethod->__name))
            {
                // Found matching method
                itab->methods[li + 1] = rhsMethod->__tfn;
                ri++;
                break;
            }
            ri++;
        }
    }

    // Add to cache if space available
    if (itab_cache.count < MAX_CACHED_ITABS)
    {
        itab_cache.itabs[itab_cache.count++] = itab;
    }

    return itab;
}

// ===== Empty interface operations =====

// Convert non-empty interface to empty interface
// runtime.ifaceI2E2 - with ok return
void runtime_ifaceI2E2(Iface iface, Eface *ret, bool *ok) __asm__("_runtime.ifaceI2E2");
void runtime_ifaceI2E2(Iface iface, Eface *ret, bool *ok)
{
    if (!ret || !ok)
        return;

    if (!iface.itab)
    {
        // Nil interface
        ret->type = NULL;
        ret->data = NULL;
        *ok = false;
        return;
    }

    Itab *tab = itab_from_iface(iface.itab);
    ret->type = itab_type(tab);
    ret->data = iface.data;
    *ok = true;
}

// Convert empty interface to empty interface (identity, with ok)
// runtime.ifaceE2E2
void runtime_ifaceE2E2(Eface e, Eface *ret, bool *ok) __asm__("_runtime.ifaceE2E2");
void runtime_ifaceE2E2(Eface e, Eface *ret, bool *ok)
{
    if (!ret || !ok)
        return;

    ret->type = e.type;
    ret->data = e.data;
    *ok = (e.type != NULL);
}

// Convert empty interface to concrete type (pointer)
// runtime.ifaceE2T2P - returns (pointer, ok) in registers, not through output params!
E2T2P_result runtime_ifaceE2T2P(struct __go_type_descriptor *typ, Eface e) __asm__("_runtime.ifaceE2T2P");
E2T2P_result runtime_ifaceE2T2P(struct __go_type_descriptor *typ, Eface e)
{
    E2T2P_result result = {NULL, false};

    if (!e.type || e.type != typ)
    {
        return result;
    }

    result.ptr = e.data;
    result.ok = true;
    return result;
}

// Convert empty interface to concrete type (non-pointer)
// runtime.ifaceE2T2 with gccgo symbol name
bool runtime_ifaceE2T2(struct __go_type_descriptor *typ, Eface e, void *ret) __asm__("_runtime.ifaceE2T2");
bool runtime_ifaceE2T2(struct __go_type_descriptor *typ, Eface e, void *ret)
{
    if (!ret)
        return false;

    if (!e.type || e.type != typ)
    {
        // Zero the output on failure (Go requires zero value on failed assertion)
        if (typ)
            memset(ret, 0, typ->__size);
        return false;
    }

    // Read size from type descriptor (at offset 0 in gccgo-15.1.0)
    if (e.data)
    {
        memcpy(ret, e.data, typ->__size);
    }
    return true;
}

// ===== Non-empty interface operations =====

// Convert empty interface to non-empty interface
// runtime.ifaceE2I2
void runtime_ifaceE2I2(struct __go_type_descriptor *inter, Eface e, Iface *ret, bool *ok) __asm__("_runtime.ifaceE2I2");
void runtime_ifaceE2I2(struct __go_type_descriptor *inter, Eface e, Iface *ret, bool *ok)
{
    if (!ret || !ok)
        return;

    if (!e.type)
    {
        // Nil interface
        ret->itab = NULL;
        ret->data = NULL;
        *ok = false;
        return;
    }

    if (!implements_interface(inter, e.type))
    {
        ret->itab = NULL;
        ret->data = NULL;
        *ok = false;
        return;
    }

    Itab *tab = get_itab(inter, e.type);
    if (!tab)
    {
        // Allocation failed
        ret->itab = NULL;
        ret->data = NULL;
        *ok = false;
        return;
    }

    ret->itab = itab_methods_ptr(tab);
    ret->data = e.data;
    *ok = true;
}

// Convert non-empty interface to non-empty interface
// runtime.ifaceI2I2
void runtime_ifaceI2I2(struct __go_type_descriptor *inter, Iface i, Iface *ret, bool *ok) __asm__("_runtime.ifaceI2I2");
void runtime_ifaceI2I2(struct __go_type_descriptor *inter, Iface i, Iface *ret, bool *ok)
{
    if (!ret || !ok)
        return;

    if (!i.itab)
    {
        ret->itab = NULL;
        ret->data = NULL;
        *ok = false;
        return;
    }

    // For interface-to-interface conversion:
    // 1. Get concrete type from source interface's itab (stored at methods[0])
    // 2. Create new itab for target interface with that concrete type

    // The concrete type is stored at i.itab[0] (methods[0])
    struct __go_type_descriptor *ctype = *((struct __go_type_descriptor **)i.itab);

    // Get or create itab for target interface with the concrete type
    Itab *new_tab = get_itab(inter, ctype);
    if (!new_tab)
    {
        ret->itab = NULL;
        ret->data = NULL;
        *ok = false;
        return;
    }

    ret->itab = itab_methods_ptr(new_tab);
    ret->data = i.data;
    *ok = true;
}

// Convert non-empty interface to concrete type (pointer)
// runtime.ifaceI2T2P
void runtime_ifaceI2T2P(struct __go_type_descriptor *typ, Iface i, void **ret, bool *ok) __asm__("_runtime.ifaceI2T2P");
void runtime_ifaceI2T2P(struct __go_type_descriptor *typ, Iface i, void **ret, bool *ok)
{
    if (!ret || !ok)
        return;

    if (!i.itab)
    {
        *ret = NULL;
        *ok = false;
        return;
    }

    Itab *tab = itab_from_iface(i.itab);
    if (itab_type(tab) != typ)
    {
        *ret = NULL;
        *ok = false;
        return;
    }

    *ret = i.data;
    *ok = true;
}

// Convert non-empty interface to concrete type (non-pointer)
// runtime.ifaceI2T2
bool runtime_ifaceI2T2(struct __go_type_descriptor *typ, Iface i, void *ret) __asm__("_runtime.ifaceI2T2");
bool runtime_ifaceI2T2(struct __go_type_descriptor *typ, Iface i, void *ret)
{
    if (!ret)
        return false;

    // Early NULL check for typ - can't proceed without knowing the size
    if (!typ)
        return false;

    if (!i.itab)
    {
        // Zero the output on failure (Go requires zero value on failed assertion)
        memset(ret, 0, typ->__size);
        return false;
    }

    Itab *tab = itab_from_iface(i.itab);
    if (itab_type(tab) != typ)
    {
        // Zero the output on failure (Go requires zero value on failed assertion)
        memset(ret, 0, typ->__size);
        return false;
    }

    if (i.data)
    {
        memcpy(ret, i.data, typ->__size);
    }
    return true;
}

// ===== Interface table operations =====

// Get interface method table for type assertion
// runtime.assertitab
void *runtime_assertitab(struct __go_type_descriptor *inter, struct __go_type_descriptor *typ) __asm__("_runtime.assertitab");
void *runtime_assertitab(struct __go_type_descriptor *inter, struct __go_type_descriptor *typ)
{
    if (!inter || !typ)
    {
        runtime_panicstring("interface conversion: interface is nil, not a valid type");
        return NULL;
    }

    if (!implements_interface(inter, typ))
    {
        runtime_panicstring("interface conversion: type does not implement interface");
        return NULL;
    }

    void *itab = get_itab(inter, typ);
    if (!itab)
    {
        // implements_interface passed, so this is allocation failure
        runtime_panicstring("runtime: failed to allocate itab");
    }

    return itab;
}

// Get interface method table for assignment
// runtime.requireitab
void *runtime_requireitab(struct __go_type_descriptor *inter, struct __go_type_descriptor *typ) __asm__("_runtime.requireitab");
void *runtime_requireitab(struct __go_type_descriptor *inter, struct __go_type_descriptor *typ)
{
    if (!typ)
    {
        // Nil is allowed for assignment - this is the ONLY valid NULL return
        return NULL;
    }

    if (!implements_interface(inter, typ))
    {
        runtime_panicstring("interface conversion: type does not implement interface");
        return NULL;
    }

    void *itab = get_itab(inter, typ);
    if (!itab)
    {
        // implements_interface passed, so this is allocation failure
        runtime_panicstring("runtime: failed to allocate itab");
    }

    // gccgo expects the methods array pointer, not the Itab struct pointer!
    // Layout expected by gccgo: methods[0]=type, methods[1]=fn1, methods[2]=fn2...
    return itab_methods_ptr(itab);
}

// Check if type can be converted to interface
// runtime.ifaceT2Ip
bool runtime_ifaceT2Ip(struct __go_type_descriptor *inter, struct __go_type_descriptor *typ) __asm__("_runtime.ifaceT2Ip");
bool runtime_ifaceT2Ip(struct __go_type_descriptor *inter, struct __go_type_descriptor *typ)
{
    if (!inter || !typ)
        return false;
    return implements_interface(inter, typ);
}

// ===== Interface comparison =====

// Compare two empty interfaces
// runtime.efaceeq with gccgo symbol name
bool runtime_efaceeq(Eface e1, Eface e2) __asm__("_runtime.efaceeq");
bool runtime_efaceeq(Eface e1, Eface e2)
{
    // If types differ, not equal
    if (e1.type != e2.type)
        return false;

    // Both nil
    if (!e1.type && !e2.type)
        return true;

    // One nil, one not
    if (!e1.data || !e2.data)
        return (e1.data == e2.data);

    // Compare values based on type
    if (e1.type->__equalfn)
    {
        // Use type's equality function
        bool (*eq)(void *, void *) = (bool (*)(void *, void *))e1.type->__equalfn;
        return eq(e1.data, e2.data);
    }

    // Default: byte-wise comparison
    return memcmp(e1.data, e2.data, e1.type->__size) == 0;
}

// Compare empty interface to concrete value
// runtime.efacevaleq
bool runtime_efacevaleq(Eface e, struct __go_type_descriptor *typ, void *val) __asm__("_runtime.efacevaleq");
bool runtime_efacevaleq(Eface e, struct __go_type_descriptor *typ, void *val)
{
    if (e.type != typ)
        return false;
    if (!e.data || !val)
        return (e.data == val);

    if (typ->__equalfn)
    {
        bool (*eq)(void *, void *) = (bool (*)(void *, void *))typ->__equalfn;
        return eq(e.data, val);
    }

    return memcmp(e.data, val, typ->__size) == 0;
}

// Compare two non-empty interfaces
// runtime.ifaceeq
bool runtime_ifaceeq(Iface i1, Iface i2) __asm__("_runtime.ifaceeq");
bool runtime_ifaceeq(Iface i1, Iface i2)
{
    // If itabs differ, not equal
    if (i1.itab != i2.itab)
        return false;

    // Both nil
    if (!i1.itab && !i2.itab)
        return true;

    // One nil, one not
    if (!i1.data || !i2.data)
        return (i1.data == i2.data);

    // Same itab, compare data
    Itab *tab = itab_from_iface(i1.itab);
    struct __go_type_descriptor *typ = itab_type(tab);

    if (typ->__equalfn)
    {
        bool (*eq)(void *, void *) = (bool (*)(void *, void *))typ->__equalfn;
        return eq(i1.data, i2.data);
    }

    return memcmp(i1.data, i2.data, typ->__size) == 0;
}

// Compare non-empty interface to concrete value
// runtime.ifacevaleq
bool runtime_ifacevaleq(Iface i, struct __go_type_descriptor *typ, void *val) __asm__("_runtime.ifacevaleq");
bool runtime_ifacevaleq(Iface i, struct __go_type_descriptor *typ, void *val)
{
    if (!i.itab)
        return false;

    Itab *tab = itab_from_iface(i.itab);
    if (itab_type(tab) != typ)
        return false;

    if (!i.data || !val)
        return (i.data == val);

    if (typ->__equalfn)
    {
        bool (*eq)(void *, void *) = (bool (*)(void *, void *))typ->__equalfn;
        return eq(i.data, val);
    }

    return memcmp(i.data, val, typ->__size) == 0;
}

// Compare non-empty interface to empty interface
// runtime.ifaceefaceeq
bool runtime_ifaceefaceeq(Iface i, Eface e) __asm__("_runtime.ifaceefaceeq");
bool runtime_ifaceefaceeq(Iface i, Eface e)
{
    if (!i.itab)
        return !e.type;

    Itab *tab = itab_from_iface(i.itab);
    if (itab_type(tab) != e.type)
        return false;

    if (!i.data || !e.data)
        return (i.data == e.data);

    struct __go_type_descriptor *typ = itab_type(tab);
    if (typ->__equalfn)
    {
        bool (*eq)(void *, void *) = (bool (*)(void *, void *))typ->__equalfn;
        return eq(i.data, e.data);
    }

    return memcmp(i.data, e.data, typ->__size) == 0;
}

// Compare two type descriptors
// runtime.eqtype
bool runtime_eqtype(struct __go_type_descriptor *t1, struct __go_type_descriptor *t2) __asm__("_runtime.eqtype");
bool runtime_eqtype(struct __go_type_descriptor *t1, struct __go_type_descriptor *t2)
{
    return t1 == t2;
}

// ===== Interface printing (for debugging) =====

// Print empty interface
// runtime.printeface with gccgo symbol name
void runtime_printeface(Eface e) __asm__("_runtime.printeface");
void runtime_printeface(Eface e)
{
    printf("(");
    if (e.type)
    {
        // Validate type pointer is in reasonable memory range
        uintptr_t type_addr = (uintptr_t)e.type;
        if (type_addr < 0x8c000000 || type_addr > 0x8e000000)
        {
            printf("invalid_type@%p", e.type);
        }
        else if (e.type->__reflection && e.type->__reflection->__data &&
                 e.type->__reflection->__length > 0 && e.type->__reflection->__length < 256)
        {
            // Validate reflection data pointer
            uintptr_t data_addr = (uintptr_t)e.type->__reflection->__data;
            if (data_addr >= 0x8c000000 && data_addr < 0x8e000000)
            {
                printf("%.*s", (int)e.type->__reflection->__length, e.type->__reflection->__data);
            }
            else
            {
                printf("type@%p", e.type);
            }
        }
        else
        {
            printf("type@%p", e.type);
        }
        printf(",");
        if (e.data)
        {
            // For string types, try to print the string value
            // Go string type code is usually 24 (STRING)
            if (e.type->__code == 24 && e.data)
            {
                // e.data points to a string header
                GoString *str = (GoString *)e.data;
                uintptr_t str_addr = (uintptr_t)str->str;
                if (str_addr >= 0x8c000000 && str_addr < 0x8e000000 && str->len > 0 &&
                    str->len < 1024)
                {
                    printf("\"%.*s\"", (int)str->len, str->str);
                }
                else
                {
                    printf("%p", e.data);
                }
            }
            else
            {
                printf("%p", e.data);
            }
        }
        else
        {
            printf("nil");
        }
    }
    else
    {
        printf("nil,nil");
    }
    printf(")");
}

// Print non-empty interface
// runtime.printiface
void runtime_printiface(Iface i) __asm__("_runtime.printiface");
void runtime_printiface(Iface i)
{
    printf("(");
    if (i.itab)
    {
        Itab *tab = itab_from_iface(i.itab);
        if (itab_type(tab) && itab_type(tab)->__reflection && itab_type(tab)->__reflection->__data)
        {
            printf(
                "%.*s", (int)itab_type(tab)->__reflection->__length, itab_type(tab)->__reflection->__data);
        }
        else
        {
            printf("type@%p", itab_type(tab));
        }
        printf(",");
        if (i.data)
        {
            printf("%p", i.data);
        }
        else
        {
            printf("nil");
        }
    }
    else
    {
        printf("nil,nil");
    }
    printf(")");
}

// ===== Type assertion panic =====

// Panic when interface type assertion fails
// runtime.panicdottype with gccgo symbol name
void runtime_panicdottype(struct __go_type_descriptor *have,
                          struct __go_type_descriptor *want,
                          struct __go_type_descriptor *iface) __asm__("_runtime.panicdottype");
void runtime_panicdottype(struct __go_type_descriptor *have,
                          struct __go_type_descriptor *want,
                          struct __go_type_descriptor *iface)
{
    printf("panic: interface conversion: ");

    if (have)
    {
        if (have->__reflection && have->__reflection->__data)
        {
            printf("%.*s", (int)have->__reflection->__length, have->__reflection->__data);
        }
        else
        {
            printf("<unknown type>");
        }
    }
    else
    {
        printf("nil");
    }

    printf(" is not ");

    if (want)
    {
        if (want->__reflection && want->__reflection->__data)
        {
            printf("%.*s", (int)want->__reflection->__length, want->__reflection->__data);
        }
        else
        {
            printf("<unknown type>");
        }
    }
    else
    {
        printf("<unknown type>");
    }

    printf("\n");
    abort();
}

// ============================================================================
// Interface Equality Function Pointers (for type descriptors)
// ============================================================================
// NOTE: Hash functions (memhash8, memhash16, memhash32, etc.) are in runtime_stubs.c
// with correct gccgo signatures: (void *key, uintptr_t seed)

// Compare two non-empty interfaces (function pointer for type descriptor)
_Bool runtime_interequal_f(void *p, void *q) __asm__("_runtime.interequal..f");
_Bool runtime_interequal_f(void *p, void *q)
{
    Iface *i1 = (Iface *)p;
    Iface *i2 = (Iface *)q;
    return runtime_ifaceeq(*i1, *i2);
}

// Compare two empty interfaces (nil interface equality)
_Bool runtime_nilinterequal_f(void *p, void *q) __asm__("_runtime.nilinterequal..f");
_Bool runtime_nilinterequal_f(void *p, void *q)
{
    Eface *e1 = (Eface *)p;
    Eface *e2 = (Eface *)q;
    return runtime_efaceeq(*e1, *e2);
}

// ============================================================================
// Static Integer Storage (avoids allocations for small integer values)
// ============================================================================

// staticuint64s is used to avoid allocating in convT* for small integer values.
// This matches the gccgo runtime's optimization in iface.go
static const uint64_t staticuint64s[256] = {
    0x00,
    0x01,
    0x02,
    0x03,
    0x04,
    0x05,
    0x06,
    0x07,
    0x08,
    0x09,
    0x0a,
    0x0b,
    0x0c,
    0x0d,
    0x0e,
    0x0f,
    0x10,
    0x11,
    0x12,
    0x13,
    0x14,
    0x15,
    0x16,
    0x17,
    0x18,
    0x19,
    0x1a,
    0x1b,
    0x1c,
    0x1d,
    0x1e,
    0x1f,
    0x20,
    0x21,
    0x22,
    0x23,
    0x24,
    0x25,
    0x26,
    0x27,
    0x28,
    0x29,
    0x2a,
    0x2b,
    0x2c,
    0x2d,
    0x2e,
    0x2f,
    0x30,
    0x31,
    0x32,
    0x33,
    0x34,
    0x35,
    0x36,
    0x37,
    0x38,
    0x39,
    0x3a,
    0x3b,
    0x3c,
    0x3d,
    0x3e,
    0x3f,
    0x40,
    0x41,
    0x42,
    0x43,
    0x44,
    0x45,
    0x46,
    0x47,
    0x48,
    0x49,
    0x4a,
    0x4b,
    0x4c,
    0x4d,
    0x4e,
    0x4f,
    0x50,
    0x51,
    0x52,
    0x53,
    0x54,
    0x55,
    0x56,
    0x57,
    0x58,
    0x59,
    0x5a,
    0x5b,
    0x5c,
    0x5d,
    0x5e,
    0x5f,
    0x60,
    0x61,
    0x62,
    0x63,
    0x64,
    0x65,
    0x66,
    0x67,
    0x68,
    0x69,
    0x6a,
    0x6b,
    0x6c,
    0x6d,
    0x6e,
    0x6f,
    0x70,
    0x71,
    0x72,
    0x73,
    0x74,
    0x75,
    0x76,
    0x77,
    0x78,
    0x79,
    0x7a,
    0x7b,
    0x7c,
    0x7d,
    0x7e,
    0x7f,
    0x80,
    0x81,
    0x82,
    0x83,
    0x84,
    0x85,
    0x86,
    0x87,
    0x88,
    0x89,
    0x8a,
    0x8b,
    0x8c,
    0x8d,
    0x8e,
    0x8f,
    0x90,
    0x91,
    0x92,
    0x93,
    0x94,
    0x95,
    0x96,
    0x97,
    0x98,
    0x99,
    0x9a,
    0x9b,
    0x9c,
    0x9d,
    0x9e,
    0x9f,
    0xa0,
    0xa1,
    0xa2,
    0xa3,
    0xa4,
    0xa5,
    0xa6,
    0xa7,
    0xa8,
    0xa9,
    0xaa,
    0xab,
    0xac,
    0xad,
    0xae,
    0xaf,
    0xb0,
    0xb1,
    0xb2,
    0xb3,
    0xb4,
    0xb5,
    0xb6,
    0xb7,
    0xb8,
    0xb9,
    0xba,
    0xbb,
    0xbc,
    0xbd,
    0xbe,
    0xbf,
    0xc0,
    0xc1,
    0xc2,
    0xc3,
    0xc4,
    0xc5,
    0xc6,
    0xc7,
    0xc8,
    0xc9,
    0xca,
    0xcb,
    0xcc,
    0xcd,
    0xce,
    0xcf,
    0xd0,
    0xd1,
    0xd2,
    0xd3,
    0xd4,
    0xd5,
    0xd6,
    0xd7,
    0xd8,
    0xd9,
    0xda,
    0xdb,
    0xdc,
    0xdd,
    0xde,
    0xdf,
    0xe0,
    0xe1,
    0xe2,
    0xe3,
    0xe4,
    0xe5,
    0xe6,
    0xe7,
    0xe8,
    0xe9,
    0xea,
    0xeb,
    0xec,
    0xed,
    0xee,
    0xef,
    0xf0,
    0xf1,
    0xf2,
    0xf3,
    0xf4,
    0xf5,
    0xf6,
    0xf7,
    0xf8,
    0xf9,
    0xfa,
    0xfb,
    0xfc,
    0xfd,
    0xfe,
    0xff,
};

// ============================================================================
// isDirectIface - Check if type is stored directly in interface
// ============================================================================

// kindDirectIface flag from Go's typekind.go
#define GO_KIND_DIRECT_IFACE 0x20

// Check if type is stored directly in interface value
// Direct interface types are: pointer, channel, map, func, and single-element
// structs/arrays of pointer-shaped types
static inline bool isDirectIface(struct __go_type_descriptor *t)
{
    if (!t)
        return false;
    return (t->__code & GO_KIND_DIRECT_IFACE) != 0;
}

// ============================================================================
// Type Hash Functions (for maps with interface keys)
// ============================================================================

// FNV-1a hash constants
#define FNV_OFFSET_BASIS 0x811c9dc5
#define FNV_PRIME 0x01000193

// Hash constants from gccgo (same as gc runtime)
// c0 and c1 are mixing constants
#define HASH_C0 ((uintptr_t)2860486313UL)
#define HASH_C1 ((uintptr_t)3267000013UL)

// Forward declaration for typehash
static uintptr_t typehash(struct __go_type_descriptor *t, void *p, uintptr_t h);

// memhash - hash arbitrary memory
static uintptr_t memhash(void *p, uintptr_t h, uintptr_t size)
{
    if (!p || size == 0)
        return h;

    const uint8_t *data = (const uint8_t *)p;
    uintptr_t hash = h ^ FNV_OFFSET_BASIS;

    for (uintptr_t i = 0; i < size; i++)
    {
        hash ^= data[i];
        hash *= FNV_PRIME;
    }

    return hash;
}

// memhash64 - hash 8 bytes
static uintptr_t memhash64(void *p, uintptr_t h)
{
    return memhash(p, h, 8);
}

// strhash - hash a Go string
static uintptr_t strhash(void *p, uintptr_t h)
{
    struct __go_string *s = (struct __go_string *)p;
    if (!s || !s->__data)
        return h;
    return memhash((void *)s->__data, h, s->__length);
}

// f32hash - hash a float32 (handling +0, -0, NaN)
static uintptr_t f32hash(void *p, uintptr_t h)
{
    float f = *(float *)p;
    if (f == 0)
    {
        return HASH_C1 * (HASH_C0 ^ h); // +0, -0
    }
    if (f != f)
    {
        // NaN - use a random-ish value based on current hash
        return HASH_C1 * (HASH_C0 ^ h ^ (uintptr_t)0xDEADBEEF);
    }
    return memhash(p, h, 4);
}

// f64hash - hash a float64 (handling +0, -0, NaN)
static uintptr_t f64hash(void *p, uintptr_t h)
{
    double f = *(double *)p;
    if (f == 0)
    {
        return HASH_C1 * (HASH_C0 ^ h); // +0, -0
    }
    if (f != f)
    {
        // NaN - use a random-ish value based on current hash
        return HASH_C1 * (HASH_C0 ^ h ^ (uintptr_t)0xDEADBEEF);
    }
    return memhash(p, h, 8);
}

// c64hash - hash a complex64
static uintptr_t c64hash(void *p, uintptr_t h)
{
    float *x = (float *)p;
    return f32hash(&x[1], f32hash(&x[0], h));
}

// c128hash - hash a complex128
static uintptr_t c128hash(void *p, uintptr_t h)
{
    double *x = (double *)p;
    return f64hash(&x[1], f64hash(&x[0], h));
}

// typehash - compute hash of object of type t at address p
// This is used for hashing interface values in maps
static uintptr_t typehash(struct __go_type_descriptor *t, void *p, uintptr_t h)
{
    if (!t || !p)
        return h;

    uint8_t kind = t->__code & 0x1F; // GO_KIND_MASK

    switch (kind)
    {
    case GO_BOOL:
    case GO_INT8:
    case GO_UINT8:
        return memhash(p, h, 1);

    case GO_INT16:
    case GO_UINT16:
        return memhash(p, h, 2);

    case GO_INT32:
    case GO_UINT32:
    case GO_FLOAT32:
        if (kind == GO_FLOAT32)
            return f32hash(p, h);
        return memhash(p, h, 4);

    case GO_INT64:
    case GO_UINT64:
    case GO_FLOAT64:
        if (kind == GO_FLOAT64)
            return f64hash(p, h);
        return memhash64(p, h);

    case GO_INT:
    case GO_UINT:
    case GO_UINTPTR:
        return memhash(p, h, sizeof(uintptr_t));

    case GO_COMPLEX64:
        return c64hash(p, h);

    case GO_COMPLEX128:
        return c128hash(p, h);

    case GO_STRING:
        return strhash(p, h);

    case GO_PTR:
    case GO_CHAN:
    case GO_UNSAFE_POINTER:
        return memhash(p, h, sizeof(void *));

    case GO_INTERFACE:
    {
        // Hash the interface - get type from itab or eface
        struct __go_interface_type *ityp = (struct __go_interface_type *)t;
        if (ityp->__methods_count == 0)
        {
            // Empty interface (eface)
            Eface *e = (Eface *)p;
            if (!e->type)
                return h;
            if (isDirectIface(e->type))
            {
                return HASH_C1 * typehash(e->type, &e->data, h ^ HASH_C0);
            }
            else
            {
                return HASH_C1 * typehash(e->type, e->data, h ^ HASH_C0);
            }
        }
        else
        {
            // Non-empty interface (iface)
            Iface *i = (Iface *)p;
            if (!i->itab)
                return h;
            Itab *tab = itab_from_iface(i->itab);
            struct __go_type_descriptor *typ = itab_type(tab);
            if (isDirectIface(typ))
            {
                return HASH_C1 * typehash(typ, &i->data, h ^ HASH_C0);
            }
            else
            {
                return HASH_C1 * typehash(typ, i->data, h ^ HASH_C0);
            }
        }
    }

    case GO_ARRAY:
    {
        struct __go_array_type *a = (struct __go_array_type *)t;
        for (uintptr_t i = 0; i < a->__len; i++)
        {
            h = typehash(a->__element_type,
                         (uint8_t *)p + i * a->__element_type->__size, h);
        }
        return h;
    }

    case GO_STRUCT:
    {
        struct __go_struct_type *s = (struct __go_struct_type *)t;
        for (uintptr_t i = 0; i < s->__fields_count; i++)
        {
            struct __go_struct_field *f = &s->__fields[i];
            // Skip blank fields (name starts with '_' or is empty)
            if (f->__name.__length > 0 && f->__name.__data[0] == '_' &&
                f->__name.__length == 1)
            {
                continue;
            }
            // Recursive call will panic if field is unhashable
            h = typehash(f->__typ, (uint8_t *)p + f->__offset, h);
        }
        return h;
    }

    // UNHASHABLE TYPES - must panic!
    case GO_SLICE:
        runtime_panicstring("runtime error: hash of unhashable type []T");
        return 0; // Unreachable

    case GO_MAP:
        runtime_panicstring("runtime error: hash of unhashable type map");
        return 0; // Unreachable

    case GO_FUNC:
        runtime_panicstring("runtime error: hash of unhashable type func");
        return 0; // Unreachable

    default:
        // Unknown type - panic rather than silently fail
        runtime_panicstring("runtime error: hash of unknown type");
        return 0; // Unreachable
    }
}

// interhash - hash a non-empty interface (iface)
// runtime.interhash with gccgo symbol
uintptr_t runtime_interhash(void *p, uintptr_t h) __asm__("_runtime.interhash");
uintptr_t runtime_interhash(void *p, uintptr_t h)
{
    Iface *i = (Iface *)p;
    if (!i->itab)
        return h; // nil interface hashes to seed (this is correct)

    Itab *tab = itab_from_iface(i->itab);
    struct __go_type_descriptor *t = itab_type(tab);

    // Check if type is hashable (has equality function)
    // Types without __equalfn (slices, maps, funcs) are unhashable
    if (!t->__equalfn)
    {
        runtime_panicstring("runtime error: hash of unhashable type");
        return 0; // Unreachable
    }

    // typehash will also panic for unhashable types
    if (isDirectIface(t))
    {
        return HASH_C1 * typehash(t, &i->data, h ^ HASH_C0);
    }
    else
    {
        return HASH_C1 * typehash(t, i->data, h ^ HASH_C0);
    }
}

// interhash function pointer form for type descriptors
uintptr_t runtime_interhash_f(void *p, uintptr_t h) __asm__("_runtime.interhash..f");
uintptr_t runtime_interhash_f(void *p, uintptr_t h)
{
    return runtime_interhash(p, h);
}

// nilinterhash - hash an empty interface (eface/interface{})
// runtime.nilinterhash with gccgo symbol
uintptr_t runtime_nilinterhash(void *p, uintptr_t h) __asm__("_runtime.nilinterhash");
uintptr_t runtime_nilinterhash(void *p, uintptr_t h)
{
    Eface *e = (Eface *)p;
    if (!e->type)
        return h; // nil interface hashes to seed (this is correct)

    struct __go_type_descriptor *t = e->type;

    // Check if type is hashable (has equality function)
    // Types without __equalfn (slices, maps, funcs) are unhashable
    if (!t->__equalfn)
    {
        runtime_panicstring("runtime error: hash of unhashable type");
        return 0; // Unreachable
    }

    // typehash will also panic for unhashable types
    if (isDirectIface(t))
    {
        return HASH_C1 * typehash(t, &e->data, h ^ HASH_C0);
    }
    else
    {
        return HASH_C1 * typehash(t, e->data, h ^ HASH_C0);
    }
}

// nilinterhash function pointer form for type descriptors
uintptr_t runtime_nilinterhash_f(void *p, uintptr_t h) __asm__("_runtime.nilinterhash..f");
uintptr_t runtime_nilinterhash_f(void *p, uintptr_t h)
{
    return runtime_nilinterhash(p, h);
}

// ============================================================================
// convT Functions - Boxing values into interface{}
// ============================================================================

// convT - box a generic value into interface (allocates and copies)
void *runtime_convT(struct __go_type_descriptor *t, void *v) __asm__("_runtime.convT");
void *runtime_convT(struct __go_type_descriptor *t, void *v)
{
    if (!t || !v)
        return NULL;

    void *x = gc_alloc(t->__size, t);
    if (x)
    {
        memcpy(x, v, t->__size);
    }
    return x;
}

// convTnoptr - box a value that contains no pointers (NOSCAN allocation)
void *runtime_convTnoptr(struct __go_type_descriptor *t, void *v) __asm__("_runtime.convTnoptr");
void *runtime_convTnoptr(struct __go_type_descriptor *t, void *v)
{
    if (!t || !v)
        return NULL;

    // Allocate without pointer scanning (t = NULL means NOSCAN)
    void *x = gc_alloc(t->__size, NULL);
    if (x)
    {
        memcpy(x, v, t->__size);
    }
    return x;
}

// convT16 - box a 16-bit value
void *runtime_convT16(uint16_t v) __asm__("_runtime.convT16");
void *runtime_convT16(uint16_t v)
{
    if (v < 256)
    {
        // Use static storage for small values
        return (void *)&staticuint64s[v];
    }

    uint16_t *p = (uint16_t *)gc_alloc(2, NULL);
    if (p)
        *p = v;
    return p;
}

// convT32 - box a 32-bit value
void *runtime_convT32(uint32_t v) __asm__("_runtime.convT32");
void *runtime_convT32(uint32_t v)
{
    if (v < 256)
    {
        // Use static storage for small values
        return (void *)&staticuint64s[v];
    }

    uint32_t *p = (uint32_t *)gc_alloc(4, NULL);
    if (p)
        *p = v;
    return p;
}

// convT64 - box a 64-bit value
void *runtime_convT64(uint64_t v) __asm__("_runtime.convT64");
void *runtime_convT64(uint64_t v)
{
    if (v < 256)
    {
        // Use static storage for small values
        return (void *)&staticuint64s[v];
    }

    uint64_t *p = (uint64_t *)gc_alloc(8, NULL);
    if (p)
        *p = v;
    return p;
}

// Type descriptor for allocating string HEADERS (not string types).
// Used only by convTstring for GC allocation - tells GC about the __data pointer.
// Placed in .rodata to save RAM on Dreamcast.
//
// Note: __code is GO_STRING (not GO_STRUCT) because:
// 1. This represents string memory semantically
// 2. If accidentally used in typehash, will correctly handle as string
// 3. The actual string type descriptor comes from the compiler
// 4. GC only uses __ptrdata, not __code, for scanning decisions
static const struct __go_type_descriptor __go_string_header_type = {
    .__size = sizeof(struct __go_string),
    .__ptrdata = sizeof(void *), // First field (__data) is a pointer
    .__hash = 0,
    .__tflag = 0,
    .__align = _Alignof(struct __go_string),
    .__field_align = _Alignof(struct __go_string),
    .__code = GO_STRING, // Semantic type, not structural
    .__equalfn = NULL,   // Not used - real type has proper equalfn
    .__gcdata = NULL,
    .__reflection = NULL,
    .__uncommon = NULL,
    .__pointer_to_this = NULL,
};

// Type descriptor for allocating slice HEADERS (not slice types).
// Used only by convTslice for GC allocation - tells GC about the __values pointer.
// Placed in .rodata to save RAM on Dreamcast.
//
// Note: __code is GO_SLICE (not GO_STRUCT) because:
// 1. This represents slice memory semantically
// 2. If accidentally used in typehash, will correctly panic as unhashable
// 3. The actual slice type descriptor comes from the compiler
// 4. GC only uses __ptrdata, not __code, for scanning decisions
static const struct __go_type_descriptor __go_slice_header_type = {
    .__size = sizeof(struct __go_open_array),
    .__ptrdata = sizeof(void *), // First field (__values) is a pointer
    .__hash = 0,
    .__tflag = 0,
    .__align = _Alignof(struct __go_open_array),
    .__field_align = _Alignof(struct __go_open_array),
    .__code = GO_SLICE, // Semantic type, not structural
    .__equalfn = NULL,  // Not used - slices are unhashable anyway
    .__gcdata = NULL,
    .__reflection = NULL,
    .__uncommon = NULL,
    .__pointer_to_this = NULL,
};

// convTstring - box a string
void *runtime_convTstring(struct __go_string s) __asm__("_runtime.convTstring");
void *runtime_convTstring(struct __go_string s)
{
    // String header contains __data pointer - must use typed allocation for GC
    // Cast away const - gc_alloc only reads the type descriptor
    struct __go_string *p = (struct __go_string *)gc_alloc(
        sizeof(struct __go_string),
        (struct __go_type_descriptor *)&__go_string_header_type);
    if (p)
    {
        *p = s;
    }
    return p;
}

// convTslice - box a slice
void *runtime_convTslice(struct __go_open_array s) __asm__("_runtime.convTslice");
void *runtime_convTslice(struct __go_open_array s)
{
    // Slice header contains __values pointer - must use typed allocation for GC
    // Cast away const - gc_alloc only reads the type descriptor
    struct __go_open_array *p = (struct __go_open_array *)gc_alloc(
        sizeof(struct __go_open_array),
        (struct __go_type_descriptor *)&__go_slice_header_type);
    if (p)
    {
        *p = s;
    }
    return p;
}

// ============================================================================
// Type Assertion Functions (gccgo linkname versions)
// ============================================================================

// assertE2I - assert empty interface to non-empty interface
Iface runtime_assertE2I(struct __go_type_descriptor *inter, Eface e) __asm__("_runtime.assertE2I");
Iface runtime_assertE2I(struct __go_type_descriptor *inter, Eface e)
{
    Iface ret;

    if (!e.type)
    {
        runtime_panicstring("interface conversion: interface is nil");
        ret.itab = NULL;
        ret.data = NULL;
        return ret;
    }

    // First check: Does the type implement the interface?
    if (!implements_interface(inter, e.type))
    {
        // Definite type mismatch - correct error message
        runtime_panicdottype(e.type, inter, inter);
        ret.itab = NULL;
        ret.data = NULL;
        return ret;
    }

    // Type implements interface, now get/create itab
    Itab *itab = get_itab(inter, e.type);
    if (!itab)
    {
        // implements_interface passed, so this is allocation failure
        runtime_panicstring("runtime: failed to allocate interface table");
        ret.itab = NULL;
        ret.data = NULL;
        return ret;
    }

    ret.itab = itab_methods_ptr(itab);
    ret.data = e.data;
    return ret;
}

// assertE2I2 - assert empty interface to non-empty interface with ok
void runtime_assertE2I2(struct __go_type_descriptor *inter, Eface e, Iface *ret, _Bool *ok) __asm__("_runtime.assertE2I2");
void runtime_assertE2I2(struct __go_type_descriptor *inter, Eface e, Iface *ret, _Bool *ok)
{
    if (!ret || !ok)
        return;

    if (!e.type)
    {
        ret->itab = NULL;
        ret->data = NULL;
        *ok = false;
        return;
    }

    Itab *itab = get_itab(inter, e.type);
    if (!itab)
    {
        ret->itab = NULL;
        ret->data = NULL;
        *ok = false;
        return;
    }

    ret->itab = itab_methods_ptr(itab);
    ret->data = e.data;
    *ok = true;
}

// assertI2I - assert non-empty interface to non-empty interface
Iface runtime_assertI2I(struct __go_type_descriptor *inter, Iface i) __asm__("_runtime.assertI2I");
Iface runtime_assertI2I(struct __go_type_descriptor *inter, Iface i)
{
    Iface ret;

    if (!i.itab)
    {
        runtime_panicstring("interface conversion: interface is nil");
        ret.itab = NULL;
        ret.data = NULL;
        return ret;
    }

    Itab *tab = itab_from_iface(i.itab);
    struct __go_type_descriptor *concrete_type = itab_type(tab);

    // First check: Does the type implement the target interface?
    if (!implements_interface(inter, concrete_type))
    {
        // Definite type mismatch - correct error message
        runtime_panicdottype(concrete_type, inter, tab->inter);
        ret.itab = NULL;
        ret.data = NULL;
        return ret;
    }

    // Type implements interface, now get/create itab
    Itab *new_itab = get_itab(inter, concrete_type);
    if (!new_itab)
    {
        // implements_interface passed, so this is allocation failure
        runtime_panicstring("runtime: failed to allocate interface table");
        ret.itab = NULL;
        ret.data = NULL;
        return ret;
    }

    ret.itab = itab_methods_ptr(new_itab);
    ret.data = i.data;
    return ret;
}

// assertI2I2 - assert non-empty interface to non-empty interface with ok
void runtime_assertI2I2(struct __go_type_descriptor *inter, Iface i, Iface *ret, _Bool *ok) __asm__("_runtime.assertI2I2");
void runtime_assertI2I2(struct __go_type_descriptor *inter, Iface i, Iface *ret, _Bool *ok)
{
    if (!ret || !ok)
        return;

    if (!i.itab)
    {
        ret->itab = NULL;
        ret->data = NULL;
        *ok = false;
        return;
    }

    Itab *tab = itab_from_iface(i.itab);
    Itab *new_itab = get_itab(inter, itab_type(tab));
    if (!new_itab)
    {
        ret->itab = NULL;
        ret->data = NULL;
        *ok = false;
        return;
    }

    ret->itab = itab_methods_ptr(new_itab);
    ret->data = i.data;
    *ok = true;
}

// ============================================================================
// Panic Functions (gccgo linkname versions)
// ============================================================================

// panicdottypeE - panic for empty interface type assertion failure
void runtime_panicdottypeE(struct __go_type_descriptor *have, struct __go_type_descriptor *want) __asm__("_runtime.panicdottypeE");
void runtime_panicdottypeE(struct __go_type_descriptor *have, struct __go_type_descriptor *want)
{
    runtime_panicdottype(have, want, NULL);
}

// panicdottypeI - panic for non-empty interface type assertion failure
void runtime_panicdottypeI(struct __go_type_descriptor *have,
                           struct __go_type_descriptor *want,
                           struct __go_type_descriptor *iface) __asm__("_runtime.panicdottypeI");
void runtime_panicdottypeI(struct __go_type_descriptor *have,
                           struct __go_type_descriptor *want,
                           struct __go_type_descriptor *iface)
{
    runtime_panicdottype(have, want, iface);
}

// panicnildottype - panic when asserting from nil interface
void runtime_panicnildottype(struct __go_type_descriptor *want) __asm__("_runtime.panicnildottype");
void runtime_panicnildottype(struct __go_type_descriptor *want)
{
    runtime_panicdottype(NULL, want, NULL);
}

// ============================================================================
// getitab - Get or create interface table (public version for C callers)
// ============================================================================

// This matches the declaration in libgo's runtime.h
void *getitab(const struct __go_type_descriptor *lhs, const struct __go_type_descriptor *rhs, _Bool canfail) __asm__(GOSYM_PREFIX "runtime.getitab");
void *getitab(const struct __go_type_descriptor *lhs, const struct __go_type_descriptor *rhs, _Bool canfail)
{
    // Cast away const - our internal functions handle this
    Itab *result = get_itab((struct __go_type_descriptor *)lhs,
                            (struct __go_type_descriptor *)rhs);

    if (!result && !canfail)
    {
        // Type does not implement interface - panic
        runtime_panicdottype((struct __go_type_descriptor *)rhs,
                             (struct __go_type_descriptor *)lhs, NULL);
    }

    // For gccgo, return pointer to methods array, not itab struct
    // The first entry in methods is the type pointer, followed by function pointers
    return result ? &result->methods[0] : NULL;
}
