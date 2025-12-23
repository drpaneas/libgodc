// Hello World on Dreamcast screen using BIOS font
package main

import "kos"

func main() {
	// center "Hello World" on 640x480 screen
	x := 640/2 - (11*kos.BFONT_THIN_WIDTH)/2
	y := 480/2 - kos.BFONT_HEIGHT/2
	offset := y*640 + x

	kos.BfontDrawStr(kos.VramSOffset(offset), 640, true, "Hello World")

	for {
		kos.TimerSpinSleep(100)
	}
}

