#include "runtime.h"
#include "gc_semispace.h"
#include <string.h>
#include <stdio.h>
#include <float.h>
#include <math.h>

// Empty string constant
GoString runtime_emptystring = {NULL, 0};

/* UTF-8 constants */
#define RUNE_ERROR 0xFFFD // Unicode replacement character
#define RUNE_SELF 0x80    // Below this: ASCII (1 byte)
#define RUNE_MAX 0x10FFFF // Maximum valid Unicode code point

// UTF-8 encoding masks
#define T1 0x00 // 0000 0000
#define TX 0x80 // 1000 0000
#define T2 0xC0 // 1100 0000
#define T3 0xE0 // 1110 0000
#define T4 0xF0 // 1111 0000
#define T5 0xF8 // 1111 1000

#define MASKX 0x3F // 0011 1111
#define MASK2 0x1F // 0001 1111
#define MASK3 0x0F // 0000 1111
#define MASK4 0x07 // 0000 0111

// Surrogate range (invalid for UTF-8)
#define SURROGATE_MIN 0xD800
#define SURROGATE_MAX 0xDFFF

// Encode a rune to UTF-8. Returns bytes written (1-4), or 0 on error.
// Buffer p must have at least 4 bytes available.
int runtime_encoderune(uint8_t *p, int32_t r)
{
    // Invalid rune
    if (r < 0 || r > RUNE_MAX || (r >= SURROGATE_MIN && r <= SURROGATE_MAX))
    {
        r = RUNE_ERROR;
    }

    // ASCII fast path
    if (r <= 0x7F)
    {
        p[0] = (uint8_t)r;
        return 1;
    }

    // 2-byte sequence
    if (r <= 0x7FF)
    {
        p[0] = T2 | (uint8_t)(r >> 6);
        p[1] = TX | (uint8_t)(r & MASKX);
        return 2;
    }

    // 3-byte sequence
    if (r <= 0xFFFF)
    {
        p[0] = T3 | (uint8_t)(r >> 12);
        p[1] = TX | (uint8_t)((r >> 6) & MASKX);
        p[2] = TX | (uint8_t)(r & MASKX);
        return 3;
    }

    // 4-byte sequence
    p[0] = T4 | (uint8_t)(r >> 18);
    p[1] = TX | (uint8_t)((r >> 12) & MASKX);
    p[2] = TX | (uint8_t)((r >> 6) & MASKX);
    p[3] = TX | (uint8_t)(r & MASKX);
    return 4;
}

// Decode a rune from UTF-8. Returns rune and bytes consumed.
// On error, returns RUNE_ERROR and consumes 1 byte.
// This is the internal helper function.
int runtime_decoderune_internal(const uint8_t *s, intptr_t len, int32_t *rune)
{
    if (len == 0)
    {
        *rune = RUNE_ERROR;
        return 0;
    }

    uint8_t c0 = s[0];

    // ASCII fast path (most common case)
    if (c0 < RUNE_SELF)
    {
        *rune = c0;
        return 1;
    }

    // Error cases for invalid lead bytes
    if (c0 < T2)
    {
        *rune = RUNE_ERROR;
        return 1;
    }

    // 2-byte sequence
    if (c0 < T3)
    {
        if (len < 2)
        {
            *rune = RUNE_ERROR;
            return 1;
        }
        uint8_t c1 = s[1];
        if ((c1 & 0xC0) != TX)
        {
            *rune = RUNE_ERROR;
            return 1;
        }
        int32_t r = ((int32_t)(c0 & MASK2) << 6) | (int32_t)(c1 & MASKX);
        if (r < 0x80)
        {
            *rune = RUNE_ERROR; // Overlong encoding
            return 1;
        }
        *rune = r;
        return 2;
    }

    // 3-byte sequence
    if (c0 < T4)
    {
        if (len < 3)
        {
            *rune = RUNE_ERROR;
            return 1;
        }
        uint8_t c1 = s[1];
        uint8_t c2 = s[2];
        if ((c1 & 0xC0) != TX || (c2 & 0xC0) != TX)
        {
            *rune = RUNE_ERROR;
            return 1;
        }
        int32_t r = ((int32_t)(c0 & MASK3) << 12) |
                    ((int32_t)(c1 & MASKX) << 6) |
                    (int32_t)(c2 & MASKX);
        if (r < 0x800 || (r >= SURROGATE_MIN && r <= SURROGATE_MAX))
        {
            *rune = RUNE_ERROR; // Overlong or surrogate
            return 1;
        }
        *rune = r;
        return 3;
    }

    // 4-byte sequence
    if (c0 < T5)
    {
        if (len < 4)
        {
            *rune = RUNE_ERROR;
            return 1;
        }
        uint8_t c1 = s[1];
        uint8_t c2 = s[2];
        uint8_t c3 = s[3];
        if ((c1 & 0xC0) != TX || (c2 & 0xC0) != TX || (c3 & 0xC0) != TX)
        {
            *rune = RUNE_ERROR;
            return 1;
        }
        int32_t r = ((int32_t)(c0 & MASK4) << 18) |
                    ((int32_t)(c1 & MASKX) << 12) |
                    ((int32_t)(c2 & MASKX) << 6) |
                    (int32_t)(c3 & MASKX);
        if (r < 0x10000 || r > RUNE_MAX)
        {
            *rune = RUNE_ERROR; // Overlong or out of range
            return 1;
        }
        *rune = r;
        return 4;
    }

    // Invalid lead byte
    *rune = RUNE_ERROR;
    return 1;
}

