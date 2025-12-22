// Goroutines - multiple bouncing balls, each managed by its own goroutine
package main

import "kos"

const MaxBalls = 20

type Ball struct {
	X, Y, VX, VY float32
	Color        uint32
	Active       bool
}

var balls [MaxBalls]Ball
var ballCount int
var seed uint32 = 12345

var colors = []uint32{
	0xFFFF4D4D, 0xFFFF994D, 0xFFFFFF4D, 0xFF4DFF4D,
	0xFF4DFFFF, 0xFF4D4DFF, 0xFFFF4DFF,
}

func random() float32 {
	seed = seed*1103515245 + 12345
	return float32(seed%1000) / 1000.0
}

func randomRange(min, max float32) float32 {
	return min + random()*(max-min)
}

func runBall(index int) {
	ball := &balls[index]
	for ball.Active {
		ball.X += ball.VX
		ball.Y += ball.VY

		if ball.X < 0 || ball.X > 620 {
			ball.VX = -ball.VX
		}
		if ball.Y < 50 || ball.Y > 460 {
			ball.VY = -ball.VY
		}
		kos.ThdSleep(16)
	}
}

func addBall() {
	if ballCount >= MaxBalls {
		return
	}
	for i := 0; i < MaxBalls; i++ {
		if !balls[i].Active {
			balls[i] = Ball{
				X: randomRange(100, 540), Y: randomRange(100, 380),
				VX: randomRange(-4, 4), VY: randomRange(-4, 4),
				Color: colors[ballCount%len(colors)], Active: true,
			}
			if balls[i].VX > -1 && balls[i].VX < 1 {
				balls[i].VX = 2
			}
			if balls[i].VY > -1 && balls[i].VY < 1 {
				balls[i].VY = 2
			}
			go runBall(i)
			ballCount++
			return
		}
	}
}

func removeBall() {
	for i := MaxBalls - 1; i >= 0; i-- {
		if balls[i].Active {
			balls[i].Active = false
			ballCount--
			return
		}
	}
}

var buttonsNow, buttonsPrev uint32

func readInput() {
	buttonsPrev = buttonsNow
	if ctrl := kos.MapleEnumType(0, kos.MAPLE_FUNC_CONTROLLER); ctrl != nil {
		if state := ctrl.ContState(); state != nil {
			buttonsNow = state.Buttons
		}
	}
}

func justPressed(btn uint32) bool {
	return (buttonsPrev&btn) == 0 && (buttonsNow&btn) != 0
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

	drawRect(0, 0, 640, 480, 1, 0xFF0D0D26)
	drawRect(0, 0, 640, 45, 50, 0xFF1A1A40)

	for i := 0; i < ballCount; i++ {
		drawRect(float32(20+i*30), 10, 25, 25, 100, colors[i%len(colors)])
	}

	for i := 0; i < MaxBalls; i++ {
		if balls[i].Active {
			drawRect(balls[i].X+3, balls[i].Y+3, 20, 20, 99, 0x80000000)
			drawRect(balls[i].X, balls[i].Y, 20, 20, 100, balls[i].Color)
		}
	}

	kos.PvrListFinish()
	kos.PvrSceneFinish()
}

func main() {
	kos.PvrInitDefaults()
	kos.PvrSetBgColor(0.05, 0.05, 0.15)

	for i := 0; i < 3; i++ {
		addBall()
	}

	for {
		readInput()
		if justPressed(kos.CONT_START) {
			break
		}
		if justPressed(kos.CONT_A) {
			addBall()
		}
		if justPressed(kos.CONT_B) {
			removeBall()
		}
		render()
	}

	for i := 0; i < MaxBalls; i++ {
		balls[i].Active = false
	}
	kos.ThdSleep(100)
}
