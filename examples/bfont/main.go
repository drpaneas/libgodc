// BIOS font - Dreamcast's built-in font rendering
package main

import (
	"kos"
	"unsafe"
)

func main() {
	// Draw XOR pattern background
	vram := kos.VramS()
	for y := 0; y < 480; y++ {
		for x := 0; x < 640; x++ {
			c := (x ^ y) & 255
			pixel := uint16(((c >> 3) << 11) | ((c >> 2) << 5) | (c >> 3))
			*(*uint16)(unsafe.Pointer(uintptr(vram) + uintptr((y*640+x)*2))) = pixel
		}
	}

	o := (640 * kos.BFONT_HEIGHT) + (kos.BFONT_THIN_WIDTH * 2)

	kos.BfontSetEncoding(kos.BFONT_CODE_ISO8859_1)
	kos.BfontDrawStr(kos.VramSOffset(o), 640, true, "Test of basic ASCII")
	o += 640 * kos.BFONT_HEIGHT

	kos.BfontDrawStr(kos.VramSOffset(o), 640, true, "Hello from Go on Dreamcast!")
	o += 640 * kos.BFONT_HEIGHT

	kos.BfontDrawStr(kos.VramSOffset(o), 640, false, "Transparent text rendering")
	o += 640 * kos.BFONT_HEIGHT

	kos.BfontDrawStr(kos.VramSOffset(o), 640, false, "BIOS font demo complete!")
	o += 640 * kos.BFONT_HEIGHT

	kos.BfontDrawStr(kos.VramSOffset(o), 640, true, "To exit, press ")
	kos.BfontSetEncoding(kos.BFONT_CODE_RAW)
	kos.BfontDrawWide(kos.VramSOffset(o+(kos.BFONT_THIN_WIDTH*15)), 640, true, kos.BFONT_STARTBUTTON)

	for {
		kos.TimerSpinSleep(50)
	}
}