// Legacy wrapper for internal use - calls internal helper
int runtime_decoderune(const uint8_t *s, intptr_t len, int32_t *rune)
{
    return runtime_decoderune_internal(s, len, rune);
}

// Result struct for decoderune - returns (rune, width)
typedef struct
{
    int32_t rune;
    int32_t width;
} DecodeRuneResult;

// gccgo signature: func decoderune(s string, k int) (r rune, width int)
// Takes GoString and index, returns rune and width
DecodeRuneResult runtime_decoderune_gccgo(GoString s, int32_t k) __asm__("_runtime.decoderune");
DecodeRuneResult runtime_decoderune_gccgo(GoString s, int32_t k)
{
    DecodeRuneResult result;
    result.rune = RUNE_ERROR;
    result.width = 1;

    if (s.str == NULL || k < 0 || k >= s.len)
    {
        return result;
    }

    result.width = runtime_decoderune_internal(s.str + k, s.len - k, &result.rune);
    return result;
}

// Count runes in a UTF-8 string
intptr_t runtime_countrunes(const uint8_t *s, intptr_t len)
{
    intptr_t count = 0;
    intptr_t i = 0;
    int32_t rune;

    while (i < len)
    {
        int n = runtime_decoderune_internal(s + i, len - i, &rune);
        i += n;
        count++;
    }

    return count;
}

/* tmpBuf for avoiding small allocations (escape analysis optimization) */
#define TMP_BUF_SIZE 32

typedef struct
{
    uint8_t data[TMP_BUF_SIZE];
} tmpBuf;

// Find length of null-terminated C string
intptr_t runtime_findnull(const uint8_t *s)
{
    intptr_t l;

    if (s == NULL)
        return 0;

    for (l = 0; s[l] != 0; l++)
        ;

    return l;
}

// Helper: allocate string of given size
// Strings are immutable, so we allocate them on the heap
// and include space for a null terminator for C compatibility
static GoString gostringsize(intptr_t len)
{
    GoString s;

    if (len < 0)
    {
        return runtime_emptystring;
    }

    if (len > 10000000)
    {
        return runtime_emptystring;
    }

    if (len == 0)
        return runtime_emptystring;

    // Allocate memory for string + null terminator
    // Strings don't contain pointers
    s.str = (uint8_t *)gc_alloc(len + 1, NULL);
    if (s.str == NULL)
    {
        s.len = 0;
        return s;
    }

    s.len = len;
    ((uint8_t *)s.str)[len] = 0; // Null terminator

    return s;
}

