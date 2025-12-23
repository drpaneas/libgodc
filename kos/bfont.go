//go:build gccgo

package kos

import "unsafe"

const (
	BFONT_THIN_WIDTH = 12
	BFONT_WIDE_WIDTH = BFONT_THIN_WIDTH * 2
	BFONT_HEIGHT     = 24
)

const (
	BFONT_CODE_ISO8859_1 int32 = 0
	BFONT_CODE_EUC       int32 = 1
	BFONT_CODE_SJIS      int32 = 2
	BFONT_CODE_RAW       int32 = 3
)

const (
	BFONT_ABUTTON     uint32 = 0x86 + 0
	BFONT_BBUTTON     uint32 = 0x86 + 1
	BFONT_CBUTTON     uint32 = 0x86 + 2
	BFONT_DBUTTON     uint32 = 0x86 + 3
	BFONT_XBUTTON     uint32 = 0x86 + 4
	BFONT_YBUTTON     uint32 = 0x86 + 5
	BFONT_ZBUTTON     uint32 = 0x86 + 6
	BFONT_LTRIGGER    uint32 = 0x86 + 18
	BFONT_RTRIGGER    uint32 = 0x86 + 19
	BFONT_STARTBUTTON uint32 = 0x86 + 20
	BFONT_VMUICON     uint32 = 0x86 + 21
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
