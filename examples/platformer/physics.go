package main

var physics = Physics{
	walkMax:      1.0 * ScaleH,
	runMax:       1.567 * ScaleH,
	accel:        0.077 * ScaleH,
	friction:     0.967,
	skidDecel:    0.1 * ScaleH,
	runThreshold: 1.2 * ScaleH,

	jumpVelSlow: 4.4 * ScaleV,
	jumpVelFast: 5.35 * ScaleV,

	gravityHold: 0.147 * ScaleV,
	gravityFall: 0.397 * ScaleV,

	terminalVel: 5.0 * ScaleV,
	jumpHoldMax: 25,
}

func updatePlayer() {
	updatePlayerMovement()
	updatePlayerJump()
}
