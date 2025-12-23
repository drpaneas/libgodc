/* libgodc/runtime/go-panic.c - panic helper functions */

#include "runtime.h"
#include "panic_dreamcast.h"
#include <stdio.h>
#include <stdlib.h>
#include <arch/arch.h>

#define UNUSED(x) ((void)(x))

void _runtime_panicmem_impl(void) __asm__("_runtime.panicmem");
void _runtime_panicmem_impl(void) { runtime_panicstring("nil pointer dereference"); }
void _runtime_panicmem(void) { _runtime_panicmem_impl(); }

void runtime_panicdivide(void) __asm__("_runtime.panicdivide");
void runtime_panicdivide(void) { runtime_panicstring("divide by zero"); }

void runtime_panicindex(void) { runtime_panicstring("index out of range"); }

void runtime_goPanicIndex(int idx, int len) __asm__("_runtime.goPanicIndex");
void runtime_goPanicIndex(int idx, int len) { UNUSED(idx); UNUSED(len); runtime_panicstring("index out of range"); }

void runtime_goPanicIndexU(uint32_t idx, int len) __asm__("_runtime.goPanicIndexU");
void runtime_goPanicIndexU(uint32_t idx, int len) { UNUSED(idx); UNUSED(len); runtime_panicstring("index out of range"); }

void runtime_panicslice(void) { runtime_panicstring("slice bounds out of range"); }

void runtime_goPanicSlice(void) __asm__("_runtime.goPanicSlice");
void runtime_goPanicSlice(void) { runtime_panicstring("slice bounds out of range"); }

void runtime_goPanicSliceAlen(void) __asm__("_runtime.goPanicSliceAlen.1");
void runtime_goPanicSliceAlen(void) { runtime_panicstring("slice bounds out of range"); }

void runtime_goPanicSliceAcap(void) __asm__("_runtime.goPanicSliceAcap.1");
void runtime_goPanicSliceAcap(void) { runtime_panicstring("slice bounds out of range"); }

void runtime_goPanicSliceB(void) __asm__("_runtime.goPanicSliceB.1");
void runtime_goPanicSliceB(void) { runtime_panicstring("slice bounds out of range"); }

void runtime_panicnilcompare(void) { runtime_panicstring("comparing uncomparable type"); }

bool runtime_canpanic(G *gp) { UNUSED(gp); return true; }

void runtime_goPanicSliceAlen_int(int x, int y) __asm__("_runtime.goPanicSliceAlen");
void runtime_goPanicSliceAlen_int(int x, int y) { UNUSED(x); UNUSED(y); runtime_panicstring("slice bounds out of range"); }

void runtime_goPanicSliceAcap_int(int x, int y) __asm__("_runtime.goPanicSliceAcap");
void runtime_goPanicSliceAcap_int(int x, int y) { UNUSED(x); UNUSED(y); runtime_panicstring("slice bounds out of range"); }

void runtime_goPanicSliceB_int(int x, int y) __asm__("_runtime.goPanicSliceB");
void runtime_goPanicSliceB_int(int x, int y) { UNUSED(x); UNUSED(y); runtime_panicstring("slice bounds out of range"); }

void runtime_goPanicSliceAlenU(uint32_t x, int y) __asm__("_runtime.goPanicSliceAlenU");
void runtime_goPanicSliceAlenU(uint32_t x, int y) { UNUSED(x); UNUSED(y); runtime_panicstring("slice bounds out of range"); }

void runtime_goPanicSliceAcapU(uint32_t x, int y) __asm__("_runtime.goPanicSliceAcapU");
void runtime_goPanicSliceAcapU(uint32_t x, int y) { UNUSED(x); UNUSED(y); runtime_panicstring("slice bounds out of range"); }

void runtime_goPanicSliceBU(uint32_t x, int y) __asm__("_runtime.goPanicSliceBU");
void runtime_goPanicSliceBU(uint32_t x, int y) { UNUSED(x); UNUSED(y); runtime_panicstring("slice bounds out of range"); }

void runtime_panicshift(void) __asm__("_runtime.panicshift");
void runtime_panicshift(void) { runtime_panicstring("negative shift"); }

void runtime_panicmakeslicelen(void) __asm__("_runtime.panicmakeslicelen");
void runtime_panicmakeslicelen(void) { runtime_panicstring("makeslice: len out of range"); }

void runtime_panicmakeslicecap(void) __asm__("_runtime.panicmakeslicecap");
void runtime_panicmakeslicecap(void) { runtime_panicstring("makeslice: cap out of range"); }

