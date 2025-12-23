//go:build gccgo

package kos

import "unsafe"

const (
	PNG_NO_ALPHA   int32 = 0
	PNG_MASK_ALPHA int32 = 1
	PNG_FULL_ALPHA int32 = 2
)

//extern png_to_texture
func pngToTexture(filename uintptr, tex uint32, mask int32) int32

func PngToTexture(filename string, tex PvrPtr, mask int32) int32 {
	cstr := make([]byte, len(filename)+1)
	copy(cstr, filename)
	return pngToTexture(uintptr(unsafe.Pointer(&cstr[0])), uint32(tex), mask)
}

//extern png_load_texture
func pngLoadTexture(filename uintptr, tex *uint32, mask int32, w, h *uint32) int32

func PngLoadTexture(filename string, mask int32) (tex PvrPtr, w, h uint32, err int32) {
	cstr := make([]byte, len(filename)+1)
	copy(cstr, filename)
	var texVal uint32
	err = pngLoadTexture(uintptr(unsafe.Pointer(&cstr[0])), &texVal, mask, &w, &h)
	tex = PvrPtr(texVal)
	return
}

//extern snd_stream_init
func sndStreamInit(callback unsafe.Pointer)

func SndStreamInit() {
	sndStreamInit(nil)
}

//extern snd_stream_shutdown
func SndStreamShutdown()