// Convert C string to Go string
GoString runtime_gostring(const uint8_t *str)
{
    intptr_t len;
    GoString s;

    len = runtime_findnull(str);
    s = gostringsize(len);
    if (len > 0 && s.str != NULL)
        memcpy((void *)s.str, str, len);

    return s;
}

// Convert C string with known length to Go string
GoString runtime_gostringn(const uint8_t *str, intptr_t len)
{
    GoString s;

    s = gostringsize(len);
    if (len > 0 && s.str != NULL)
        memcpy((void *)s.str, str, len);

    return s;
}

// Concatenate two strings
GoString runtime_catstring(GoString s1, GoString s2)
{
    GoString s3;

    // Optimizations for empty strings
    if (s1.len == 0)
        return s2;
    if (s2.len == 0)
        return s1;

    // Allocate new string
    s3 = gostringsize(s1.len + s2.len);
    if (s3.str == NULL)
    {
        s3.len = 0;
        return s3;
    }

    // Copy both strings
    memcpy((void *)s3.str, s1.str, s1.len);
    memcpy((void *)(s3.str + s1.len), s2.str, s2.len);

    return s3;
}

// Concatenate multiple strings
GoString runtime_concatstring(int32_t n, GoString *s)
{
    intptr_t i, total_len;
    GoString out;
    uint8_t *p;

    // Calculate total length
    total_len = 0;
    for (i = 0; i < n; i++)
    {
        // Check for overflow
        if (total_len + s[i].len < total_len)
        {
            return runtime_emptystring;
        }
        total_len += s[i].len;
    }

    // Allocate result
    out = gostringsize(total_len);
    if (out.str == NULL)
    {
        out.len = 0;
        return out;
    }

    // Copy all strings
    p = (uint8_t *)out.str;
    for (i = 0; i < n; i++)
    {
        if (s[i].len > 0 && s[i].str != NULL)
        {
            memcpy(p, s[i].str, s[i].len);
            p += s[i].len;
        }
    }

    return out;
}

// Compare two strings
// Returns: -1 if s1 < s2, 0 if s1 == s2, 1 if s1 > s2
int32_t runtime_cmpstring(GoString s1, GoString s2) __asm__("_runtime.cmpstring");
int32_t runtime_cmpstring(GoString s1, GoString s2)
{
    intptr_t i, minlen;
    uint8_t c1, c2;

    minlen = s1.len;
    if (s2.len < minlen)
        minlen = s2.len;

    // Compare byte by byte
    for (i = 0; i < minlen; i++)
    {
        c1 = s1.str[i];
        c2 = s2.str[i];
        if (c1 < c2)
            return -1;
        if (c1 > c2)
            return 1;
    }

    // All bytes equal, compare lengths
    if (s1.len < s2.len)
        return -1;
    if (s1.len > s2.len)
        return 1;

    return 0;
}

// Slice a string s[lo:hi]
GoString runtime_slicestring(GoString s, intptr_t lo, intptr_t hi)
{
    GoString result;

    // Bounds check
    if (lo < 0 || lo > s.len || hi < lo || hi > s.len)
    {
        return runtime_emptystring;
    }

    // Create slice (no allocation needed, just pointer and length)
    result.str = s.str + lo;
    result.len = hi - lo;

    return result;
}

// Print a Go string
void runtime_printstring(GoString s) __asm__("_runtime.printstring");
void runtime_printstring(GoString s)
{
    intptr_t i;

    for (i = 0; i < s.len; i++)
    {
        printf("%c", s.str[i]);
    }
}

