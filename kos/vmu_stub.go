//go:build !gccgo

package kos

import "unsafe"

const (
	VMU_SCREEN_WIDTH  = 48
	VMU_SCREEN_HEIGHT = 32
)

type VmuFb struct {
	data [VMU_SCREEN_WIDTH * VMU_SCREEN_HEIGHT / 8]byte
}

func NewVmuFb() *VmuFb                                  { panic("kos: not on Dreamcast") }
func (fb *VmuFb) Clear()                                { panic("kos: not on Dreamcast") }
func (fb *VmuFb) PaintArea(x, y, w, h int, data []byte) { panic("kos: not on Dreamcast") }
func (fb *VmuFb) Present(dev *MapleDevice) int32        { panic("kos: not on Dreamcast") }
func (fb *VmuFb) PrintString(font *VmuFont, x, y, w, h, line int, str string) int32 {
	panic("kos: not on Dreamcast")
}

type VmuFont struct {
	_ [64]byte
}

func GetVmuFont() *VmuFont { panic("kos: not on Dreamcast") }

func BeepRaw(dev *MapleDevice, effect uint32) int32  { panic("kos: not on Dreamcast") }
func Beep(dev *MapleDevice, period, duty uint8) int32 { panic("kos: not on Dreamcast") }
func StopBeep(dev *MapleDevice) int32                 { panic("kos: not on Dreamcast") }

func DrawLcd(dev *MapleDevice, bitmap []byte) int32 { panic("kos: not on Dreamcast") }

const VMU_ICON_SIZE = 32 * 32 / 2

// VmuPkg matches KOS vmu_pkg_t struct layout exactly
type VmuPkg struct {
	DescShort     [20]byte       // Short description (max 20 chars)
	DescLong      [36]byte       // Long description (max 36 chars)
	AppId         [20]byte       // Application ID (max 20 chars)
	IconCnt       int32          // Number of icons
	IconAnimSpeed int32          // Icon animation speed
	EyecatchType  int32          // Eyecatch type (VMUPKG_EC_*)
	DataLen       int32          // Length of data
	IconPal       [16]uint16     // Icon palette (ARGB4444)
	IconData      unsafe.Pointer // Icon bitmap data
	EyecatchData  unsafe.Pointer // Eyecatch data
	Data          unsafe.Pointer // Save data
}

const (
	VMUPKG_EC_NONE   = 0
	VMUPKG_EC_16BIT  = 1
	VMUPKG_EC_256COL = 2
	VMUPKG_EC_16COL  = 3
)

const (
	VMUFS_OVERWRITE = 1 // Overwrite existing files
	VMUFS_VMUGAME   = 2 // This file is a VMU game
	VMUFS_NOCOPY    = 4 // Set the no-copy flag
)

func WriteVmu(dev *MapleDevice, filename string, data []byte, flags int32) int32 {
	panic("kos: not on Dreamcast")
}
func ReadVmu(dev *MapleDevice, filename string) []byte { panic("kos: not on Dreamcast") }
func DeleteVmu(dev *MapleDevice, filename string) int32 { panic("kos: not on Dreamcast") }

func GetFirstVmuLcd() *MapleDevice   { panic("kos: not on Dreamcast") }
func GetAllVmuLcd() []*MapleDevice   { panic("kos: not on Dreamcast") }
func GetFirstVmuClock() *MapleDevice { panic("kos: not on Dreamcast") }

