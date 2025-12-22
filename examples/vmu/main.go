// VMU - Visual Memory Unit LCD and buzzer demo
package main

import (
	"kos"
	"unsafe"
)

var smiley = []byte{
	0b00111100, 0b01000010, 0b10100101, 0b10000001,
	0b10100101, 0b10011001, 0b01000010, 0b00111100,
}
var heart = []byte{
	0b01100110, 0b11111111, 0b11111111, 0b11111111,
	0b01111110, 0b00111100, 0b00011000, 0b00000000,
}
var star = []byte{
	0b00010000, 0b00111000, 0b11111110, 0b00111000,
	0b01101100, 0b11000110, 0b10000010, 0b00000000,
}

var (
	vmuFb           *kos.VmuFb
	vmuFont         *kos.VmuFont
	posX, posY      int
	animFrame       int
	animEnabled     bool = true
	beepPlaying     bool
	previousButtons uint32
	beepTone        uint8 = 0xF0
)

func renderVmu() {
	if vmuFb == nil {
		return
	}
	vmuFb.Clear()

	var sprite []byte
	switch animFrame % 3 {
	case 0:
		sprite = smiley
	case 1:
		sprite = heart
	case 2:
		sprite = star
	}

	if posX < 0 {
		posX = 0
	}
	if posX > kos.VMU_SCREEN_WIDTH-8 {
		posX = kos.VMU_SCREEN_WIDTH - 8
	}
	if posY < 0 {
		posY = 0
	}
	if posY > kos.VMU_SCREEN_HEIGHT-8 {
		posY = kos.VMU_SCREEN_HEIGHT - 8
	}

	vmuFb.PaintArea(posX, posY, 8, 8, sprite)

	for _, dev := range kos.GetAllVmuLcd() {
		vmuFb.Present(dev)
	}
}

func drawText(x, y int, text string, opaque bool) {
	kos.BfontDrawStr(kos.VramSOffset(y*640+x), 640, opaque, text)
}

func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	neg := n < 0
	if neg {
		n = -n
	}
	digits := make([]byte, 0, 10)
	for n > 0 {
		digits = append(digits, byte('0'+n%10))
		n /= 10
	}
	result := make([]byte, len(digits))
	for i := range digits {
		result[i] = digits[len(digits)-1-i]
	}
	if neg {
		return "-" + string(result)
	}
	return string(result)
}

