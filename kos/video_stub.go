//go:build !gccgo

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

func VidInit(dispMode int32, pixelMode int32) { panic("kos: not on Dreamcast") }
func VidShutdown()                            { panic("kos: not on Dreamcast") }
func VidSetMode(dm int32, pm int32)           { panic("kos: not on Dreamcast") }

func VidCheckCable() int8 { panic("kos: not on Dreamcast") }

func VidSetVram(base uint32)      { panic("kos: not on Dreamcast") }
func VidSetStart(base uint32)     { panic("kos: not on Dreamcast") }
func VidGetStart(fb int32) uint32 { panic("kos: not on Dreamcast") }
func VidSetFb(fb int32)           { panic("kos: not on Dreamcast") }
func VidFlip(fb int32)            { panic("kos: not on Dreamcast") }

func VidBorderColor(r, g, b uint8) uint32 { panic("kos: not on Dreamcast") }
func VidClear(r, g, b uint8)              { panic("kos: not on Dreamcast") }
func VidEmpty()                           { panic("kos: not on Dreamcast") }
func VidGetEnabled() bool                 { panic("kos: not on Dreamcast") }
func vidSetEnabled(val int32)             { panic("kos: not on Dreamcast") }
func VidSetEnabled(val bool)              { panic("kos: not on Dreamcast") }
func VidWaitvbl()                         { panic("kos: not on Dreamcast") }

func vramSPtr() uintptr                     { panic("kos: not on Dreamcast") }
func VramS() unsafe.Pointer                 { panic("kos: not on Dreamcast") }
func VramSOffset(offset int) unsafe.Pointer { panic("kos: not on Dreamcast") }