// Convert byte slice to string
// gccgo passes 3 separate parameters, not a struct!
// Signature: slicebytetostring(buf *tmpBuf, ptr *byte, n int) string
//
// tmpBuf optimization: The compiler passes a stack-allocated buffer for strings
// that don't escape (determined by escape analysis). If buf != NULL and the
// string fits, we use the stack buffer to avoid heap allocation.
// This is particularly effective for short-lived strings in comparisons,
// concatenations, and format operations.
GoString runtime_slicebytetostring(void *buf, void *ptr, int n) __asm__("_runtime.slicebytetostring");
GoString runtime_slicebytetostring(void *buf, void *ptr, int n)
{
    GoString s;


    // Validate length
    if (n < 0)
    {
        LIBGODC_ERROR("slicebytetostring: negative length %d", n);
        return runtime_emptystring;
    }

    if (n > 10000000)
    {
        LIBGODC_ERROR("slicebytetostring: suspiciously large length %d (0x%x)", n, (unsigned int)n);
        return runtime_emptystring;
    }

    if (n == 0)
    {
        return runtime_emptystring;
    }

    const uint8_t *src = (const uint8_t *)ptr;
    uint8_t *dst;

    // Use stack buffer if provided and size fits (avoids heap allocation)
    // The compiler's escape analysis ensures this is only used when safe
    if (buf != NULL && n <= TMP_BUF_SIZE)
    {
        dst = (uint8_t *)buf;
    }
    else
    {
        // Heap allocate - includes null terminator for C compatibility
        s = gostringsize(n);
        if (s.str == NULL)
        {
            return runtime_emptystring;
        }
        dst = (uint8_t *)s.str;
    }

    // Copy data
    if (src != NULL)
    {
        memcpy(dst, src, n);
    }

    // If we used the stack buffer, construct the string struct manually
    if (buf != NULL && n <= TMP_BUF_SIZE)
    {
        s.str = dst;
        s.len = n;
    }

    return s;
}

// Convert string to byte slice - internal helper
static GoSlice stringtoslicebyte_internal(void *buf, const uint8_t *str, intptr_t len)
{
    GoSlice b;


    if (len <= 0 || str == NULL)
    {
        b.__values = NULL;
        b.__count = 0;
        b.__capacity = 0;
        return b;
    }

    // tmpBuf optimization: use stack buffer if provided and size fits
    if (buf != NULL && len <= TMP_BUF_SIZE)
    {
        b.__values = buf;
    }
    else
    {
        // Allocate new byte array (strings are immutable, slices are not)
        b.__values = gc_alloc(len, NULL);
        if (b.__values == NULL)
        {
            b.__count = 0;
            b.__capacity = 0;
            return b;
        }
    }

    b.__count = len;
    b.__capacity = len;

    memcpy(b.__values, str, len);

    return b;
}

// gccgo signature: stringtoslicebyte(buf *tmpBuf, s string) []byte
// Takes GoString directly as a struct
GoSlice runtime_stringtoslicebyte(void *buf, GoString s) __asm__("_runtime.stringtoslicebyte");
GoSlice runtime_stringtoslicebyte(void *buf, GoString s)
{
    return stringtoslicebyte_internal(buf, s.str, s.len);
}

// Runtime wrappers for gccgo compatibility

// String comparison for equality
bool __go_strings_equal(GoString s1, GoString s2)
{
    return runtime_cmpstring(s1, s2) == 0;
}

// String concatenation (2 strings)
GoString __go_string_plus(GoString s1, GoString s2)
{
    return runtime_catstring(s1, s2);
}

// Convert a single rune to a UTF-8 string
// This is called for string(rune) conversions
// gccgo signature: intstring(buf *[4]byte, v int64) string
// buf is a 4-byte buffer for the UTF-8 result (max UTF-8 rune size)
GoString runtime_intstring(void *buf, int64_t v) __asm__("_runtime.intstring");
GoString runtime_intstring(void *buf, int64_t v)
{
    GoString s;
    uint8_t tmpbuf[4];
    int len;

    // Validate rune
    int32_t r = (int32_t)v;
    if (r < 0 || r > RUNE_MAX || (r >= SURROGATE_MIN && r <= SURROGATE_MAX))
    {
        r = RUNE_ERROR;
    }

    // Encode to UTF-8 into temp buffer first
    len = runtime_encoderune(tmpbuf, r);

    uint8_t *dst;

    // Use provided buffer if available (optimization for non-escaping strings)
    if (buf != NULL)
    {
        dst = (uint8_t *)buf;
        memcpy(dst, tmpbuf, len);
        s.str = dst;
        s.len = len;
    }
    else
    {
        // Heap allocate
        dst = (uint8_t *)gc_alloc(len + 1, NULL);
        if (dst == NULL)
        {
            return runtime_emptystring;
        }
        memcpy(dst, tmpbuf, len);
        dst[len] = 0; // Null terminator for C compatibility
        s.str = dst;
        s.len = len;
    }

    return s;
}

