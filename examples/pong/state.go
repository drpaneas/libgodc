// state.go - Global game state variables
package main

import "kos"

// Game state
var (
	GameState  = STATE_MENU
	NumPlayers = 1
	Bats       [2]Bat
	TheBall    Ball
	AIOffset   int
)

// Input state
var (
	ButtonsNow  uint32
	ButtonsPrev uint32
)

// Effect state
var (
	ImpactX, ImpactY float32
	ImpactTimer      int
	GoalEffectTimer  int
	GoalEffectPlayer int // Which player conceded (0 or 1)
)

// Star field for animated background
const (
	NUM_STARS_FAR    = 60  // Slow, dim stars (distant)
	NUM_STARS_MID    = 40  // Medium speed stars
	NUM_STARS_NEAR   = 25  // Fast, bright stars (close)
	NUM_STARS_TOTAL  = NUM_STARS_FAR + NUM_STARS_MID + NUM_STARS_NEAR
)

type Star struct {
	X, Y   float32
	Speed  float32
	Size   float32
	Bright uint8
}

var Stars [NUM_STARS_TOTAL]Star
var StarsInitialized bool
var StarSpeedMult float32 = 1.0 // Current star speed multiplier (for dimming)

// === ADVANCED PARTICLE SYSTEM ===
const (
	MAX_PARTICLES     = 200  // Maximum particles at once
	MAX_TRAIL_POINTS  = 16   // Motion blur segments
	MAX_STREAK_LINES  = 30   // Hyperspace streaks
	MAX_BALL_SPARKS   = 20   // Sparks trailing the ball
)

type Particle struct {
	X, Y       float32
	VX, VY     float32
	Life       int     // Frames remaining
	MaxLife    int     // Starting life (for alpha calc)
	Size       float32
	R, G, B    float32 // Color
}

type TrailPoint struct {
	X, Y  float32
	Age   int
}

type BallSpark struct {
	X, Y     float32
	VX, VY   float32
	Life     int
	Size     float32
}

type StreakLine struct {
	X, Y       float32
	Length     float32
	Speed      float32
	Brightness float32
}

var (
	Particles      [200]Particle
	ParticleCount  int
	
	BallTrail      [MAX_TRAIL_POINTS]TrailPoint
	TrailIndex     int
	
	BallSparks     [MAX_BALL_SPARKS]BallSpark
	BallGlowPulse  float32  // Pulsing glow intensity
	
	StreakLines    [MAX_STREAK_LINES]StreakLine
	StreaksActive  bool
	
	ScreenShakeX   float32
	ScreenShakeY   float32
	ScreenShakeTime int
	
	LightningTimer int  // For flickering effect
)

// Colors (computed at runtime after PVR init)
var (
	ColorWhite  uint32
	ColorGreen  uint32
	ColorBlue   uint32
	ColorYellow uint32
	ColorGray   uint32
	ColorDark   uint32
)

// Textures
var (
	TexBall    *kos.PlxTexture
	TexBat0    [3]*kos.PlxTexture // bat00, bat01, bat02 (normal, hit1, hit2) - green lightsaber
	TexBat1    [3]*kos.PlxTexture // bat10, bat11, bat12 (normal, hit1, hit2) - red lightsaber
	TexImpact  [5]*kos.PlxTexture // impact0-4 (animated hit effect)
	TexEffect0 *kos.PlxTexture    // Goal effect for player 0 (left)
	TexEffect1 *kos.PlxTexture    // Goal effect for player 1 (right)
)

// Sound effects
var (
	SndBounce      [5]kos.SfxHandle
	SndBounceSynth kos.SfxHandle
	SndHit         [5]kos.SfxHandle
	SndHitSlow     kos.SfxHandle
	SndHitMedium   kos.SfxHandle
	SndHitFast     kos.SfxHandle
	SndHitVeryFast kos.SfxHandle
	SndHitSynth    kos.SfxHandle
	SndScoreGoal   kos.SfxHandle
	SndUp          kos.SfxHandle
	SndDown        kos.SfxHandle
)

