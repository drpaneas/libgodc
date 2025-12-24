package main

func drawPlayer() {
	x := player.x
	y := player.y
	s := player.size

	col := colorPlayer
	if player.isSkidding {
		col = colorSkid
	} else if !player.onGround {
		col = colorAir
	}

	drawRect(x, y, s, s, ZPlayer, col)

	var eyeX float32
	if player.facingRight {
		eyeX = x + s - EyeOffsetRight
	} else {
		eyeX = x + EyeOffsetLeft
	}
	drawRect(eyeX, y+EyeOffsetY, EyeSize, EyeSize, ZPlayerEye, colorEye)

	drawVelocityIndicators(x, y, s)
}

func drawVelocityIndicators(x, y, s float32) {
	if abs32(player.vx) > SpeedLineThreshold {
		var lx float32
		if player.facingRight {
			lx = x - SpeedLineOffset
		} else {
			lx = x + s + SpeedLineOffset - SpeedLineSize
		}

		for i := 0; i < SpeedLineCount; i++ {
			ly := y + SpeedLineOffset + float32(i)*SpeedLineGap
			drawRect(lx, ly, SpeedLineSize, SpeedLineSize, ZSpeedLines, colorSpeed)
		}
	}

	if player.vy < RisingThreshold {
		centerX := x + s/2
		drawLine(centerX, y-IndicatorOffset, centerX, y-IndicatorOffset-IndicatorLength,
			ZIndicator, IndicatorWidth, colorRising)
	}

	if player.vy > FallingThreshold {
		centerX := x + s/2
		bottom := y + s
		drawLine(centerX, bottom+IndicatorOffset, centerX, bottom+IndicatorOffset+IndicatorLength,
			ZIndicator, IndicatorWidth, colorFalling)
	}
}

