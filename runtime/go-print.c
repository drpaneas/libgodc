/* libgodc/runtime/go-print.c - print functions */

#include "runtime.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void runtime_printbool(uint8_t b) __asm__("_runtime.printbool");
void runtime_printbool(uint8_t b) { printf("%s", b ? "true" : "false"); }

void runtime_printint(int64_t n) __asm__("_runtime.printint");
void runtime_printint(int64_t n) { printf("%lld", (long long)n); }

void runtime_printuint(uint64_t n) __asm__("_runtime.printuint");
void runtime_printuint(uint64_t n) { printf("%llu", (unsigned long long)n); }

void runtime_printfloat(double f) __asm__("_runtime.printfloat");
void runtime_printfloat(double f)
{
    if (isnan(f)) { printf("NaN"); return; }
    if (isinf(f)) { printf("%sInf", f > 0 ? "+" : "-"); return; }
    printf("%g", f);
}

typedef struct { double real; double imag; } Complex128;

void runtime_printcomplex(Complex128 c) __asm__("_runtime.printcomplex");
void runtime_printcomplex(Complex128 c)
{
    printf("(");
    runtime_printfloat(c.real);
    if (c.imag >= 0) printf("+");
    runtime_printfloat(c.imag);
    printf("i)");
}

void runtime_printslice(GoSlice s) __asm__("_runtime.printslice");
void runtime_printslice(GoSlice s) { printf("[%d/%d]%p", s.__count, s.__capacity, s.__values); }

void runtime_printpointer(void *p) __asm__("_runtime.printpointer");
void runtime_printpointer(void *p) { printf("%p", p); }

void runtime_printhex(uint64_t v) __asm__("_runtime.printhex");
void runtime_printhex(uint64_t v) { printf("%llx", (unsigned long long)v); }

void runtime_printsp(void) __asm__("_runtime.printsp");
void runtime_printsp(void) { printf(" "); }

void runtime_printnl(void) __asm__("_runtime.printnl");
void runtime_printnl(void) { printf("\n"); }

void runtime_printlock(void) __asm__("_runtime.printlock");
void runtime_printlock(void) {}

void runtime_printunlock(void) __asm__("_runtime.printunlock");
void runtime_printunlock(void) {}

void runtime_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

int32_t runtime_snprintf(uint8_t *buf, int32_t n, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int32_t ret = vsnprintf((char *)buf, n, fmt, args);
    va_end(args);
    return ret;
}
