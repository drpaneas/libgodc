package main

const (
	PlayerSize      float32 = 40
	PlayerStartX    float32 = 320
	PlayerStartY    float32 = 375
	EyeSize         float32 = 8
	EyeOffsetY      float32 = 8
	EyeOffsetRight  float32 = 12
	EyeOffsetLeft   float32 = 4
	SpeedLineSize   float32 = 4
	SpeedLineGap    float32 = 10
	SpeedLineOffset float32 = 8
	SpeedLineCount  int     = 3
	IndicatorOffset float32 = 8
	IndicatorLength float32 = 8
	IndicatorWidth  float32 = 3
)

const (
	VelocityStopThreshold float32 = 0.25
	SpeedLineThreshold    float32 = 10
	RisingThreshold       float32 = -5
	FallingThreshold      float32 = 10
	HighVelocityRatio     float32 = 0.7
)

var player = Player{
	x:           PlayerStartX,
	y:           PlayerStartY,
	vx:          0,
	vy:          0,
	size:        PlayerSize,
	facingRight: true,
	onGround:    true,
	jumpHeld:    false,
	jumpFrames:  0,
	isSkidding:  false,
	isRunning:   false,
}

func startGame() {
	player.x = PlayerStartX
	player.y = PlayerStartY
	player.vx = 0
	player.vy = 0
	player.facingRight = true
	player.onGround = true
	player.jumpHeld = false
	player.jumpFrames = 0
	player.isSkidding = false
	player.isRunning = false
	gameState = StatePlaying
}
