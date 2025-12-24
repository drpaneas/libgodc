// Platformer - physics demo for libgodc
//
// Controls:
//   D-Pad:  Move left/right
//   A:      Jump (hold for higher)
//   X:      Run (hold while moving)
//   START:  Pause / Exit

package main

import (
	"kos"
)

func printInstructions() {
	println("--- Platformer Demo ---")
	println("")
	println("Controls:")
	println("  D-Pad: Move")
	println("  A: Jump (hold for higher)")
	println("  X: Run (enables higher jumps)")
	println("  START: Pause / Exit")
	println("")
}

func main() {
	initGraphics()
	printInstructions()
	startGame()

	for {
		readInput()

		switch gameState {
		case StatePlaying:
			if justPressed(kos.CONT_START) {
				gameState = StatePaused
				println("[PAUSED] Press START to resume")
				continue
			}
			updatePlayer()

		case StatePaused:
			if justPressed(kos.CONT_START) {
				gameState = StatePlaying
				println("[RESUMED]")
			}
		}

		render()

		if gameState == StatePaused && isHeld(kos.CONT_START) && !justPressed(kos.CONT_START) {
			break
		}
	}

	println("Done!")
}
