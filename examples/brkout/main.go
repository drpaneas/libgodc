// BRKOUT - A Breakout Clone for Sega Dreamcast
//
// This example demonstrates how to build a complete game using Go on the
// Dreamcast. It showcases:
//   - Loading textures and sounds from the romdisk filesystem
//   - Reading controller input (D-pad and analog stick)
//   - Rendering 2D graphics using the PowerVR hardware
//   - Simple collision detection and game physics
//   - Game state management with menus and screens
//
// Controls:
//   D-Pad / Analog Stick - Move paddle
//   A or START          - Select menu option
//   START + B           - Return to menu during gameplay

package main

import "kos"

// =============================================================================
// GAME CONFIGURATION
// =============================================================================
//
// All game parameters are defined as constants, making them easy to tweak.
// Try changing these values to see how they affect gameplay!

// Screen size (Dreamcast outputs 640x480 by default)
const (
	ScreenWidth  = 640
	ScreenHeight = 480
)

// The playing field is where the action happens
const (
	FieldLeft   = 10  // Pixels from left edge
	FieldTop    = 40  // Pixels from top edge
	FieldWidth  = 500 // Width in pixels
	FieldHeight = 420 // Height in pixels
)

// Brick grid - the targets to destroy
const (
	BrickCols   = 20 // Columns of bricks
	BrickRows   = 15 // Maximum rows of bricks
	BrickWidth  = 25 // Width of each brick
	BrickHeight = 12 // Height of each brick
)

// Paddle - the player's bat
const (
	PaddleWidth  = 70               // Width in pixels
	PaddleHeight = 10               // Height in pixels
	PaddleSpeed  = 6.0              // Movement speed
	PaddleY      = FieldHeight - 30 // Distance from field bottom
)

// Ball physics
const (
	BallSize      = 12           // Ball diameter
	BallRadius    = BallSize / 2 // Ball radius
	StartSpeed    = 3.0          // Initial ball speed
	MaxSpeed      = 7.0          // Maximum ball speed
	SpeedIncrease = 0.5          // Speed boost per level
)

// Input sensitivity
const (
	StickDeadzone = 15   // Ignore small joystick movements
	StickScale    = 18.0 // How fast joystick moves paddle
)

// Game rules
const (
	StartingLives  = 3  // Lives at game start
	PointsPerBrick = 10 // Base score for each brick
)

// =============================================================================
// COLOR PALETTE
// =============================================================================
//
// Colors are packed into 32-bit values: Alpha, Red, Green, Blue (0.0 to 1.0)
// We use helper functions to make the code more readable.

func white() uint32      { return kos.PlxPackColor(1, 1, 1, 1) }
func gray() uint32       { return kos.PlxPackColor(1, 0.5, 0.5, 0.5) }
func darkBlue() uint32   { return kos.PlxPackColor(1, 0.05, 0.05, 0.1) }
func wallBlue() uint32   { return kos.PlxPackColor(1, 0.4, 0.4, 0.6) }
func paddleBlue() uint32 { return kos.PlxPackColor(1, 0.4, 0.4, 0.7) }
func shadow() uint32     { return kos.PlxPackColor(0.7, 0, 0, 0) }

// Rainbow colors for bricks - each row gets a different color!
var brickColors = []uint32{
	kos.PlxPackColor(1, 1.0, 0.2, 0.2), // 🔴 Red
	kos.PlxPackColor(1, 1.0, 0.6, 0.2), // 🟠 Orange
	kos.PlxPackColor(1, 1.0, 1.0, 0.2), // 🟡 Yellow
	kos.PlxPackColor(1, 0.2, 1.0, 0.2), // 🟢 Green
	kos.PlxPackColor(1, 0.2, 1.0, 1.0), // 🩵 Cyan
	kos.PlxPackColor(1, 0.2, 0.2, 1.0), // 🔵 Blue
	kos.PlxPackColor(1, 0.8, 0.2, 1.0), // 🟣 Purple
}

// =============================================================================
// GAME ASSETS
// =============================================================================
//
// Textures and sounds loaded from the romdisk (virtual filesystem compiled
// into the game executable).

var (
	titleImage *kos.PlxTexture // Title screen background
	fontImage  *kos.PlxTexture // Bitmap font for text
	fieldImage *kos.PlxTexture // Playing field background

	soundClick  kos.SfxHandle // Menu navigation
	soundBounce kos.SfxHandle // Ball bouncing
	soundHit    kos.SfxHandle // Brick destroyed
	soundLose   kos.SfxHandle // Lost a life
	soundWin    kos.SfxHandle // Level complete
)

