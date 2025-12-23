// BRKOUT - A Breakout Clone for Sega Dreamcast
//
// Copyright (C) 2002-2004 Jim Ursetto
// Copyright (C) 2025 Panagiotis Georgiadis
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
//
// This is a Go port of the original brkout game by Jim Ursetto.
// Original: https://3e8.org/hacks/brkout/
//
// Controls:
//   D-Pad/Analog - Move paddle
//   A/START      - Select
//   START+B      - Return to menu

package main

import "kos"

const (
	ScreenWidth  = 640
	ScreenHeight = 480

	FieldLeft   = 10
	FieldTop    = 40
	FieldWidth  = 500
	FieldHeight = 420

	BrickCols   = 20
	BrickRows   = 15
	BrickWidth  = 25
	BrickHeight = 12

	PaddleWidth  = 70
	PaddleHeight = 10
	PaddleSpeed  = 6.0
	PaddleY      = FieldHeight - 30

	BallSize      = 12
	BallRadius    = BallSize / 2
	StartSpeed    = 3.0
	MaxSpeed      = 7.0
	SpeedIncrease = 0.5

	StickDeadzone = 15
	StickScale    = 18.0

	StartingLives  = 3
	PointsPerBrick = 10

	charW    = 12
	charH    = 24
	fontSize = 256.0
)

func white() uint32      { return kos.PlxPackColor(1, 1, 1, 1) }
func gray() uint32       { return kos.PlxPackColor(1, 0.5, 0.5, 0.5) }
func darkBlue() uint32   { return kos.PlxPackColor(1, 0.05, 0.05, 0.1) }
func wallBlue() uint32   { return kos.PlxPackColor(1, 0.4, 0.4, 0.6) }
func paddleBlue() uint32 { return kos.PlxPackColor(1, 0.4, 0.4, 0.7) }
func shadow() uint32     { return kos.PlxPackColor(0.7, 0, 0, 0) }

var brickColors = []uint32{
	kos.PlxPackColor(1, 1.0, 0.2, 0.2),
	kos.PlxPackColor(1, 1.0, 0.6, 0.2),
	kos.PlxPackColor(1, 1.0, 1.0, 0.2),
	kos.PlxPackColor(1, 0.2, 1.0, 0.2),
	kos.PlxPackColor(1, 0.2, 1.0, 1.0),
	kos.PlxPackColor(1, 0.2, 0.2, 1.0),
	kos.PlxPackColor(1, 0.8, 0.2, 1.0),
}

var (
	titleImage *kos.PlxTexture
	fontImage  *kos.PlxTexture
	fieldImage *kos.PlxTexture

	soundClick  kos.SfxHandle
	soundBounce kos.SfxHandle
	soundHit    kos.SfxHandle
	soundLose   kos.SfxHandle
	soundWin    kos.SfxHandle
)

var (
	buttonsNow  uint32
	buttonsPrev uint32
	stickX      int32
)

var (
	paddleX            float32
	ballX, ballY       float32
	ballSpeedX, speedY float32
	bricks             [BrickCols * BrickRows]int
	bricksLeft         int
	lives              int
	score              int
	level              int
)

func loadAssets() {
	kos.PvrInitDefaults()

	titleImage = kos.PlxTxrLoad("/rd/brkout.png", false, 0)
	fontImage = kos.PlxTxrLoad("/rd/font.png", true, 0)
	fieldImage = kos.PlxTxrLoad("/rd/field.png", false, 0)

	kos.SndStreamInit()
	soundClick = kos.SndSfxLoad("/rd/click.wav")
	soundBounce = kos.SndSfxLoad("/rd/bounce.wav")
	soundHit = kos.SndSfxLoad("/rd/toggled.wav")
	soundLose = kos.SndSfxLoad("/rd/failure.wav")
	soundWin = kos.SndSfxLoad("/rd/success.wav")
}

func readController() {
	buttonsPrev = buttonsNow

	controller := kos.MapleEnumType(0, kos.MAPLE_FUNC_CONTROLLER)
	if controller == nil {
		buttonsNow, stickX = 0, 0
		return
	}

	state := controller.ContState()
	if state == nil {
		buttonsNow, stickX = 0, 0
		return
	}

	buttonsNow = state.Buttons
	stickX = state.Joyx
}

func justPressed(button uint32) bool {
	return (buttonsPrev&button) == 0 && (buttonsNow&button) != 0
}

