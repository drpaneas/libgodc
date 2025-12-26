package main

import "kos"

func updatePlayerJump() {
	jumpPressed := justPressed(kos.CONT_A)
	jumpHeldBtn := isHeld(kos.CONT_A)

	if jumpPressed && player.onGround {
		if player.isRunning {
			player.vy = -physics.jumpVelFast
		} else {
			player.vy = -physics.jumpVelSlow
		}
		player.onGround = false
		player.jumpHeld = true
		player.jumpFrames = 0
	}

	if !player.onGround {
		var grav float32

		if player.jumpHeld && jumpHeldBtn && player.vy < 0 && player.jumpFrames < physics.jumpHoldMax {
			grav = physics.gravityHold
			player.jumpFrames = player.jumpFrames + 1
		} else {
			grav = physics.gravityFall
			if !jumpHeldBtn {
				player.jumpHeld = false
			}
		}

		player.vy = player.vy + grav

		if player.vy > physics.terminalVel {
			player.vy = physics.terminalVel
		}
	}

	player.y = player.y + player.vy

	if player.y >= GroundY {
		player.y = GroundY
		player.vy = 0
		player.onGround = true
		player.jumpHeld = false
	}

	checkPlatformCollisions()
}


