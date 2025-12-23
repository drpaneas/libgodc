// Boing! - Classic Pong Clone for Sega Dreamcast
//
// A complete port inspired by Code the Classics' "Boing!" game.
//
// Features:
//   - Menu with 1P/2P selection
//   - Smooth ball physics with speed increase
//   - AI opponent with weighted targeting
//   - Animated sprites and effects
//   - Score display
//   - First to 10 wins!
//
// Controls:
//
//	Menu:     D-Pad Up/Down to select, START/A to begin
//	Player 1: D-Pad Up/Down
//	Player 2: D-Pad Up/Down on controller 2 - in 2P mode
//	START:    Pause during game
//
// File Structure:
//   - main.go      : Entry point and main loop
//   - constants.go : Game constants and configuration
//   - types.go     : Ball and Bat struct definitions
//   - state.go     : Global game state variables
//   - input.go     : Controller input handling
//   - math.go      : Math utility functions
//   - game.go      : Game logic and update functions
//   - render.go    : All rendering/drawing functions
//   - assets.go    : Texture and sound loading

package main

import "kos"

func main() {
	println("=== BOING! - Pong Clone ===")

	// Initialize PVR (graphics)
	kos.PvrInitDefaults()
	kos.PvrSetBgColor(0.05, 0.02, 0.12) // Dark blue-purple space background

	// Initialize colors (must be after PVR init)
	InitColors()

	// Load assets
	LoadSounds()
	LoadTextures()

	// Start in attract mode
	InitGame(false, false)

	// Print controls
	println("Controls:")
	println("  Menu: D-Pad Up/Down, START/A to begin")
	println("  P1: D-Pad Up/Down")
	println("  P2: Controller 2 D-Pad Up/Down")
	println("First to 10 wins!")

	// Main game loop
	for {
		ReadInput()

		switch GameState {
		case STATE_MENU:
			UpdateMenu()
			DrawMenu()
		case STATE_PLAY:
			UpdatePlay()
			Render()
		case STATE_GAME_OVER:
			UpdateGameOver()
			DrawGameOver()
		}
	}
}
