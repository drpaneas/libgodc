// Timer - frame-rate independent animation using delta time
package main

import "kos"

var (
	squareX, squareY   float32 = 295, 215
	velocityX, velocityY float32 = 150, 100
	autoMove           bool    = true
	lastTime           uint64
	deltaTime          float32
	frameCount         int
	fpsTimer           uint64
	fps                int
)

var buttonsNow, buttonsPrev uint32
var joystickX, joystickY int32

func updateTiming() {
	now := kos.TimerMsGettime64()
	if lastTime > 0 {
		deltaTime = float32(now-lastTime) / 1000.0
		if deltaTime > 0.1 {
			deltaTime = 0.1
		}
	} else {
		deltaTime = 1.0 / 60.0
	}
	lastTime = now

	frameCount++
	if now-fpsTimer >= 1000 {
		fps = frameCount
		frameCount = 0
		fpsTimer = now
	}
}

func readInput() {
	buttonsPrev = buttonsNow
	joystickX, joystickY = 0, 0
	if ctrl := kos.MapleEnumType(0, kos.MAPLE_FUNC_CONTROLLER); ctrl != nil {
		if state := ctrl.ContState(); state != nil {
			buttonsNow = state.Buttons
			joystickX, joystickY = state.Joyx, state.Joyy
		}
	}
}

func justPressed(btn uint32) bool {
	return (buttonsPrev&btn) == 0 && (buttonsNow&btn) != 0
}

func isHeld(btn uint32) bool {
	return (buttonsNow & btn) != 0
}

func updateMovement() {
	const speed = 200.0
	if autoMove {
		squareX += velocityX * deltaTime
		squareY += velocityY * deltaTime
		if squareX < 80 || squareX > 590 {
			velocityX = -velocityX
		}
		if squareY < 0 || squareY > 430 {
			velocityY = -velocityY
		}
	} else {
		if isHeld(kos.CONT_DPAD_LEFT) {
			squareX -= speed * deltaTime
		}
		if isHeld(kos.CONT_DPAD_RIGHT) {
			squareX += speed * deltaTime
		}
		if isHeld(kos.CONT_DPAD_UP) {
			squareY -= speed * deltaTime
		}
		if isHeld(kos.CONT_DPAD_DOWN) {
			squareY += speed * deltaTime
		}
		if joystickX > 15 || joystickX < -15 {
			squareX += float32(joystickX) / 128.0 * speed * deltaTime
		}
		if joystickY > 15 || joystickY < -15 {
			squareY += float32(joystickY) / 128.0 * speed * deltaTime
		}
	}
	if squareX < 80 {
		squareX = 80
	}
	if squareX > 590 {
		squareX = 590
	}
	if squareY < 0 {
		squareY = 0
	}
	if squareY > 430 {
		squareY = 430
	}
}

func drawRect(x, y, w, h, z float32, color uint32) {
	kos.PlxVertInp(kos.PLX_VERT, x, y+h, z, color)
	kos.PlxVertInp(kos.PLX_VERT, x, y, z, color)
	kos.PlxVertInp(kos.PLX_VERT, x+w, y+h, z, color)
	kos.PlxVertInp(kos.PLX_VERT_EOS, x+w, y, z, color)
}

func render() {
	kos.PvrWaitReady()
	kos.PvrSceneBegin()
	kos.PvrListBegin(kos.PVR_LIST_OP_POLY)

	kos.PlxCxtInit()
	kos.PlxCxtTexture(nil)
	kos.PlxCxtCulling(kos.PLX_CULL_NONE)
	kos.PlxCxtSend(int32(kos.PVR_LIST_OP_POLY))

	drawRect(0, 0, 640, 240, 1, 0xFF0D0D1A)
	drawRect(0, 240, 640, 240, 1, 0xFF1A1A33)

	// Info panel
	drawRect(0, 0, 75, 480, 50, 0xFF14142A)

	// FPS bar
	fpsColor := uint32(0xFF4DCC4D)
	if fps < 30 {
		fpsColor = 0xFFCC4D4D
	} else if fps < 50 {
		fpsColor = 0xFFCCCC4D
	}
	fpsHeight := float32(fps) * 4
	if fpsHeight > 300 {
		fpsHeight = 300
	}
	drawRect(10, 480-10-fpsHeight, 20, fpsHeight, 100, fpsColor)

	// Delta time bar
	dtHeight := deltaTime * 3000
	if dtHeight > 200 {
		dtHeight = 200
	}
	drawRect(40, 480-10-dtHeight, 20, dtHeight, 100, 0xFF4D80CC)

	// Mode indicator
	if autoMove {
		drawRect(10, 20, 55, 30, 100, 0xFF4DCC4D)
	} else {
		drawRect(10, 20, 55, 30, 100, 0xFFCC804D)
	}

	// Square with shadow
	drawRect(squareX+4, squareY+4, 50, 50, 99, 0x80000000)
	drawRect(squareX, squareY, 50, 50, 100, 0xFF6699FF)

	kos.PvrListFinish()
	kos.PvrSceneFinish()
}

func main() {
	kos.PvrInitDefaults()
	kos.PvrSetBgColor(0.05, 0.05, 0.1)

	lastTime = kos.TimerMsGettime64()
	fpsTimer = lastTime

	for {
		updateTiming()
		readInput()

		if justPressed(kos.CONT_START) {
			break
		}
		if justPressed(kos.CONT_A) {
			autoMove = !autoMove
		}

		updateMovement()
		render()
	}
}
