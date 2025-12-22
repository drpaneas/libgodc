// Controller input - shows all buttons, analog sticks, and triggers
package main

import "kos"

var (
	buttons              uint32
	analogX, analogY     int32
	analog2X, analog2Y   int32
	triggerL, triggerR   int32
)

func readInput() {
	ctrl := kos.MapleEnumType(0, kos.MAPLE_FUNC_CONTROLLER)
	if ctrl == nil {
		return
	}
	state := ctrl.ContState()
	if state == nil {
		return
	}
	buttons = state.Buttons
	analogX, analogY = state.Joyx, state.Joyy
	analog2X, analog2Y = state.Joy2x, state.Joy2y
	triggerL, triggerR = state.Ltrig, state.Rtrig
}

func isPressed(btn uint32) bool {
	return (buttons & btn) != 0
}

func drawRect(x, y, w, h, z float32, color uint32) {
	kos.PlxVertInp(kos.PLX_VERT, x, y+h, z, color)
	kos.PlxVertInp(kos.PLX_VERT, x, y, z, color)
	kos.PlxVertInp(kos.PLX_VERT, x+w, y+h, z, color)
	kos.PlxVertInp(kos.PLX_VERT_EOS, x+w, y, z, color)
}

func drawButton(x, y, size float32, pressed bool, activeColor uint32) {
	color := uint32(0xFF333340)
	if pressed {
		color = activeColor
	}
	drawRect(x, y, size, size, 100, color)
}

func drawAnalogStick(cx, cy float32, stickX, stickY int32) {
	drawRect(cx-50, cy-50, 100, 100, 50, 0xFF1E1E2A)
	drawRect(cx-10, cy-10, 20, 20, 60, 0xFF333340)
	ox := float32(stickX) * 40.0 / 128.0
	oy := float32(stickY) * 40.0 / 128.0
	drawRect(cx+ox-8, cy+oy-8, 16, 16, 100, 0xFF6699FF)
}

func drawTrigger(x, y float32, value int32) {
	drawRect(x, y, 30, 100, 50, 0xFF1E1E2A)
	h := float32(value) * 100.0 / 255.0
	drawRect(x, y+(100-h), 30, h, 100, 0xFFCC8855)
}

func render() {
	kos.PvrWaitReady()
	kos.PvrSceneBegin()
	kos.PvrListBegin(kos.PVR_LIST_OP_POLY)

	kos.PlxCxtInit()
	kos.PlxCxtTexture(nil)
	kos.PlxCxtCulling(kos.PLX_CULL_NONE)
	kos.PlxCxtSend(int32(kos.PVR_LIST_OP_POLY))

	// Background
	drawRect(0, 0, 640, 480, 1, 0xFF14141E)

	// D-Pad
	dpadX, dpadY, dpadSize := float32(80), float32(200), float32(35)
	drawRect(dpadX-10, dpadY-10, dpadSize*3+20, dpadSize*3+20, 40, 0xFF1E1E2A)
	drawButton(dpadX+dpadSize, dpadY, dpadSize, isPressed(kos.CONT_DPAD_UP), 0xFF4DCC66)
	drawButton(dpadX+dpadSize, dpadY+dpadSize*2, dpadSize, isPressed(kos.CONT_DPAD_DOWN), 0xFF4DCC66)
	drawButton(dpadX, dpadY+dpadSize, dpadSize, isPressed(kos.CONT_DPAD_LEFT), 0xFF4DCC66)
	drawButton(dpadX+dpadSize*2, dpadY+dpadSize, dpadSize, isPressed(kos.CONT_DPAD_RIGHT), 0xFF4DCC66)

	// Face buttons (A=green, B=red, X=blue, Y=yellow)
	faceX, faceY, faceSize := float32(480), float32(200), float32(40)
	drawRect(faceX-15, faceY-15, faceSize*3+30, faceSize*3+30, 40, 0xFF1E1E2A)
	drawButton(faceX+faceSize, faceY+faceSize*2, faceSize, isPressed(kos.CONT_A), 0xFF4DCC4D)
	drawButton(faceX+faceSize*2, faceY+faceSize, faceSize, isPressed(kos.CONT_B), 0xFFCC4D4D)
	drawButton(faceX, faceY+faceSize, faceSize, isPressed(kos.CONT_X), 0xFF4D4DCC)
	drawButton(faceX+faceSize, faceY, faceSize, isPressed(kos.CONT_Y), 0xFFCCCC4D)

	// Start button
	drawRect(285, 345, 70, 35, 40, 0xFF1E1E2A)
	drawButton(290, 350, 60, isPressed(kos.CONT_START), 0xFF4DCC66)

	// Analog sticks
	drawAnalogStick(200, 400, analogX, analogY)
	drawAnalogStick(440, 400, analog2X, analog2Y)

	// Triggers
	drawTrigger(30, 50, triggerL)
	drawTrigger(580, 50, triggerR)

	kos.PvrListFinish()
	kos.PvrSceneFinish()
}

func main() {
	kos.PvrInitDefaults()
	kos.PvrSetBgColor(0.08, 0.08, 0.12)

	for {
		readInput()
		if isPressed(kos.CONT_START) {
			for isPressed(kos.CONT_START) {
				readInput()
				render()
			}
			break
		}
		render()
	}
}
