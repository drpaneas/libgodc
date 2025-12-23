//go:build !gccgo

package kos

import "unsafe"

const (
	PNG_NO_ALPHA   int32 = 0
	PNG_MASK_ALPHA int32 = 1
	PNG_FULL_ALPHA int32 = 2
)

func pngToTexture(filename uintptr, tex uint32, mask int32) int32 { return -1 }
func PngToTexture(filename string, tex PvrPtr, mask int32) int32  { return -1 }
func pngLoadTexture(filename uintptr, tex *uint32, mask int32, w, h *uint32) int32 {
	return -1
}
func PngLoadTexture(filename string, mask int32) (tex PvrPtr, w, h uint32, err int32) {
	return 0, 0, 0, -1
}

func sndStreamInit(callback unsafe.Pointer) {}
func SndStreamInit()                        {}
func SndStreamShutdown()                    {}