func loadAssets() {
	// Initialize the PowerVR graphics chip
	kos.PvrInitDefaults()

	// Load images from the /rd (romdisk) filesystem
	titleImage = kos.PlxTxrLoad("/rd/brkout.png", false, 0)
	fontImage = kos.PlxTxrLoad("/rd/font.png", true, 0) // true = has transparency
	fieldImage = kos.PlxTxrLoad("/rd/field.png", false, 0)

	// Initialize sound system and load effects
	kos.SndStreamInit()
	soundClick = kos.SndSfxLoad("/rd/click.wav")
	soundBounce = kos.SndSfxLoad("/rd/bounce.wav")
	soundHit = kos.SndSfxLoad("/rd/toggled.wav")
	soundLose = kos.SndSfxLoad("/rd/failure.wav")
	soundWin = kos.SndSfxLoad("/rd/success.wav")
}

// =============================================================================
// CONTROLLER INPUT
// =============================================================================
//
// The Dreamcast controller has a D-pad, analog stick, and buttons.
// We track the previous frame's buttons to detect new presses.

var (
	buttonsNow  uint32 // Buttons held this frame
	buttonsPrev uint32 // Buttons held last frame
	stickX      int32  // Analog stick X position (-128 to 127)
)

func readController() {
	buttonsPrev = buttonsNow

	// Find the first controller plugged in
	controller := kos.MapleEnumType(0, kos.MAPLE_FUNC_CONTROLLER)
	if controller == nil {
		buttonsNow, stickX = 0, 0
		return
	}

	// Read its current state
	state := controller.ContState()
	if state == nil {
		buttonsNow, stickX = 0, 0
		return
	}

	buttonsNow = state.Buttons
	stickX = state.Joyx
}

// justPressed returns true only on the frame when a button is first pressed
func justPressed(button uint32) bool {
	wasUp := (buttonsPrev & button) == 0
	isDown := (buttonsNow & button) != 0
	return wasUp && isDown
}

// isHeld returns true every frame while a button is held down
func isHeld(button uint32) bool {
	return (buttonsNow & button) != 0
}

// getStickMovement returns paddle movement from the analog stick
func getStickMovement() float32 {
	// Ignore tiny movements (deadzone prevents drift)
	if stickX > -StickDeadzone && stickX < StickDeadzone {
		return 0
	}
	return float32(stickX) / StickScale
}

// =============================================================================
// GAME STATE
// =============================================================================
//
// All the variables that track what's happening in the game.

var (
	// Paddle position (X coordinate within the field)
	paddleX float32

	// Ball position and velocity
	ballX, ballY       float32
	ballSpeedX, speedY float32

	// Brick grid: 0 = empty, 1-7 = brick color
	bricks     [BrickCols * BrickRows]int
	bricksLeft int

	// Player progress
	lives int
	score int
	level int
)

// =============================================================================
// DRAWING HELPERS
// =============================================================================
//
// The Dreamcast's PowerVR chip renders 3D polygons. For 2D games, we draw
// rectangles (quads) at a fixed Z depth. The PLX library makes this easy!

// setupColors prepares for drawing solid-colored shapes
func setupColors(list uint32) {
	kos.PlxCxtInit()
	kos.PlxCxtTexture(nil) // No texture, just colors
	kos.PlxCxtCulling(kos.PLX_CULL_NONE)
	kos.PlxCxtSend(int32(list))
}

// setupTexture prepares for drawing textured shapes
func setupTexture(list uint32, texture *kos.PlxTexture) {
	kos.PlxCxtInit()
	kos.PlxCxtTexture(texture.Ptr())
	kos.PlxCxtCulling(kos.PLX_CULL_NONE)
	kos.PlxCxtSend(int32(list))
}

// drawRect draws a solid colored rectangle
func drawRect(x, y, w, h, z float32, color uint32) {
	// A quad needs 4 vertices, last one marked as "end of strip"
	kos.PlxVertInp(kos.PLX_VERT, x, y+h, z, color)
	kos.PlxVertInp(kos.PLX_VERT, x, y, z, color)
	kos.PlxVertInp(kos.PLX_VERT, x+w, y+h, z, color)
	kos.PlxVertInp(kos.PLX_VERT_EOS, x+w, y, z, color)
}