func isHeld(button uint32) bool {
	return (buttonsNow & button) != 0
}

func getStickMovement() float32 {
	if stickX > -StickDeadzone && stickX < StickDeadzone {
		return 0
	}
	return float32(stickX) / StickScale
}

func setupColors(list uint32) {
	kos.PlxCxtInit()
	kos.PlxCxtTexture(nil)
	kos.PlxCxtCulling(kos.PLX_CULL_NONE)
	kos.PlxCxtSend(int32(list))
}

func setupTexture(list uint32, texture *kos.PlxTexture) {
	kos.PlxCxtInit()
	kos.PlxCxtTexture(texture.Ptr())
	kos.PlxCxtCulling(kos.PLX_CULL_NONE)
	kos.PlxCxtSend(int32(list))
}

func drawRect(x, y, w, h, z float32, color uint32) {
	kos.PlxVertInp(kos.PLX_VERT, x, y+h, z, color)
	kos.PlxVertInp(kos.PLX_VERT, x, y, z, color)
	kos.PlxVertInp(kos.PLX_VERT, x+w, y+h, z, color)
	kos.PlxVertInp(kos.PLX_VERT_EOS, x+w, y, z, color)
}

func drawImage(x, y, w, h, z float32, color uint32, u1, v1, u2, v2 float32) {
	kos.PlxVertIfp(kos.PLX_VERT, x, y+h, z, color, u1, v2)
	kos.PlxVertIfp(kos.PLX_VERT, x, y, z, color, u1, v1)
	kos.PlxVertIfp(kos.PLX_VERT, x+w, y+h, z, color, u2, v2)
	kos.PlxVertIfp(kos.PLX_VERT_EOS, x+w, y, z, color, u2, v1)
}

func drawText(x, y, z float32, color uint32, text string) {
	for i, ch := range text {
		if ch < 32 {
			continue
		}
		col := int(ch) % 16
		row := int(ch) / 16
		u1 := float32(col*charW) / fontSize
		v1 := float32(row*charH) / fontSize
		u2 := u1 + charW/fontSize
		v2 := v1 + charH/fontSize
		drawImage(x+float32(i*charW), y, charW, charH, z, color, u1, v1, u2, v2)
	}
}

func drawNumber(x, y, z float32, color uint32, n int) {
	if n == 0 {
		drawText(x, y, z, color, "0")
		return
	}
	text := ""
	for n > 0 {
		text = string('0'+byte(n%10)) + text
		n /= 10
	}
	drawText(x, y, z, color, text)
}

func playSound(sound kos.SfxHandle)      { kos.SndSfxPlay(sound, 160, 128) }
func playSoundLeft(sound kos.SfxHandle)  { kos.SndSfxPlay(sound, 140, 64) }
func playSoundRight(sound kos.SfxHandle) { kos.SndSfxPlay(sound, 140, 192) }

func abs(x float32) float32 {
	if x < 0 {
		return -x
	}
	return x
}

func showMenu() bool {
	kos.PvrSetBgColor(0, 0, 0)
	choice := 0

	for {
		readController()

		if justPressed(kos.CONT_DPAD_UP) || justPressed(kos.CONT_DPAD_DOWN) {
			choice = 1 - choice
			playSound(soundClick)
		}

		if justPressed(kos.CONT_A) || justPressed(kos.CONT_START) {
			playSound(soundHit)
			fadeOutMenu(choice)
			return choice == 1
		}

		renderMenu(choice, 1.0)
	}
}

func fadeOutMenu(choice int) {
	for frame := 30; frame >= 0; frame-- {
		renderMenu(choice, float32(frame)/30.0)
	}
}

func renderMenu(choice int, alpha float32) {
	kos.PvrWaitReady()
	kos.PvrSceneBegin()

	kos.PvrListBegin(kos.PVR_LIST_OP_POLY)
	if titleImage != nil {
		setupTexture(kos.PVR_LIST_OP_POLY, titleImage)
		tint := kos.PlxPackColor(1, alpha, alpha, alpha)
		drawImage(0, 0, ScreenWidth, ScreenHeight, 100, tint, 0, 0, 640.0/1024.0, 480.0/512.0)
	}
	kos.PvrListFinish()

	kos.PvrListBegin(kos.PVR_LIST_TR_POLY)
	setupColors(kos.PVR_LIST_TR_POLY)
	cursorY := float32(314 + choice*50)
	drawRect(250, cursorY, 20, 20, 200, kos.PlxPackColor(alpha, 1, 1, 1))
	kos.PvrListFinish()

	kos.PvrSceneFinish()
}

