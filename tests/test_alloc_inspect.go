//go:build ignore

// test_alloc_inspect.go - Memory allocation inspection test
package main

import "unsafe"

type Player struct {
	X, Y  float32
	Score int32
}

var globalCounter int = 12345

//go:noinline
func allocOnHeap() *Player {
	return &Player{X: 10, Y: 20, Score: 100}
}

//go:noinline
func allocMultiple(n int) []*Player {
	result := make([]*Player, n)
	for i := 0; i < n; i++ {
		result[i] = &Player{X: float32(i), Y: float32(i * 2), Score: int32(i * 100)}
	}
	return result
}

func formatHex(addr uintptr) string {
	const hexDigits = "0123456789abcdef"
	buf := make([]byte, 10)
	buf[0] = '0'
	buf[1] = 'x'
	for i := 9; i >= 2; i-- {
		buf[i] = hexDigits[addr&0xf]
		addr >>= 4
	}
	return string(buf)
}

func testStackVsHeap() {
	println("stack vs heap:")

	var local Player
	local.X = 1
	stackAddr := uintptr(unsafe.Pointer(&local))
	println("  stack addr:", formatHex(stackAddr))

	p := allocOnHeap()
	heapAddr := uintptr(unsafe.Pointer(p))
	println("  heap addr: ", formatHex(heapAddr))
}

func testBumpPointer() {
	println("bump pointer pattern:")
	println("  (Player = 12 bytes + 8-byte header, aligned to 24)")

	players := allocMultiple(5)
	var prevAddr uintptr
	for i, player := range players {
		addr := uintptr(unsafe.Pointer(player))
		if i == 0 {
			println("  player", i, ":", formatHex(addr))
		} else {
			diff := int64(addr) - int64(prevAddr)
			println("  player", i, ":", formatHex(addr), "+", diff)
		}
		prevAddr = addr
	}
}

func testDifferentSizes() {
	println("different sizes:")

	small := new(uint64)
	*small = 42
	println("  uint64 (8B): ", formatHex(uintptr(unsafe.Pointer(small))))

	type S16 struct{ a, b uint64 }
	s16 := new(S16)
	println("  struct (16B):", formatHex(uintptr(unsafe.Pointer(s16))))

	type S32 struct{ a, b, c, d uint64 }
	s32 := new(S32)
	println("  struct (32B):", formatHex(uintptr(unsafe.Pointer(s32))))

	slice := make([]int, 10)
	slice[0] = 1
	println("  []int(10):  ", formatHex(uintptr(unsafe.Pointer(&slice[0]))))
}

func testGlobalVariable() {
	println("global variable:")
	globalAddr := uintptr(unsafe.Pointer(&globalCounter))
	println("  data segment:", formatHex(globalAddr))
}

func main() {
	println("test_alloc_inspect")
	println("")

	testStackVsHeap()
	testBumpPointer()
	testDifferentSizes()
	testGlobalVariable()

	println("")
	println("done")
}
