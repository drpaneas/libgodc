// types.go - Game entity types
package main

// Ball represents the game ball
type Ball struct {
	X, Y   float32 // Position
	DX, DY float32 // Direction vector (normalized)
	Speed  int     // Current speed (increases with each hit)
}

// Bat represents a player paddle
type Bat struct {
	X, Y    float32 // Position (center)
	Score   int     // Current score
	Timer   int     // Animation timer (for glow effects)
	IsHuman bool    // True if controlled by player, false for AI
}

