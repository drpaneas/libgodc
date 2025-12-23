//go:build gccgo

package kos

import "unsafe"

const (
	CT_ANY       int8 = -1
	CT_VGA       int8 = 0
	CT_NONE      int8 = 1
	CT_RGB       int8 = 2
	CT_COMPOSITE int8 = 3
)

const (
	VID_320x240     int32 = 0x1000
	VID_640x480     int32 = 0x1001
	VID_256x256     int32 = 0x1002
	VID_768x480     int32 = 0x1003
	VID_768x576     int32 = 0x1004
	VID_MULTIBUFFER int32 = 0x2000
)

const (
	VID_INTERLACE   uint32 = 0x00000001
	VID_LINEDOUBLE  uint32 = 0x00000002
	VID_PIXELDOUBLE uint32 = 0x00000004
	VID_PAL         uint32 = 0x00000008
)

//extern vid_init
func VidInit(dispMode int32, pixelMode int32)

//extern vid_shutdown
func VidShutdown()

//extern vid_set_mode
func VidSetMode(dm int32, pm int32)

//extern vid_check_cable
func VidCheckCable() int8

//extern vid_set_vram
func VidSetVram(base uint32)

//extern vid_set_start
func VidSetStart(base uint32)

//extern vid_get_start
func VidGetStart(fb int32) uint32

//extern vid_set_fb
func VidSetFb(fb int32)

//extern vid_flip
func VidFlip(fb int32)

//extern vid_border_color
func VidBorderColor(r, g, b uint8) uint32

//extern vid_clear
func VidClear(r, g, b uint8)

//extern vid_empty
func VidEmpty()

//extern vid_get_enabled
func VidGetEnabled() bool

//extern vid_set_enabled
func vidSetEnabled(val int32)

func VidSetEnabled(val bool) {
	if val {
		vidSetEnabled(1)
	} else {
		vidSetEnabled(0)
	}
}

//extern vid_waitvbl
func VidWaitvbl()

//extern __go_vram_s
func vramSPtr() uintptr

func VramS() unsafe.Pointer {
	return unsafe.Pointer(vramSPtr())
}

func VramSOffset(offset int) unsafe.Pointer {
	return unsafe.Pointer(vramSPtr() + uintptr(offset*2))
}