func renderTV() {
	vram := kos.VramS()
	for i := 0; i < 640*480; i++ {
		*(*uint16)(unsafe.Pointer(uintptr(vram) + uintptr(i*2))) = 0x0008
	}

	kos.BfontSetEncoding(kos.BFONT_CODE_ISO8859_1)
	kos.BfontSetForegroundColor(0xFF00FFFF)
	drawText(kos.BFONT_THIN_WIDTH*2, kos.BFONT_HEIGHT, "=== VMU Demo ===", true)

	kos.BfontSetForegroundColor(0xFFFFFFFF)
	y := kos.BFONT_HEIGHT * 3

	if kos.GetFirstVmuLcd() != nil {
		drawText(kos.BFONT_THIN_WIDTH*2, y, "VMU LCD: Connected", true)
	} else {
		kos.BfontSetForegroundColor(0xFFFF0000)
		drawText(kos.BFONT_THIN_WIDTH*2, y, "VMU LCD: Not found", true)
		kos.BfontSetForegroundColor(0xFFFFFFFF)
	}
	y += kos.BFONT_HEIGHT

	if kos.GetFirstVmuClock() != nil {
		drawText(kos.BFONT_THIN_WIDTH*2, y, "VMU Buzzer: Connected", true)
	} else {
		kos.BfontSetForegroundColor(0xFFFF0000)
		drawText(kos.BFONT_THIN_WIDTH*2, y, "VMU Buzzer: Not found", true)
		kos.BfontSetForegroundColor(0xFFFFFFFF)
	}
	y += kos.BFONT_HEIGHT

	if beepPlaying {
		kos.BfontSetForegroundColor(0xFF00FF00)
		drawText(kos.BFONT_THIN_WIDTH*2, y, "Beep: Playing", true)
	} else {
		drawText(kos.BFONT_THIN_WIDTH*2, y, "Beep: Stopped", true)
	}
	kos.BfontSetForegroundColor(0xFFFFFFFF)
	y += kos.BFONT_HEIGHT

	if animEnabled {
		drawText(kos.BFONT_THIN_WIDTH*2, y, "Animation: ON", true)
	} else {
		drawText(kos.BFONT_THIN_WIDTH*2, y, "Animation: OFF", true)
	}
	y += kos.BFONT_HEIGHT * 2

	drawText(kos.BFONT_THIN_WIDTH*2, y, "Sprite: ("+itoa(posX)+", "+itoa(posY)+")", true)
	y += kos.BFONT_HEIGHT * 2

	kos.BfontSetForegroundColor(0xFF00FFFF)
	drawText(kos.BFONT_THIN_WIDTH*2, y, "Controls:", true)
	y += kos.BFONT_HEIGHT
	kos.BfontSetForegroundColor(0xFFFFFFFF)
	drawText(kos.BFONT_THIN_WIDTH*4, y, "D-Pad: Move sprite", true)
	y += kos.BFONT_HEIGHT
	drawText(kos.BFONT_THIN_WIDTH*4, y, "A: Play beep  B: Stop beep", true)
	y += kos.BFONT_HEIGHT
	drawText(kos.BFONT_THIN_WIDTH*4, y, "X: Toggle animation  START: Exit", true)
}

func buttonPressed(current, changed, button uint32) bool {
	return (changed & current & button) != 0
}

func handleInput() bool {
	ctrl := kos.MapleEnumType(0, kos.MAPLE_FUNC_CONTROLLER)
	if ctrl == nil {
		return true
	}
	state := ctrl.ContState()
	if state == nil {
		return true
	}

	current := state.Buttons
	changed := current ^ previousButtons
	previousButtons = current

	if buttonPressed(current, changed, kos.CONT_START) {
		return false
	}
	if (current & kos.CONT_DPAD_UP) != 0 {
		posY--
	}
	if (current & kos.CONT_DPAD_DOWN) != 0 {
		posY++
	}
	if (current & kos.CONT_DPAD_LEFT) != 0 {
		posX--
	}
	if (current & kos.CONT_DPAD_RIGHT) != 0 {
		posX++
	}
	if buttonPressed(current, changed, kos.CONT_A) {
		if vmuClock := kos.GetFirstVmuClock(); vmuClock != nil {
			kos.Beep(vmuClock, beepTone, beepTone/2)
			beepPlaying = true
		}
	}
	if buttonPressed(current, changed, kos.CONT_B) {
		if vmuClock := kos.GetFirstVmuClock(); vmuClock != nil {
			kos.StopBeep(vmuClock)
			beepPlaying = false
		}
	}
	if buttonPressed(current, changed, kos.CONT_X) {
		animEnabled = !animEnabled
	}
	return true
}

func main() {
	kos.VidSetMode(kos.DM_640x480, kos.PM_RGB555)

	vmuFb = kos.NewVmuFb()
	vmuFont = kos.GetVmuFont()
	posX = (kos.VMU_SCREEN_WIDTH - 8) / 2
	posY = (kos.VMU_SCREEN_HEIGHT - 8) / 2

	frameCount := 0
	for {
		if !handleInput() {
			break
		}
		if animEnabled {
			animFrame++
		}
		if frameCount%3 == 0 {
			renderVmu()
		}
		renderTV()
		kos.TimerSpinSleep(16)
		frameCount++
	}

	if vmuClock := kos.GetFirstVmuClock(); vmuClock != nil {
		kos.StopBeep(vmuClock)
	}
}