func startNewGame() {
	lives = StartingLives
	score = 0
	level = 1
	setupLevel()
}

func setupLevel() {
	paddleX = float32(FieldWidth-PaddleWidth) / 2.0
	resetBall()

	rows := 3 + level
	if rows > BrickRows {
		rows = BrickRows
	}

	bricksLeft = 0
	numColors := len(brickColors)

	for row := 0; row < BrickRows; row++ {
		for col := 0; col < BrickCols; col++ {
			i := row*BrickCols + col
			if row < rows {
				bricks[i] = (row % numColors) + 1
				bricksLeft++
			} else {
				bricks[i] = 0
			}
		}
	}
}

func resetBall() {
	ballX = paddleX + PaddleWidth/2.0 - BallRadius
	ballY = FieldHeight - 50.0

	speed := StartSpeed + float32(level)*SpeedIncrease
	if speed > MaxSpeed {
		speed = MaxSpeed
	}

	ballSpeedX = speed * 0.6
	speedY = -speed
}

func playGame() {
	kos.PvrSetBgColor(0.05, 0.05, 0.1)
	startNewGame()

	for lives > 0 {
		readController()

		if isHeld(kos.CONT_START) && isHeld(kos.CONT_B) {
			return
		}

		updatePaddle()
		updateBall()
		renderGame()
	}
}

func updatePaddle() {
	if isHeld(kos.CONT_DPAD_LEFT) {
		paddleX -= PaddleSpeed
	}
	if isHeld(kos.CONT_DPAD_RIGHT) {
		paddleX += PaddleSpeed
	}

	paddleX += getStickMovement()

	if paddleX < 0 {
		paddleX = 0
	}
	if paddleX > FieldWidth-PaddleWidth {
		paddleX = FieldWidth - PaddleWidth
	}
}

func updateBall() {
	ballX += ballSpeedX
	ballY += speedY

	checkWalls()
	checkPaddle()
	checkBricks()
	checkLost()
	checkWin()
}

func checkWalls() {
	if ballX < 0 {
		ballX = 0
		ballSpeedX = -ballSpeedX
		playSoundLeft(soundBounce)
	}

	if ballX > FieldWidth-BallSize {
		ballX = FieldWidth - BallSize
		ballSpeedX = -ballSpeedX
		playSoundRight(soundBounce)
	}

	if ballY < 0 {
		ballY = 0
		speedY = -speedY
		playSound(soundBounce)
	}
}

func checkPaddle() {
	ballCenterX := ballX + BallRadius
	ballBottom := ballY + BallSize

	if ballBottom < PaddleY || ballY > PaddleY+PaddleHeight {
		return
	}
	if ballCenterX < paddleX || ballCenterX > paddleX+PaddleWidth {
		return
	}

	ballY = PaddleY - BallSize
	speedY = -abs(speedY)

	// bounce angle based on hit position
	hitPoint := (ballCenterX - paddleX) / PaddleWidth
	ballSpeedX = (hitPoint - 0.5) * 8.0

	playSound(soundBounce)
}

func checkBricks() {
	ballCenterX := ballX + BallRadius
	ballCenterY := ballY + BallRadius

	col := int(ballCenterX) / BrickWidth
	row := int(ballCenterY) / BrickHeight

	if col < 0 || col >= BrickCols || row < 0 || row >= BrickRows {
		return
	}

	i := row*BrickCols + col
	if bricks[i] == 0 {
		return
	}

	bricks[i] = 0
	bricksLeft--
	score += PointsPerBrick * level

	// figure out which side we hit
	brickCenterX := float32(col*BrickWidth) + BrickWidth/2.0
	brickCenterY := float32(row*BrickHeight) + BrickHeight/2.0

	horizDist := abs(ballCenterX-brickCenterX) / BrickWidth
	vertDist := abs(ballCenterY-brickCenterY) / BrickHeight

	if horizDist > vertDist {
		ballSpeedX = -ballSpeedX
	} else {
		speedY = -speedY
	}

	playSound(soundHit)
}

func checkLost() {
	if ballY <= FieldHeight {
		return
	}

	lives--
	playSound(soundLose)

	if lives <= 0 {
		showGameOver()
		return
	}

	resetBall()
	waitFrames(60)
}

func checkWin() {
	if bricksLeft > 0 {
		return
	}

	playSound(soundWin)
	level++
	setupLevel()
	waitFrames(60)
}

