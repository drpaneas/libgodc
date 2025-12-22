//go:build !gccgo

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

func (c *ContState) ButtonPressed(btn uint32) bool { panic("kos: not on Dreamcast") }

type MapleDevice struct {
	_ [256]byte
}

func MapleEnumCount() int32                             { panic("kos: not on Dreamcast") }
func mapleEnumType(n int32, funcCode uint32) uintptr    { panic("kos: not on Dreamcast") }
func MapleEnumType(n int, funcCode uint32) *MapleDevice { panic("kos: not on Dreamcast") }
func mapleEnumDev(port, unit int32) uintptr             { panic("kos: not on Dreamcast") }
func MapleEnumDev(port, unit int) *MapleDevice          { panic("kos: not on Dreamcast") }

func mapleDevStatus(dev uintptr) uintptr      { panic("kos: not on Dreamcast") }
func (d *MapleDevice) Status() unsafe.Pointer { panic("kos: not on Dreamcast") }
func (d *MapleDevice) ContState() *ContState  { panic("kos: not on Dreamcast") }

func mapleDevValid(port, unit int32) int32 { panic("kos: not on Dreamcast") }
func MapleDevValid(port, unit int) bool    { panic("kos: not on Dreamcast") }

func MapleWaitScan() { panic("kos: not on Dreamcast") }
func ContInit()      { panic("kos: not on Dreamcast") }
func ContShutdown()  { panic("kos: not on Dreamcast") }

func GetController(port int) *ContState { panic("kos: not on Dreamcast") }
func GetFirstController() *ContState    { panic("kos: not on Dreamcast") }