// Helper function to convert int64 to string in given base (called from strconv)
GoString runtime_formatint64(int64_t value, int base)
{
    char buf[65]; // 64 bits in base 2 + sign + null
    int pos = 64;
    int negative = 0;
    GoString s;

    // Handle INT64_MIN specially to avoid undefined behavior
    // INT64_MIN = -9223372036854775808, which can't be negated in int64
    if (value == INT64_MIN)
    {
        // Use the unsigned version for INT64_MIN
        return runtime_gostringn((const uint8_t *)"-9223372036854775808", 20);
    }

    // Handle negative
    if (value < 0)
    {
        negative = 1;
        value = -value;
    }

    // Handle zero
    if (value == 0)
    {
        buf[pos--] = '0';
    }
    else
    {
        // Convert digits (they come out in reverse)
        while (value > 0)
        {
            int digit = value % base;
            if (digit < 10)
            {
                buf[pos--] = '0' + digit;
            }
            else
            {
                buf[pos--] = 'a' + (digit - 10);
            }
            value /= base;
        }
    }

    // Add negative sign
    if (negative)
    {
        buf[pos--] = '-';
    }

    // Calculate length
    int len = 64 - pos;

    // Create string
    s.str = (uint8_t *)gc_alloc(len + 1, NULL);
    if (s.str == NULL)
    {
        s.len = 0;
        return s;
    }

    // Copy characters
    memcpy((void *)s.str, &buf[pos + 1], len);
    ((uint8_t *)s.str)[len] = 0;
    s.len = len;

    return s;
}

// Helper function to convert uint64 to string in given base
GoString runtime_formatuint64(uint64_t value, int base)
{
    char buf[65];
    int pos = 64;
    GoString s;

    // Handle zero
    if (value == 0)
    {
        buf[pos--] = '0';
    }
    else
    {
        // Convert digits (they come out in reverse)
        while (value > 0)
        {
            int digit = value % base;
            if (digit < 10)
            {
                buf[pos--] = '0' + digit;
            }
            else
            {
                buf[pos--] = 'a' + (digit - 10);
            }
            value /= base;
        }
    }

    // Calculate length
    int len = 64 - pos;

    // Create string
    s.str = (uint8_t *)gc_alloc(len + 1, NULL);
    if (s.str == NULL)
    {
        s.len = 0;
        return s;
    }

    // Copy characters
    memcpy((void *)s.str, &buf[pos + 1], len);
    ((uint8_t *)s.str)[len] = 0;
    s.len = len;

    return s;
}

// Wrapper for gccgo's runtime.concatstrings (note the 's' at the end)
// This is called when multiple strings are concatenated with +
// The function signature expected by gccgo is:
// func runtime.concatstrings(buf *tmpBuf, a []string) string
// But for simplicity, we'll use a variadic wrapper
typedef struct
{
    GoString *strings;
    intptr_t len;
    intptr_t cap;
} GoStringSlice;