// drawImage draws a textured rectangle with UV coordinates
func drawImage(x, y, w, h, z float32, color uint32, u1, v1, u2, v2 float32) {
	kos.PlxVertIfp(kos.PLX_VERT, x, y+h, z, color, u1, v2)
	kos.PlxVertIfp(kos.PLX_VERT, x, y, z, color, u1, v1)
	kos.PlxVertIfp(kos.PLX_VERT, x+w, y+h, z, color, u2, v2)
	kos.PlxVertIfp(kos.PLX_VERT_EOS, x+w, y, z, color, u2, v1)
}

// =============================================================================
// TEXT RENDERING
// =============================================================================
//
// We use a bitmap font stored in a texture. Each character is 12x24 pixels,
// arranged in a 16x16 grid (256 characters total, ASCII order).

const (
	charW    = 12    // Character width
	charH    = 24    // Character height
	fontSize = 256.0 // Texture size
)

// drawText draws a string at the given position
func drawText(x, y, z float32, color uint32, text string) {
	for i, ch := range text {
		if ch < 32 {
			continue // Skip control characters
		}
		// Calculate UV coordinates for this character
		col := int(ch) % 16
		row := int(ch) / 16
		u1 := float32(col*charW) / fontSize
		v1 := float32(row*charH) / fontSize
		u2 := u1 + charW/fontSize
		v2 := v1 + charH/fontSize
		// Draw the character
		drawImage(x+float32(i*charW), y, charW, charH, z, color, u1, v1, u2, v2)
	}
}

// drawNumber draws an integer as text
func drawNumber(x, y, z float32, color uint32, n int) {
	if n == 0 {
		drawText(x, y, z, color, "0")
		return
	}
	// Build string from digits (right to left)
	text := ""
	for n > 0 {
		digit := byte(n % 10)
		text = string('0'+digit) + text
		n = n / 10
	}
	drawText(x, y, z, color, text)
}

// =============================================================================
// SOUND EFFECTS
// =============================================================================
//
// Simple wrappers for playing sounds with panning (left/center/right)

func playSound(sound kos.SfxHandle)      { kos.SndSfxPlay(sound, 160, 128) }
func playSoundLeft(sound kos.SfxHandle)  { kos.SndSfxPlay(sound, 140, 64) }
func playSoundRight(sound kos.SfxHandle) { kos.SndSfxPlay(sound, 140, 192) }

// =============================================================================
// MATH HELPERS
// =============================================================================

func abs(x float32) float32 {
	if x < 0 {
		return -x
	}
	return x
}

// =============================================================================
// TITLE MENU
// =============================================================================

func showMenu() bool {
	kos.PvrSetBgColor(0, 0, 0) // Black background
	choice := 0                // 0 = Play, 1 = Quit

	for {
		readController()

		// Navigate with D-pad
		if justPressed(kos.CONT_DPAD_UP) || justPressed(kos.CONT_DPAD_DOWN) {
			choice = 1 - choice // Toggle between 0 and 1
			playSound(soundClick)
		}

		// Select with A or START
		if justPressed(kos.CONT_A) || justPressed(kos.CONT_START) {
			playSound(soundHit)
			fadeOutMenu(choice)
			return choice == 1 // true = quit
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

	// Draw title image (opaque layer)
	kos.PvrListBegin(kos.PVR_LIST_OP_POLY)
	if titleImage != nil {
		setupTexture(kos.PVR_LIST_OP_POLY, titleImage)
		tint := kos.PlxPackColor(1, alpha, alpha, alpha)
		drawImage(0, 0, ScreenWidth, ScreenHeight, 100, tint, 0, 0, 640.0/1024.0, 480.0/512.0)
	}
	kos.PvrListFinish()

	// Draw menu cursor (translucent layer)
	kos.PvrListBegin(kos.PVR_LIST_TR_POLY)
	setupColors(kos.PVR_LIST_TR_POLY)
	cursorY := float32(314 + choice*50)
	drawRect(250, cursorY, 20, 20, 200, kos.PlxPackColor(alpha, 1, 1, 1))
	kos.PvrListFinish()

	kos.PvrSceneFinish()
}

// =============================================================================
// GAME INITIALIZATION
// =============================================================================

func startNewGame() {
	lives = StartingLives
	score = 0
	level = 1
	setupLevel()
}

func setupLevel() {
	// Center the paddle
	paddleX = float32(FieldWidth-PaddleWidth) / 2.0

	// Reset the ball
	resetBall()

	// Fill brick grid (more rows at higher levels)
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
				bricks[i] = (row % numColors) + 1 // Color index 1-7
				bricksLeft++
			} else {
				bricks[i] = 0 // Empty
			}
		}
	}
}

