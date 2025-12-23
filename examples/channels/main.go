// Channels - producer/consumer pattern with goroutines
package main

import "kos"

type Square struct {
	X, Y  float32
	Color uint32
}

var squareChan chan Square
var requestChan chan bool
var squares [50]Square
var squareCount int
var seed uint32 = 54321

var colors = []uint32{
	0xFFFF4D4D, 0xFFFF994D, 0xFFFFFF4D, 0xFF4DFF4D,
	0xFF4DFFFF, 0xFF4D4DFF, 0xFFFF4DFF,
}

func random() float32 {
	seed = seed*1103515245 + 12345
	return float32(seed%1000) / 1000.0
}

func producer() {
	colorIndex := 0
	for {
		<-requestChan
		sq := Square{
			X:     50 + random()*540,
			Y:     80 + random()*350,
			Color: colors[colorIndex%len(colors)],
		}
		colorIndex++
		select {
		case squareChan <- sq:
		default:
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

	drawRect(0, 0, 640, 480, 1, 0xFF140D1E)
	drawRect(0, 0, 640, 60, 50, 0xFF261A33)

	// Channel buffer indicator
	for i := 0; i < len(squareChan); i++ {
		drawRect(float32(20+i*15), 15, 12, 30, 100, 0xFF4DCC4D)
	}
	for i := len(squareChan); i < cap(squareChan); i++ {
		drawRect(float32(20+i*15), 15, 12, 30, 100, 0xFF333333)
	}

	for i := 0; i < squareCount; i++ {
		drawRect(squares[i].X+3, squares[i].Y+3, 30, 30, 99, 0x80000000)
		drawRect(squares[i].X, squares[i].Y, 30, 30, 100, squares[i].Color)
	}

	kos.PvrListFinish()
	kos.PvrSceneFinish()
}

func main() {
	kos.PvrInitDefaults()
	kos.PvrSetBgColor(0.08, 0.05, 0.12)

	squareChan = make(chan Square, 10)
	requestChan = make(chan bool, 1)

	go producer()

	for {
		readInput()
		if justPressed(kos.CONT_START) {
			break
		}
		if justPressed(kos.CONT_A) {
			select {
			case requestChan <- true:
			default:
			}
		}
		if justPressed(kos.CONT_B) {
			squareCount = 0
		}

		for {
			select {
			case sq := <-squareChan:
				if squareCount < 50 {
					squares[squareCount] = sq
					squareCount++
				}
			default:
				goto done
			}
		}
	done:
		render()
	}
}