GoString runtime_concatstrings(void *buf, GoStringSlice strings) __asm__("_runtime.concatstrings");
GoString runtime_concatstrings(void *buf, GoStringSlice strings)
{
    // Handle empty or single string cases
    if (strings.len == 0)
        return runtime_emptystring;
    if (strings.len == 1)
        return strings.strings[0];

    // Calculate total length
    intptr_t total_len = 0;
    for (intptr_t i = 0; i < strings.len; i++)
    {
        // Check for overflow
        if (total_len + strings.strings[i].len < total_len)
        {
            return runtime_emptystring;
        }
        total_len += strings.strings[i].len;
    }

    if (total_len == 0)
        return runtime_emptystring;

    GoString out;
    uint8_t *dst;

    // tmpBuf optimization: use stack buffer if provided and size fits
    // Avoids heap allocation for short concatenations (very common case)
    if (buf != NULL && total_len <= TMP_BUF_SIZE)
    {
        dst = (uint8_t *)buf;
        out.str = dst;
        out.len = total_len;
    }
    else
    {
        // Heap allocate
        out = gostringsize(total_len);
        if (out.str == NULL)
        {
            return runtime_emptystring;
        }
        dst = (uint8_t *)out.str;
    }

    // Copy all strings
    uint8_t *p = dst;
    for (intptr_t i = 0; i < strings.len; i++)
    {
        if (strings.strings[i].len > 0 && strings.strings[i].str != NULL)
        {
            memcpy(p, strings.strings[i].str, strings.strings[i].len);
            p += strings.strings[i].len;
        }
    }

    return out;
}

// Format float64 to string using C's sprintf
// This avoids the string slicing bugs in Go implementations
GoString runtime_formatfloat64(double value, int prec)
{
    char buf[64];
    int len;

    // Handle special values
    if (isnan(value))
    {
        return runtime_gostringn((const uint8_t *)"NaN", 3);
    }
    if (isinf(value))
    {
        if (value > 0)
            return runtime_gostringn((const uint8_t *)"+Inf", 4);
        else
            return runtime_gostringn((const uint8_t *)"-Inf", 4);
    }

    // Format using sprintf
    if (prec < 0)
    {
        len = snprintf(buf, sizeof(buf), "%.6f", value);
    }
    else if (prec < 20)
    {
        len = snprintf(buf, sizeof(buf), "%.*f", prec, value);
    }
    else
    {
        len = snprintf(buf, sizeof(buf), "%.6f", value);
    }

    if (len < 0 || len >= (int)sizeof(buf))
    {
        return runtime_emptystring;
    }

    return runtime_gostringn((const uint8_t *)buf, len);
}

// Print helpers (already defined in runtime_stubs.c, removed to avoid duplicate)
// void runtime_printsp(void) { printf(" "); }
// void runtime_printnl(void) { printf("\n"); }

/* Raw allocations for string/slice backing arrays (NOSCAN) */

// Allocate raw string backing array
void *runtime_rawstring(size_t size)
{
    if (size == 0)
        return NULL;
    return gc_alloc(size, NULL); // NULL type = NOSCAN
}

// Allocate raw byte slice backing array (NOSCAN)
void *runtime_rawbyteslice(size_t size)
{
    if (size == 0)
        return NULL;
    return gc_alloc(size, NULL); // NULL type = NOSCAN
}

// Allocate raw rune slice backing array (NOSCAN - rune is int32, no pointers)
void *runtime_rawruneslice(size_t count)
{
    if (count == 0)
        return NULL;
    return gc_alloc(count * sizeof(int32_t), NULL); // NULL type = NOSCAN
}

// copy(dst, src) builtin - returns number of elements copied
// Works for any slice type
int runtime_slicecopy(void *toPtr, int toLen, void *fromPtr, int fromLen, uintptr_t elemWidth)
{
    if (fromLen == 0 || toLen == 0)
        return 0;
    if (toPtr == NULL || fromPtr == NULL)
        return 0;

    // Copy min(toLen, fromLen) elements
    int n = (toLen < fromLen) ? toLen : fromLen;

    // Zero-size elements - just return count
    if (elemWidth == 0)
        return n;

    size_t size = (size_t)n * elemWidth;

    // Use memmove to handle overlapping regions safely
    memmove(toPtr, fromPtr, size);

    return n;
}

// Assembly symbol for gccgo
int _runtime_slicecopy(void *to, int tolen, void *from, int fromlen, uintptr_t w)
    __attribute__((alias("runtime_slicecopy")));