func resetBall() {
	// Position ball above paddle
	ballX = paddleX + PaddleWidth/2.0 - BallRadius
	ballY = FieldHeight - 50.0

	// Calculate speed for current level
	speed := StartSpeed + float32(level)*SpeedIncrease
	if speed > MaxSpeed {
		speed = MaxSpeed
	}

	// Launch at an angle
	ballSpeedX = speed * 0.6
	speedY = -speed
}

// =============================================================================
// GAME LOOP
// =============================================================================

func playGame() {
	kos.PvrSetBgColor(0.05, 0.05, 0.1) // Dark blue background
	startNewGame()

	for lives > 0 {
		readController()

		// Return to menu with START+B
		if isHeld(kos.CONT_START) && isHeld(kos.CONT_B) {
			return
		}

		updatePaddle()
		updateBall()
		renderGame()
	}
}

// =============================================================================
// PADDLE MOVEMENT
// =============================================================================

func updatePaddle() {
	// D-pad control
	if isHeld(kos.CONT_DPAD_LEFT) {
		paddleX -= PaddleSpeed
	}
	if isHeld(kos.CONT_DPAD_RIGHT) {
		paddleX += PaddleSpeed
	}

	// Analog stick control
	paddleX += getStickMovement()

	// Keep paddle inside the field
	if paddleX < 0 {
		paddleX = 0
	}
	if paddleX > FieldWidth-PaddleWidth {
		paddleX = FieldWidth - PaddleWidth
	}
}

// =============================================================================
// BALL PHYSICS
// =============================================================================

func updateBall() {
	// Move ball
	ballX += ballSpeedX
	ballY += speedY

	// Check all collisions
	checkWalls()
	checkPaddle()
	checkBricks()
	checkLost()
	checkWin()
}

func checkWalls() {
	// Left wall
	if ballX < 0 {
		ballX = 0
		ballSpeedX = -ballSpeedX
		playSoundLeft(soundBounce)
	}

	// Right wall
	if ballX > FieldWidth-BallSize {
		ballX = FieldWidth - BallSize
		ballSpeedX = -ballSpeedX
		playSoundRight(soundBounce)
	}

	// Top wall
	if ballY < 0 {
		ballY = 0
		speedY = -speedY
		playSound(soundBounce)
	}
}

func checkPaddle() {
	// Ball center coordinates
	ballCenterX := ballX + BallRadius
	ballBottom := ballY + BallSize

	// Is ball at paddle height?
	if ballBottom < PaddleY || ballY > PaddleY+PaddleHeight {
		return
	}

	// Is ball above paddle?
	if ballCenterX < paddleX || ballCenterX > paddleX+PaddleWidth {
		return
	}

	// Bounce!
	ballY = PaddleY - BallSize
	speedY = -abs(speedY)

	// Angle depends on where ball hit the paddle
	// Hit left = go left, hit right = go right
	hitPoint := (ballCenterX - paddleX) / PaddleWidth // 0.0 to 1.0
	ballSpeedX = (hitPoint - 0.5) * 8.0               // -4.0 to +4.0

	playSound(soundBounce)
}

func checkBricks() {
	// Ball center coordinates
	ballCenterX := ballX + BallRadius
	ballCenterY := ballY + BallRadius

	// Which brick is the ball in?
	col := int(ballCenterX) / BrickWidth
	row := int(ballCenterY) / BrickHeight

	// Out of bounds?
	if col < 0 || col >= BrickCols || row < 0 || row >= BrickRows {
		return
	}

	// Is there a brick here?
	i := row*BrickCols + col
	if bricks[i] == 0 {
		return
	}

	// Destroy the brick!
	bricks[i] = 0
	bricksLeft--
	score += PointsPerBrick * level

	// Bounce off brick (figure out which side we hit)
	brickCenterX := float32(col*BrickWidth) + BrickWidth/2.0
	brickCenterY := float32(row*BrickHeight) + BrickHeight/2.0

	horizDist := abs(ballCenterX-brickCenterX) / BrickWidth
	vertDist := abs(ballCenterY-brickCenterY) / BrickHeight

	if horizDist > vertDist {
		ballSpeedX = -ballSpeedX // Hit side
	} else {
		speedY = -speedY // Hit top/bottom
	}

	playSound(soundHit)
}