void runtime_panicgonil(void) __asm__("_runtime.panicgonil");
void runtime_panicgonil(void) { runtime_panicstring("go of nil func"); }

void runtime_goPanicSlice3Alen(int x, int y) __asm__("_runtime.goPanicSlice3Alen");
void runtime_goPanicSlice3Alen(int x, int y) { UNUSED(x); UNUSED(y); runtime_panicstring("slice3 bounds"); }

void runtime_goPanicSlice3Acap(int x, int y) __asm__("_runtime.goPanicSlice3Acap");
void runtime_goPanicSlice3Acap(int x, int y) { UNUSED(x); UNUSED(y); runtime_panicstring("slice3 bounds"); }

void runtime_goPanicSlice3B(int x, int y) __asm__("_runtime.goPanicSlice3B");
void runtime_goPanicSlice3B(int x, int y) { UNUSED(x); UNUSED(y); runtime_panicstring("slice3 bounds"); }

void runtime_goPanicSlice3C(int x, int y) __asm__("_runtime.goPanicSlice3C");
void runtime_goPanicSlice3C(int x, int y) { UNUSED(x); UNUSED(y); runtime_panicstring("slice3 bounds"); }

void runtime_goPanicSlice3AlenU(uint32_t x, int y) __asm__("_runtime.goPanicSlice3AlenU");
void runtime_goPanicSlice3AlenU(uint32_t x, int y) { UNUSED(x); UNUSED(y); runtime_panicstring("slice3 bounds"); }

void runtime_goPanicSlice3AcapU(uint32_t x, int y) __asm__("_runtime.goPanicSlice3AcapU");
void runtime_goPanicSlice3AcapU(uint32_t x, int y) { UNUSED(x); UNUSED(y); runtime_panicstring("slice3 bounds"); }

void runtime_goPanicSlice3BU(uint32_t x, int y) __asm__("_runtime.goPanicSlice3BU");
void runtime_goPanicSlice3BU(uint32_t x, int y) { UNUSED(x); UNUSED(y); runtime_panicstring("slice3 bounds"); }

void runtime_goPanicSlice3CU(uint32_t x, int y) __asm__("_runtime.goPanicSlice3CU");
void runtime_goPanicSlice3CU(uint32_t x, int y) { UNUSED(x); UNUSED(y); runtime_panicstring("slice3 bounds"); }

#define EXTEND_PANIC(name, msg) \
    void runtime_##name(int64_t idx, int len) __asm__("_runtime." #name); \
    void runtime_##name(int64_t idx, int len) { UNUSED(idx); UNUSED(len); runtime_panicstring(msg); }

#define EXTEND_PANIC_U(name, msg) \
    void runtime_##name(uint64_t idx, int len) __asm__("_runtime." #name); \
    void runtime_##name(uint64_t idx, int len) { UNUSED(idx); UNUSED(len); runtime_panicstring(msg); }

EXTEND_PANIC(goPanicExtendIndex, "index out of range")
EXTEND_PANIC_U(goPanicExtendIndexU, "index out of range")
EXTEND_PANIC(goPanicExtendSliceAlen, "slice bounds")
EXTEND_PANIC_U(goPanicExtendSliceAlenU, "slice bounds")
EXTEND_PANIC(goPanicExtendSliceAcap, "slice bounds")
EXTEND_PANIC_U(goPanicExtendSliceAcapU, "slice bounds")
EXTEND_PANIC(goPanicExtendSliceB, "slice bounds")
EXTEND_PANIC_U(goPanicExtendSliceBU, "slice bounds")
EXTEND_PANIC(goPanicExtendSlice3Alen, "slice bounds")
EXTEND_PANIC_U(goPanicExtendSlice3AlenU, "slice bounds")
EXTEND_PANIC(goPanicExtendSlice3Acap, "slice bounds")
EXTEND_PANIC_U(goPanicExtendSlice3AcapU, "slice bounds")
EXTEND_PANIC(goPanicExtendSlice3B, "slice bounds")
EXTEND_PANIC_U(goPanicExtendSlice3BU, "slice bounds")
EXTEND_PANIC(goPanicExtendSlice3C, "slice bounds")
EXTEND_PANIC_U(goPanicExtendSlice3CU, "slice bounds")

#undef EXTEND_PANIC
#undef EXTEND_PANIC_U

void runtime_goPanicSliceConvert(int len, int cap) __asm__("_runtime.goPanicSliceConvert");
void runtime_goPanicSliceConvert(int len, int cap) { UNUSED(len); UNUSED(cap); runtime_panicstring("slice convert"); }