func waitFrames(n int) {
	for i := 0; i < n; i++ {
		renderGame()
	}
}

func showGameOver() {
	bg := kos.PlxPackColor(1, 0.1, 0.1, 0.15)

	for frame := 0; frame < 180; frame++ {
		kos.PvrWaitReady()
		kos.PvrSceneBegin()

		kos.PvrListBegin(kos.PVR_LIST_OP_POLY)
		setupColors(kos.PVR_LIST_OP_POLY)
		drawRect(0, 0, ScreenWidth, ScreenHeight, 1, bg)
		kos.PvrListFinish()

		kos.PvrListBegin(kos.PVR_LIST_TR_POLY)
		if fontImage != nil {
			setupTexture(kos.PVR_LIST_TR_POLY, fontImage)
			drawText(220, 180, 100, white(), "GAME OVER")
			drawText(220, 230, 100, gray(), "Score:")
			drawNumber(320, 230, 100, white(), score)
		}
		kos.PvrListFinish()

		kos.PvrSceneFinish()

		readController()
		if justPressed(kos.CONT_A) {
			return
		}
	}
}

func renderGame() {
	kos.PvrWaitReady()
	kos.PvrSceneBegin()

	kos.PvrListBegin(kos.PVR_LIST_OP_POLY)
	drawBackground()
	drawField()
	drawWalls()
	drawBricks()
	drawPaddle()
	drawBall()
	kos.PvrListFinish()

	kos.PvrListBegin(kos.PVR_LIST_TR_POLY)
	drawHUD()
	kos.PvrListFinish()

	kos.PvrSceneFinish()
}

func drawBackground() {
	setupColors(kos.PVR_LIST_OP_POLY)
	drawRect(0, 0, ScreenWidth, ScreenHeight, 1, darkBlue())
}

func drawField() {
	if fieldImage == nil {
		return
	}
	setupTexture(kos.PVR_LIST_OP_POLY, fieldImage)
	drawImage(FieldLeft, FieldTop, FieldWidth, FieldHeight, 50, white(),
		0, 0, float32(FieldWidth)/512.0, float32(FieldHeight)/512.0)
}

func drawWalls() {
	setupColors(kos.PVR_LIST_OP_POLY)
	c := wallBlue()
	drawRect(FieldLeft-5, FieldTop, 5, FieldHeight, 100, c)
	drawRect(FieldLeft+FieldWidth, FieldTop, 5, FieldHeight, 100, c)
	drawRect(FieldLeft-5, FieldTop-5, FieldWidth+10, 5, 100, c)
}

func drawBricks() {
	numColors := len(brickColors)

	for row := 0; row < BrickRows; row++ {
		for col := 0; col < BrickCols; col++ {
			colorIndex := bricks[row*BrickCols+col]
			if colorIndex == 0 {
				continue
			}

			x := float32(FieldLeft + col*BrickWidth)
			y := float32(FieldTop + row*BrickHeight)

			drawRect(x+2, y+2, BrickWidth-2, BrickHeight-2, 99, shadow())
			drawRect(x, y, BrickWidth-2, BrickHeight-2, 100, brickColors[(colorIndex-1)%numColors])
		}
	}
}

func drawPaddle() {
	x := float32(FieldLeft) + paddleX
	y := float32(FieldTop + PaddleY)

	drawRect(x+3, y+3, PaddleWidth, PaddleHeight, 99, shadow())
	drawRect(x, y, PaddleWidth, PaddleHeight, 100, paddleBlue())
}

func drawBall() {
	x := float32(FieldLeft) + ballX
	y := float32(FieldTop) + ballY
	drawRect(x, y, BallSize, BallSize, 100, white())
}

func drawHUD() {
	if fontImage == nil {
		return
	}

	setupTexture(kos.PVR_LIST_TR_POLY, fontImage)

	x := float32(FieldLeft + FieldWidth + 15)

	drawText(x, 50, 200, white(), "BRKOUT")

	drawText(x, 100, 200, gray(), "Level")
	drawNumber(x, 125, 200, white(), level)

	drawText(x, 170, 200, gray(), "Score")
	drawNumber(x, 195, 200, white(), score)

	drawText(x, 250, 200, gray(), "Lives")
	drawNumber(x, 275, 200, white(), lives)
}

func main() {
	loadAssets()

	for {
		if showMenu() {
			return
		}
		playGame()
	}
}
