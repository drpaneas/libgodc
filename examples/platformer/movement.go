package main

import "kos"

func updatePlayerMovement() {
	left := isHeld(kos.CONT_DPAD_LEFT)
	right := isHeld(kos.CONT_DPAD_RIGHT)
	run := isHeld(kos.CONT_X)

	maxSpeed := physics.walkMax
	if run {
		maxSpeed = physics.runMax
	}

	player.isRunning = player.vx > physics.runThreshold || player.vx < -physics.runThreshold
	player.isSkidding = false

	if right && !left {
		player.facingRight = true

		if player.vx < 0 {
			player.isSkidding = true
			player.vx = player.vx + physics.skidDecel
		} else {
			player.vx = player.vx + physics.accel
			if player.vx > maxSpeed {
				player.vx = maxSpeed
			}
		}
	} else if left && !right {
		player.facingRight = false

		if player.vx > 0 {
			player.isSkidding = true
			player.vx = player.vx - physics.skidDecel
		} else {
			player.vx = player.vx - physics.accel
			if player.vx < -maxSpeed {
				player.vx = -maxSpeed
			}
		}
	} else {
		player.vx = player.vx * physics.friction

		if player.vx < VelocityStopThreshold && player.vx > -VelocityStopThreshold {
			player.vx = 0
		}
	}

	player.x = player.x + player.vx

	if player.x < -player.size {
		player.x = ScreenWidth
	} else if player.x > ScreenWidth {
		player.x = -player.size
	}
}


