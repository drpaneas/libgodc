package main

func drawWorld() {
	drawRect(0, 0, ScreenWidth, ScreenHeight, ZBackground, colorBg)

	groundHeight := ScreenHeight - (GroundY + player.size)
	drawRect(0, GroundY+player.size, ScreenWidth, groundHeight, ZWorld, colorGround)

	for i := 0; i < platformCount; i++ {
		p := platforms[i]
		drawRect(p.x1, p.y1, p.x2-p.x1, p.y2-p.y1, ZWorld, colorPlatform)
	}
}

