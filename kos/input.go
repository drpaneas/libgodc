//go:build gccgo

package kos

import "unsafe"

type ContState struct {
	Buttons uint32
	Ltrig   int32
	Rtrig   int32
	Joyx    int32
	Joyy    int32
	Joy2x   int32
	Joy2y   int32
}

func (c *ContState) ButtonPressed(btn uint32) bool {
	return c.Buttons&btn == btn
}

type MapleDevice struct {
	_ [256]byte
}

//extern maple_enum_count
func MapleEnumCount() int32

//extern maple_enum_type
func mapleEnumType(n int32, funcCode uint32) uintptr

func MapleEnumType(n int, funcCode uint32) *MapleDevice {
	ptr := mapleEnumType(int32(n), funcCode)
	if ptr == 0 {
		return nil
	}
	return (*MapleDevice)(unsafe.Pointer(ptr))
}

//extern maple_enum_dev
func mapleEnumDev(port, unit int32) uintptr

func MapleEnumDev(port, unit int) *MapleDevice {
	ptr := mapleEnumDev(int32(port), int32(unit))
	if ptr == 0 {
		return nil
	}
	return (*MapleDevice)(unsafe.Pointer(ptr))
}

//extern maple_dev_status
func mapleDevStatus(dev uintptr) uintptr

func (d *MapleDevice) Status() unsafe.Pointer {
	if d == nil {
		return nil
	}
	return unsafe.Pointer(mapleDevStatus(uintptr(unsafe.Pointer(d))))
}

func (d *MapleDevice) ContState() *ContState {
	if d == nil {
		return nil
	}
	ptr := mapleDevStatus(uintptr(unsafe.Pointer(d)))
	if ptr == 0 {
		return nil
	}
	return (*ContState)(unsafe.Pointer(ptr))
}

//extern maple_dev_valid
func mapleDevValid(port, unit int32) int32

func MapleDevValid(port, unit int) bool {
	return mapleDevValid(int32(port), int32(unit)) != 0
}

//extern maple_wait_scan
func MapleWaitScan()

//extern cont_init
func ContInit()

//extern cont_shutdown
func ContShutdown()

func GetController(port int) *ContState {
	dev := MapleEnumDev(port, 0)
	if dev == nil {
		return nil
	}
	return dev.ContState()
}

func GetFirstController() *ContState {
	dev := MapleEnumType(0, MAPLE_FUNC_CONTROLLER)
	if dev == nil {
		return nil
	}
	return dev.ContState()
}
