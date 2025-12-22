/* libgodc/runtime/runtime.h - Go runtime for Dreamcast */
#ifndef GODC_RUNTIME_H
#define GODC_RUNTIME_H

#include "godc_config.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <kos.h>
#include <arch/arch.h>
#include <arch/cache.h>

/* GC subsystem */
#include "gc_semispace.h"

/* Branch hints */
#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

/* Compiler barrier for context switches */
#define CONTEXT_SWITCH_BARRIER() __asm__ volatile("" ::: "memory")

/* Go string */
#ifndef GODC_GOSTRING_DEFINED
#define GODC_GOSTRING_DEFINED
typedef struct {
    const uint8_t *str;
    intptr_t len;
} GoString;
#endif

/* Go slice */
typedef struct {
    void *__values;
    int __count;
    int __capacity;
} GoSlice;

/* Forward declarations */
struct __go_type_descriptor;

/* Empty interface */
#ifndef GODC_EFACE_DEFINED
#define GODC_EFACE_DEFINED
typedef struct {
    struct __go_type_descriptor *type;
    void *data;
} Eface;
#endif

/* Non-empty interface */
typedef struct {
    void *itab;
    void *data;
} Iface;

/* Panic/defer subsystem */
#include "panic_dreamcast.h"

/* Goroutines */
#include "goroutine.h"

/* Core runtime functions */
void runtime_init(void);
void *runtime_malloc(size_t size);
void *runtime_mallocgc(size_t size, struct __go_type_descriptor *type, bool needzero);
void runtime_GC(void);
int32_t debug_SetGCPercent(int32_t percent);
G *runtime_getg(void);

/* Interface operations */
void runtime_ifaceI2E2(Iface iface, Eface *ret, bool *ok);
void runtime_ifaceE2E2(Eface e, Eface *ret, bool *ok);
void runtime_ifaceE2I2(struct __go_type_descriptor *inter, Eface e, Iface *ret, bool *ok);
void runtime_ifaceI2I2(struct __go_type_descriptor *inter, Iface i, Iface *ret, bool *ok);

typedef struct { void *ptr; _Bool ok; } E2T2P_result;
E2T2P_result runtime_ifaceE2T2P(struct __go_type_descriptor *typ, Eface e);
bool runtime_ifaceE2T2(struct __go_type_descriptor *typ, Eface e, void *ret);
void runtime_ifaceI2T2P(struct __go_type_descriptor *typ, Iface i, void **ret, bool *ok);
bool runtime_ifaceI2T2(struct __go_type_descriptor *typ, Iface i, void *ret);

void *runtime_assertitab(struct __go_type_descriptor *inter, struct __go_type_descriptor *typ);
void *runtime_requireitab(struct __go_type_descriptor *inter, struct __go_type_descriptor *typ);
bool runtime_ifaceT2Ip(struct __go_type_descriptor *inter, struct __go_type_descriptor *typ);
void *getitab(const struct __go_type_descriptor *lhs, const struct __go_type_descriptor *rhs, bool canfail);

uintptr_t runtime_interhash(void *p, uintptr_t h);
uintptr_t runtime_nilinterhash(void *p, uintptr_t h);

void *runtime_convT(struct __go_type_descriptor *t, void *v);
void *runtime_convTnoptr(struct __go_type_descriptor *t, void *v);
void *runtime_convT16(uint16_t v);
void *runtime_convT32(uint32_t v);
void *runtime_convT64(uint64_t v);

bool runtime_efaceeq(Eface e1, Eface e2);
bool runtime_efacevaleq(Eface e, struct __go_type_descriptor *typ, void *val);
bool runtime_ifaceeq(Iface i1, Iface i2);
bool runtime_ifacevaleq(Iface i, struct __go_type_descriptor *typ, void *val);
bool runtime_ifaceefaceeq(Iface i, Eface e);
bool runtime_eqtype(struct __go_type_descriptor *t1, struct __go_type_descriptor *t2);

void runtime_printeface(Eface e);
void runtime_printiface(Iface i);

void runtime_panicdottype(struct __go_type_descriptor *have,
                          struct __go_type_descriptor *want,
                          struct __go_type_descriptor *iface);
void runtime_panicstring(const char *msg);

/* String operations */
GoString runtime_gostring(const uint8_t *str);
GoString runtime_gostringn(const uint8_t *str, intptr_t len);
GoString runtime_catstring(GoString s1, GoString s2);
GoString runtime_concatstring(int32_t n, GoString *s);
int32_t runtime_cmpstring(GoString s1, GoString s2);
GoString runtime_slicestring(GoString s, intptr_t lo, intptr_t hi);
void runtime_printstring(GoString s);
intptr_t runtime_findnull(const uint8_t *s);

GoString runtime_slicebytetostring(void *buf, void *ptr, int n);
GoSlice runtime_stringtoslicebyte(void *buf, GoString s);
GoString runtime_intstring(void *buf, int64_t v);
GoString runtime_formatint64(int64_t value, int base);
GoString runtime_formatuint64(uint64_t value, int base);
GoString runtime_formatfloat64(double value, int prec);

int runtime_encoderune(uint8_t *p, int32_t r);
int runtime_decoderune(const uint8_t *s, intptr_t len, int32_t *rune);
intptr_t runtime_countrunes(const uint8_t *s, intptr_t len);

typedef struct { int32_t *array; intptr_t len; intptr_t cap; } RuneSlice;
GoSlice runtime_stringtoslicerune(void *buf, GoString s);
GoString runtime_slicerunetostring(void *buf, RuneSlice a);

void *runtime_rawstring(size_t size);
void *runtime_rawbyteslice(size_t size);
void *runtime_rawruneslice(size_t count);

int runtime_slicecopy(void *toPtr, int toLen, void *fromPtr, int fromLen, uintptr_t elemWidth);
size_t runtime_roundupsize(size_t size);

extern GoString runtime_emptystring;

/* Map operations */
#include "map_dreamcast.h"

/* Type equality functions */
_Bool runtime_f32equal(void *p, void *q) __asm__("_runtime.f32equal..f");
_Bool runtime_f64equal(void *p, void *q) __asm__("_runtime.f64equal..f");
_Bool runtime_c64equal(void *p, void *q) __asm__("_runtime.c64equal..f");
_Bool runtime_c128equal(void *p, void *q) __asm__("_runtime.c128equal..f");

/* Panic functions */
bool runtime_canrecover(uintptr_t frame_addr);
void _runtime_panicdivide(void);
void _runtime_panicmem(void);

/* Print functions */
void runtime_printbool(uint8_t b);
void runtime_printint(int64_t n);
void runtime_printuint(uint64_t n);
void runtime_printfloat(double f);
void runtime_printsp(void);
void runtime_printnl(void);

/* Type registration */
void _runtime_registerTypeDescriptors(void);

/* Error logging */
#include <kos/dbglog.h>
#define LIBGODC_ERROR(fmt, ...) dbglog(DBG_ERROR, "[godc] " fmt, ##__VA_ARGS__)
#define LIBGODC_CRITICAL(fmt, ...) dbglog(DBG_CRITICAL, "[godc] " fmt, ##__VA_ARGS__)

#endif /* GODC_RUNTIME_H */
