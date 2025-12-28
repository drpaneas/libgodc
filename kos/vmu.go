//go:build gccgo

package kos

import "unsafe"

const (
	VMU_SCREEN_WIDTH  = 48
	VMU_SCREEN_HEIGHT = 32
)

// VmuFb matches KOS vmufb_t: uint32_t data[VMU_SCREEN_WIDTH]
// Using uint32 array ensures 4-byte alignment required by KOS.
type VmuFb struct {
	data [VMU_SCREEN_WIDTH]uint32
}

func NewVmuFb() *VmuFb {
	return &VmuFb{}
}

//extern vmufb_clear
func vmufbClear(fb uintptr)

func (fb *VmuFb) Clear() {
	vmufbClear(uintptr(unsafe.Pointer(&fb.data[0])))
}

//extern vmufb_paint_area
func vmufbPaintArea(fb uintptr, x, y, w, h int32, data uintptr)

func (fb *VmuFb) PaintArea(x, y, w, h int, data []byte) {
	if len(data) == 0 {
		return
	}
	vmufbPaintArea(uintptr(unsafe.Pointer(&fb.data[0])),
		int32(x), int32(y), int32(w), int32(h),
		uintptr(unsafe.Pointer(&data[0])))
}

// vmufb_present is void in KOS (dc/vmu_fb.h)
//
//extern vmufb_present
func vmufbPresent(fb uintptr, dev uintptr)

func (fb *VmuFb) Present(dev *MapleDevice) {
	if dev == nil {
		return
	}
	vmufbPresent(uintptr(unsafe.Pointer(&fb.data[0])),
		uintptr(unsafe.Pointer(dev)))
}

type VmuFont struct {
	_ [64]byte
}

//extern vmu_get_font
func vmuGetFont() uintptr

func GetVmuFont() *VmuFont {
	ptr := vmuGetFont()
	if ptr == 0 {
		return nil
	}
	return (*VmuFont)(unsafe.Pointer(ptr))
}

// vmufb_print_string_into is void in KOS (dc/vmu_fb.h)
//
//extern vmufb_print_string_into
func vmufbPrintStringInto(fb uintptr, font uintptr, x, y, w, h, line int32, str *byte)

func (fb *VmuFb) PrintString(font *VmuFont, x, y, w, h, line int, str string) {
	if font == nil {
		return
	}
	cstr := make([]byte, len(str)+1)
	copy(cstr, str)
	vmufbPrintStringInto(uintptr(unsafe.Pointer(&fb.data[0])),
		uintptr(unsafe.Pointer(font)),
		int32(x), int32(y), int32(w), int32(h), int32(line),
		&cstr[0])
}

//extern vmu_beep_raw
func vmuBeepRaw(dev uintptr, effect uint32) int32

func BeepRaw(dev *MapleDevice, effect uint32) int32 {
	if dev == nil {
		return -1
	}
	return vmuBeepRaw(uintptr(unsafe.Pointer(dev)), effect)
}

func Beep(dev *MapleDevice, period, duty uint8) int32 {
	effect := uint32(duty)<<8 | uint32(period)
	return BeepRaw(dev, effect)
}

func StopBeep(dev *MapleDevice) int32 {
	return BeepRaw(dev, 0)
}

//extern vmu_draw_lcd
func vmuDrawLcd(dev uintptr, bitmap uintptr) int32

func DrawLcd(dev *MapleDevice, bitmap []byte) int32 {
	if dev == nil || len(bitmap) < 192 {
		return -1
	}
	return vmuDrawLcd(uintptr(unsafe.Pointer(dev)), uintptr(unsafe.Pointer(&bitmap[0])))
}

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

//extern vmu_pkg_build
func vmuPkgBuild(pkg uintptr, outPtr *uintptr, outSize *int32) int32

//extern vmu_pkg_load_icon
func vmuPkgLoadIcon(pkg uintptr, path *byte) int32

const (
	VMUFS_OVERWRITE = 1 // Overwrite existing files
	VMUFS_VMUGAME   = 2 // This file is a VMU game
	VMUFS_NOCOPY    = 4 // Set the no-copy flag
)

//extern vmufs_write
func vmufsWrite(dev uintptr, filename *byte, data uintptr, size int32, flags int32) int32

func WriteVmu(dev *MapleDevice, filename string, data []byte, flags int32) int32 {
	if dev == nil || len(data) == 0 {
		return -1
	}
	cname := make([]byte, len(filename)+1)
	copy(cname, filename)
	return vmufsWrite(uintptr(unsafe.Pointer(dev)), &cname[0],
		uintptr(unsafe.Pointer(&data[0])), int32(len(data)), flags)
}

//extern vmufs_read
func vmufsRead(dev uintptr, filename *byte, outBuf *uintptr, outSize *int32) int32

//extern free
func cfree(ptr uintptr)

// ReadVmu reads a file from VMU. The buffer returned by vmufs_read() is
// malloc'd by KOS, so we must free it after copying the data.
func ReadVmu(dev *MapleDevice, filename string) []byte {
	if dev == nil {
		return nil
	}
	cname := make([]byte, len(filename)+1)
	copy(cname, filename)

	var bufPtr uintptr
	var size int32

	result := vmufsRead(uintptr(unsafe.Pointer(dev)), &cname[0], &bufPtr, &size)
	if result < 0 || bufPtr == 0 || size <= 0 {
		return nil
	}

	// Copy data from malloc'd buffer to Go slice
	data := make([]byte, size)
	for i := int32(0); i < size; i++ {
		data[i] = *(*byte)(unsafe.Pointer(bufPtr + uintptr(i)))
	}

	// Free the buffer allocated by vmufs_read()
	cfree(bufPtr)

	return data
}

//extern vmufs_delete
func vmufsDelete(dev uintptr, filename *byte) int32

func DeleteVmu(dev *MapleDevice, filename string) int32 {
	if dev == nil {
		return -1
	}
	cname := make([]byte, len(filename)+1)
	copy(cname, filename)
	return vmufsDelete(uintptr(unsafe.Pointer(dev)), &cname[0])
}

func GetFirstVmuLcd() *MapleDevice {
	return MapleEnumType(0, MAPLE_FUNC_LCD)
}

func GetAllVmuLcd() []*MapleDevice {
	var devices []*MapleDevice
	for i := 0; ; i++ {
		dev := MapleEnumType(i, MAPLE_FUNC_LCD)
		if dev == nil {
			break
		}
		devices = append(devices, dev)
	}
	return devices
}

func GetFirstVmuClock() *MapleDevice {
	return MapleEnumType(0, MAPLE_FUNC_CLOCK)
}