// Convert string to []rune - internal helper
static GoSlice stringtoslicerune_internal(void *buf, const uint8_t *str, intptr_t len)
{
    GoSlice result;

    if (len <= 0 || str == NULL)
    {
        result.__values = NULL;
        result.__count = 0;
        result.__capacity = 0;
        return result;
    }

    // Count runes first
    intptr_t runeCount = runtime_countrunes(str, len);

    // Allocate rune array (tmpBuf is only 4 runes for stringtoslicerune)
    // We always heap allocate since the buf is small and we need capacity
    int32_t *runes = (int32_t *)runtime_rawruneslice(runeCount);
    if (runes == NULL)
    {
        result.__values = NULL;
        result.__count = 0;
        result.__capacity = 0;
        return result;
    }

    // Decode UTF-8 to runes
    intptr_t i = 0;
    intptr_t ri = 0;
    while (i < len && ri < runeCount)
    {
        int32_t r;
        int n = runtime_decoderune_internal(str + i, len - i, &r);
        runes[ri++] = r;
        i += n;
    }

    result.__values = runes;
    result.__count = (int)runeCount;
    result.__capacity = (int)runeCount;

    // Silence unused parameter warning
    (void)buf;

    return result;
}

// gccgo signature: stringtoslicerune(buf *[tmpStringBufSize/4]rune, s string) []rune
// Takes GoString directly as a struct
GoSlice runtime_stringtoslicerune(void *buf, GoString s) __asm__("_runtime.stringtoslicerune");
GoSlice runtime_stringtoslicerune(void *buf, GoString s)
{
    return stringtoslicerune_internal(buf, s.str, s.len);
}

// Convert []rune to string - internal helper
static GoString slicerunetostring_internal(void *buf, int32_t *runes, intptr_t len)
{
    if (len <= 0 || runes == NULL)
    {
        return runtime_emptystring;
    }

    // Calculate UTF-8 byte length
    size_t byteLen = 0;
    for (intptr_t i = 0; i < len; i++)
    {
        int32_t r = runes[i];
        if (r < 0 || r > RUNE_MAX || (r >= SURROGATE_MIN && r <= SURROGATE_MAX))
        {
            byteLen += 3; // RUNE_ERROR is 3 bytes in UTF-8
        }
        else if (r <= 0x7F)
        {
            byteLen += 1;
        }
        else if (r <= 0x7FF)
        {
            byteLen += 2;
        }
        else if (r <= 0xFFFF)
        {
            byteLen += 3;
        }
        else
        {
            byteLen += 4;
        }
    }

    GoString s;
    uint8_t *dst;

    // tmpBuf optimization: use stack buffer if provided and size fits
    if (buf != NULL && byteLen <= TMP_BUF_SIZE)
    {
        dst = (uint8_t *)buf;
        s.str = dst;
        s.len = byteLen;
    }
    else
    {
        // Heap allocate
        dst = (uint8_t *)runtime_rawstring(byteLen + 1);
        if (dst == NULL)
        {
            return runtime_emptystring;
        }
        s.str = dst;
        s.len = byteLen;
    }

    // Encode runes to UTF-8
    uint8_t *p = dst;
    for (intptr_t i = 0; i < len; i++)
    {
        int n = runtime_encoderune(p, runes[i]);
        p += n;
    }

    // Null terminator only for heap-allocated strings (for C compatibility)
    if (buf == NULL || byteLen > TMP_BUF_SIZE)
    {
        *p = 0;
    }

    return s;
}

// gccgo signature: slicerunetostring(buf *tmpBuf, a []rune) string
// Takes RuneSlice struct directly (RuneSlice defined in runtime.h)
GoString runtime_slicerunetostring(void *buf, RuneSlice a) __asm__("_runtime.slicerunetostring");
GoString runtime_slicerunetostring(void *buf, RuneSlice a)
{
    return slicerunetostring_internal(buf, a.array, a.len);
}

// Legacy aliases removed - gccgo-compatible wrappers defined above with __asm__ names

// Round size up to allocator class boundary (8-byte alignment minimum)
size_t runtime_roundupsize(size_t size)
{
    if (size == 0)
        return 0;

    // Match GC_ALIGN (8 bytes)
    return (size + 7) & ~(size_t)7;
}
