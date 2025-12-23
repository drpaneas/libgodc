// constants.go - Game constants and configuration
package main

// Screen dimensions
const (
	WIDTH       = 640
	HEIGHT      = 480
	HALF_WIDTH  = WIDTH / 2
	HALF_HEIGHT = HEIGHT / 2
)

// Game parameters
const (
	PLAYER_SPEED     = 6
	MAX_AI_SPEED     = 6  // Same as player (matches original Python)
	BALL_START_SPEED = 5  // Normal speed
	WIN_SCORE        = 10 // First to 10 wins
)

// Bat dimensions
const (
	BAT_WIDTH   = 12         // Lightsaber blade width
	BAT_HEIGHT  = 120        // Matches physics hitbox (diffY -60 to +60)
	BAT_LEFT_X  = 40
	BAT_RIGHT_X = 600
)

// Ball dimensions
const (
	BALL_SIZE = 14
)

// Arena boundaries
const (
	ARENA_TOP    = 40
	ARENA_BOTTOM = 440
)

// Collision threshold (distance from center where bat can hit)
const COLLISION_X = 280

// Game states
const (
	STATE_MENU      = 0
	STATE_PLAY      = 1
	STATE_GAME_OVER = 2
)

