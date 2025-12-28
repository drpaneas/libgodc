//go:build gccgo

package kos

import "unsafe"

// BFONT dimension constants
const (
	BFONT_THIN_WIDTH = 12
	BFONT_WIDE_WIDTH = BFONT_THIN_WIDTH * 2 // 24
	BFONT_HEIGHT     = 24
)

// BFONT byte size constants (from KOS biosfont.h)
const (
	BFONT_BYTES_PER_CHAR      = (BFONT_THIN_WIDTH * BFONT_HEIGHT / 8)                        // 36 bytes
	BFONT_BYTES_PER_WIDE_CHAR = (BFONT_WIDE_WIDTH * BFONT_HEIGHT / 8)                        // 72 bytes
	BFONT_WIDE_START          = (288 * BFONT_BYTES_PER_CHAR)                                 // 10368
	BFONT_DREAMCAST_SPECIFIC  = (BFONT_WIDE_START + (7056 * BFONT_BYTES_PER_WIDE_CHAR))      // 518400
)

// BFONT encoding constants
const (
	BFONT_CODE_ISO8859_1 int32 = 0
	BFONT_CODE_EUC       int32 = 1
	BFONT_CODE_SJIS      int32 = 2
	BFONT_CODE_RAW       int32 = 3
)

// BFONT Dreamcast button icon byte offsets (calculated via BFONT_DC_ICON macro)
// BFONT_DC_ICON(offset) = BFONT_DREAMCAST_SPECIFIC + (offset * BFONT_BYTES_PER_WIDE_CHAR)
const (
	BFONT_ABUTTON     uint32 = BFONT_DREAMCAST_SPECIFIC + (11 * BFONT_BYTES_PER_WIDE_CHAR) // 519192
	BFONT_BBUTTON     uint32 = BFONT_DREAMCAST_SPECIFIC + (12 * BFONT_BYTES_PER_WIDE_CHAR) // 519264
	BFONT_CBUTTON     uint32 = BFONT_DREAMCAST_SPECIFIC + (13 * BFONT_BYTES_PER_WIDE_CHAR) // 519336
	BFONT_DBUTTON     uint32 = BFONT_DREAMCAST_SPECIFIC + (14 * BFONT_BYTES_PER_WIDE_CHAR) // 519408
	BFONT_XBUTTON     uint32 = BFONT_DREAMCAST_SPECIFIC + (15 * BFONT_BYTES_PER_WIDE_CHAR) // 519480
	BFONT_YBUTTON     uint32 = BFONT_DREAMCAST_SPECIFIC + (16 * BFONT_BYTES_PER_WIDE_CHAR) // 519552
	BFONT_ZBUTTON     uint32 = BFONT_DREAMCAST_SPECIFIC + (17 * BFONT_BYTES_PER_WIDE_CHAR) // 519624
	BFONT_LTRIGGER    uint32 = BFONT_DREAMCAST_SPECIFIC + (18 * BFONT_BYTES_PER_WIDE_CHAR) // 519696
	BFONT_RTRIGGER    uint32 = BFONT_DREAMCAST_SPECIFIC + (19 * BFONT_BYTES_PER_WIDE_CHAR) // 519768
	BFONT_STARTBUTTON uint32 = BFONT_DREAMCAST_SPECIFIC + (20 * BFONT_BYTES_PER_WIDE_CHAR) // 519840
	BFONT_VMUICON     uint32 = BFONT_DREAMCAST_SPECIFIC + (21 * BFONT_BYTES_PER_WIDE_CHAR) // 519912
)

//extern bfont_set_encoding
func BfontSetEncoding(enc int32)

//extern bfont_draw_str
func bfontDrawStr(buffer unsafe.Pointer, width uint32, opaque int32, str uintptr)

func BfontDrawStr(buffer unsafe.Pointer, width uint32, opaque bool, str string) {
	cstr := make([]byte, len(str)+1)
	copy(cstr, str)
	op := int32(0)
	if opaque {
		op = 1
	}
	bfontDrawStr(buffer, width, op, uintptr(unsafe.Pointer(&cstr[0])))
}

//extern bfont_draw_wide
func bfontDrawWide(buffer unsafe.Pointer, bufwidth uint32, opaque int32, c uint32) uint32

func BfontDrawWide(buffer unsafe.Pointer, bufwidth uint32, opaque bool, c uint32) uint32 {
	op := int32(0)
	if opaque {
		op = 1
	}
	return bfontDrawWide(buffer, bufwidth, op, c)
}

//extern bfont_set_foreground_color
func BfontSetForegroundColor(c uint32) uint32

//extern bfont_set_background_color
func BfontSetBackgroundColor(c uint32) uint32
