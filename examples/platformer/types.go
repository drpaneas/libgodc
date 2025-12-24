package main

type Player struct {
	x, y        float32
	vx, vy      float32
	size        float32
	facingRight bool
	onGround    bool
	jumpHeld    bool
	jumpFrames  int
	isSkidding  bool
	isRunning   bool
}

type Physics struct {
	walkMax      float32
	runMax       float32
	accel        float32
	friction     float32
	skidDecel    float32
	runThreshold float32

	jumpVelSlow float32
	jumpVelFast float32

	gravityHold float32
	gravityFall float32

	terminalVel float32
	jumpHoldMax int
}

type Platform struct {
	x1, y1 float32
	x2, y2 float32
}

