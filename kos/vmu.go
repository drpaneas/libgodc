//go:build gccgo

package kos

import "unsafe"

const (
	VMU_SCREEN_WIDTH  = 48
	VMU_SCREEN_HEIGHT = 32
)

type VmuFb struct {
	data [VMU_SCREEN_WIDTH * VMU_SCREEN_HEIGHT / 8]byte
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

//extern vmufb_present
func vmufbPresent(fb uintptr, dev uintptr) int32

func (fb *VmuFb) Present(dev *MapleDevice) int32 {
	if dev == nil {
		return -1
	}
	return vmufbPresent(uintptr(unsafe.Pointer(&fb.data[0])),
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

//extern vmufb_print_string_into
func vmufbPrintStringInto(fb uintptr, font uintptr, x, y, w, h, line int32, str *byte) int32

func (fb *VmuFb) PrintString(font *VmuFont, x, y, w, h, line int, str string) int32 {
	if font == nil {
		return -1
	}
	cstr := make([]byte, len(str)+1)
	copy(cstr, str)
	return vmufbPrintStringInto(uintptr(unsafe.Pointer(&fb.data[0])),
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

type VmuPkg struct {
	DescShort     [16]byte
	DescLong      [32]byte
	AppId         [16]byte
	IconCnt       int32
	IconAnimSpeed int32
	EyecatchType  int32
	DataLen       int32
	IconData      unsafe.Pointer
	EyecatchData  unsafe.Pointer
	Data          unsafe.Pointer
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
	VMUFS_VMUGAME = 1
	VMUFS_NOCOPY  = 2
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

	data := make([]byte, size)
	for i := int32(0); i < size; i++ {
		data[i] = *(*byte)(unsafe.Pointer(bufPtr + uintptr(i)))
	}

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
