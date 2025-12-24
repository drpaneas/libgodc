package main

func checkPlatformCollisions() {
	if platformCount == 0 {
		return
	}

	feetY := player.y + player.size
	prevFeet := feetY - player.vy
	landedOnPlatform := false

	for i := 0; i < platformCount; i++ {
		p := platforms[i]

		if player.x+player.size <= p.x1 || player.x >= p.x2 {
			continue
		}

		if player.vy > 0 && prevFeet <= p.y1 && feetY >= p.y1 {
			player.y = p.y1 - player.size
			player.vy = 0
			player.onGround = true
			player.jumpHeld = false
			landedOnPlatform = true
			break
		}

		if player.onGround && player.y+player.size == p.y1 {
			landedOnPlatform = true
		}
	}

	if player.onGround && player.y < GroundY && !landedOnPlatform {
		player.onGround = false
	}
}