func checkLost() {
	// Ball fell below the field?
	if ballY <= FieldHeight {
		return
	}

	lives--
	playSound(soundLose)

	if lives <= 0 {
		showGameOver()
		return
	}

	// Continue with new ball
	resetBall()
	waitFrames(60) // Brief pause
}

func checkWin() {
	// All bricks destroyed?
	if bricksLeft > 0 {
		return
	}

	playSound(soundWin)
	level++
	setupLevel()
	waitFrames(60) // Brief pause
}

func waitFrames(n int) {
	for i := 0; i < n; i++ {
		renderGame()
	}
}

// =============================================================================
// GAME OVER SCREEN
// =============================================================================

func showGameOver() {
	bg := kos.PlxPackColor(1, 0.1, 0.1, 0.15)

	for frame := 0; frame < 180; frame++ { // 3 seconds
		kos.PvrWaitReady()
		kos.PvrSceneBegin()

		// Background
		kos.PvrListBegin(kos.PVR_LIST_OP_POLY)
		setupColors(kos.PVR_LIST_OP_POLY)
		drawRect(0, 0, ScreenWidth, ScreenHeight, 1, bg)
		kos.PvrListFinish()

		// Text
		kos.PvrListBegin(kos.PVR_LIST_TR_POLY)
		if fontImage != nil {
			setupTexture(kos.PVR_LIST_TR_POLY, fontImage)
			drawText(220, 180, 100, white(), "GAME OVER")
			drawText(220, 230, 100, gray(), "Score:")
			drawNumber(320, 230, 100, white(), score)
		}
		kos.PvrListFinish()

		kos.PvrSceneFinish()

		// Skip with A button
		readController()
		if justPressed(kos.CONT_A) {
			return
		}
	}
}

// =============================================================================
// GAME RENDERING
// =============================================================================

func renderGame() {
	kos.PvrWaitReady()
	kos.PvrSceneBegin()

	// === OPAQUE LAYER ===
	// Draw solid objects that don't need transparency
	kos.PvrListBegin(kos.PVR_LIST_OP_POLY)
	drawBackground()
	drawField()
	drawWalls()
	drawBricks()
	drawPaddle()
	drawBall()
	kos.PvrListFinish()

	// === TRANSLUCENT LAYER ===
	// Draw text (font has transparent background)
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
	color := wallBlue()
	drawRect(FieldLeft-5, FieldTop, 5, FieldHeight, 100, color)          // Left
	drawRect(FieldLeft+FieldWidth, FieldTop, 5, FieldHeight, 100, color) // Right
	drawRect(FieldLeft-5, FieldTop-5, FieldWidth+10, 5, 100, color)      // Top
}

func drawBricks() {
	numColors := len(brickColors)

	for row := 0; row < BrickRows; row++ {
		for col := 0; col < BrickCols; col++ {
			colorIndex := bricks[row*BrickCols+col]
			if colorIndex == 0 {
				continue // Empty space
			}

			// Calculate screen position
			x := float32(FieldLeft + col*BrickWidth)
			y := float32(FieldTop + row*BrickHeight)

			// Draw shadow first (slightly offset)
			drawRect(x+2, y+2, BrickWidth-2, BrickHeight-2, 99, shadow())

			// Draw brick on top
			color := brickColors[(colorIndex-1)%numColors]
			drawRect(x, y, BrickWidth-2, BrickHeight-2, 100, color)
		}
	}
}

func drawPaddle() {
	x := float32(FieldLeft) + paddleX
	y := float32(FieldTop + PaddleY)

	// Shadow
	drawRect(x+3, y+3, PaddleWidth, PaddleHeight, 99, shadow())
	// Paddle
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

	// Position HUD to the right of the playing field
	x := float32(FieldLeft + FieldWidth + 15)

	drawText(x, 50, 200, white(), "BRKOUT")

	drawText(x, 100, 200, gray(), "Level")
	drawNumber(x, 125, 200, white(), level)

	drawText(x, 170, 200, gray(), "Score")
	drawNumber(x, 195, 200, white(), score)

	drawText(x, 250, 200, gray(), "Lives")
	drawNumber(x, 275, 200, white(), lives)
}

// =============================================================================
// MAIN ENTRY POINT
// =============================================================================

func main() {
	// Load all game assets (textures, sounds)
	loadAssets()

	// Main game loop - keeps returning to menu after each game
	for {
		if showMenu() {
			return // User chose "Quit"
		}
		playGame()
	}
}
