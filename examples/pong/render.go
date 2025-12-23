// render.go - All rendering/drawing functions
package main

import "kos"

// SetupColors sets up the PLX context for untextured drawing
func SetupColors() {
	kos.PlxCxtInit()
	kos.PlxCxtTexture(nil)
	kos.PlxCxtCulling(kos.PLX_CULL_NONE)
	kos.PlxCxtSend(int32(kos.PVR_LIST_OP_POLY))
}

// DrawStars draws the animated parallax starfield
func DrawStars() {
	// Dim stars when moving fast to reduce distraction
	// At speed 1.0: full brightness, at speed 3.0: 40% brightness
	dimFactor := float32(1.0)
	if StarSpeedMult > 1.0 {
		dimFactor = 1.0 - (StarSpeedMult-1.0)*0.3 // 1.0 -> 0.4
		if dimFactor < 0.4 {
			dimFactor = 0.4
		}
	}

	for i := range Stars {
		star := &Stars[i]

		// Create color based on brightness (white/blue tint)
		b := float32(star.Bright) / 255.0 * dimFactor
		// Add slight blue tint to distant stars, white for close ones
		var starColor uint32
		if star.Speed < 0.6 {
			// Distant stars: slight blue tint
			starColor = kos.PlxPackColor(1, b*0.8, b*0.9, b)
		} else if star.Speed < 1.2 {
			// Mid stars: white
			starColor = kos.PlxPackColor(1, b, b, b)
		} else {
			// Near stars: slight yellow/white
			starColor = kos.PlxPackColor(1, b, b, b*0.95)
		}

		// Draw star as small rectangle
		DrawRect(star.X, star.Y, star.Size, star.Size, 2, starColor)
	}
}

// DrawStreakLines draws hyperspace warp effect
func DrawStreakLines(shakeX, shakeY float32) {
	// Dim streak lines to be more subtle (40% of original brightness)
	dimFactor := float32(0.4)

	for i := range StreakLines {
		s := &StreakLines[i]
		if s.X > -s.Length && s.X < float32(WIDTH) {
			// Gradient from bright to dim, reduced brightness
			brightness := s.Brightness * dimFactor
			c := kos.PlxPackColor(brightness, 0.5, 0.6, 0.8)
			DrawRect(s.X+shakeX, s.Y+shakeY, s.Length, 1, 3, c)
		}
	}
}

// DrawBallTrail draws rocket exhaust glow behind the ball
func DrawBallTrail(shakeX, shakeY float32) {
	if IsBallOut() {
		return
	}

	// Draw exhaust glow (fire core behind ball)
	for i := 1; i <= 4; i++ {
		dist := float32(i) * 5
		alpha := 0.5 - float32(i)*0.1
		size := float32(10 - i*2)

		// Position behind ball
		ex := TheBall.X - TheBall.DX*dist + shakeX
		ey := TheBall.Y - TheBall.DY*dist + shakeY

		// Yellow fire color fading to orange
		r := float32(1.0)
		g := 0.9 - float32(i)*0.1 // More yellow
		b := 0.2 - float32(i)*0.03

		exhaustColor := kos.PlxPackColor(alpha, r, g, b)
		DrawCenteredRect(ex, ey, size, size, 4+float32(i), exhaustColor)
	}
}

// DrawParticles draws all active particles
func DrawParticles(shakeX, shakeY float32) {
	for i := 0; i < ParticleCount; i++ {
		p := &Particles[i]

		// Alpha based on remaining life
		alpha := float32(p.Life) / float32(p.MaxLife)

		particleColor := kos.PlxPackColor(alpha, p.R, p.G, p.B)
		DrawCenteredRect(p.X+shakeX, p.Y+shakeY, p.Size, p.Size, 100, particleColor)
	}
}

// DrawLightning draws energy arcs between ball and nearby paddle
func DrawLightning(shakeX, shakeY float32) {
	if IsBallOut() {
		return
	}

	// Only draw when ball is close to a paddle
	for player := 0; player < 2; player++ {
		bat := &Bats[player]
		dist := Abs32(TheBall.X - bat.X)

		// Lightning when within 100 pixels
		if dist < 100 {
			intensity := (100 - dist) / 100.0

			// Flickering effect
			if (LightningTimer/2)%3 != 0 {
				continue
			}

			// Draw multiple lightning segments
			numSegments := 5
			startX := TheBall.X + shakeX
			startY := TheBall.Y + shakeY
			endX := bat.X + shakeX
			endY := bat.Y + shakeY

			// Choose color based on bat
			var r, g, b float32
			if player == 0 {
				r, g, b = 0.3, 1.0, 0.5 // Green
			} else {
				r, g, b = 1.0, 0.4, 0.4 // Red
			}

			prevX, prevY := startX, startY
			for seg := 1; seg <= numSegments; seg++ {
				t := float32(seg) / float32(numSegments)

				// Interpolate with random offset
				nextX := startX + (endX-startX)*t + float32(RandInt(20)-10)*intensity
				nextY := startY + (endY-startY)*t + float32(RandInt(30)-15)*intensity

				if seg == numSegments {
					nextX, nextY = endX, endY
				}

				// Draw segment as small rectangle
				alpha := intensity * 0.7
				lightColor := kos.PlxPackColor(alpha, r, g, b)

				// Draw glow
				midX := (prevX + nextX) / 2
				midY := (prevY + nextY) / 2
				DrawCenteredRect(midX, midY, 4, 4, 90, lightColor)

				prevX, prevY = nextX, nextY
			}
		}
	}
}

// DrawRect draws a solid colored rectangle
func DrawRect(x, y, w, h, z float32, color uint32) {
	kos.PlxVertInp(kos.PLX_VERT, x, y+h, z, color)
	kos.PlxVertInp(kos.PLX_VERT, x, y, z, color)
	kos.PlxVertInp(kos.PLX_VERT, x+w, y+h, z, color)
	kos.PlxVertInp(kos.PLX_VERT_EOS, x+w, y, z, color)
}

// DrawCenteredRect draws a rectangle centered at (cx, cy)
func DrawCenteredRect(cx, cy, w, h, z float32, color uint32) {
	DrawRect(cx-w/2, cy-h/2, w, h, z, color)
}

// DrawLightsaber draws a contained lightsaber bat
// player: 0 = green (left), 1 = red (right)
// intensity: 0.0-1.0 for hit effect
func DrawLightsaber(cx, cy float32, player int, intensity float32) {
	// Dimensions
	batW := float32(16)
	batH := float32(BAT_HEIGHT)
	capH := float32(12)

	// Blade color
	var r, g, b float32
	if player == 0 {
		r, g, b = 0.1, 0.95, 0.4 // Green
	} else {
		r, g, b = 0.95, 0.2, 0.2 // Red
	}

	// Positions
	topY := cy - batH/2
	bottomY := cy + batH/2
	bodyTopY := topY + capH
	bodyBottomY := bottomY - capH
	bodyH := bodyBottomY - bodyTopY

	// ===== GLOW AURA (subtle, always on, stronger on hit) =====
	baseGlow := float32(0.06)
	if intensity > 0 {
		baseGlow += intensity * 0.2
	}
	// Outermost glow
	DrawCenteredRect(cx, cy, batW+20, batH+10, 35, kos.PlxPackColor(baseGlow*0.4, r, g, b))
	DrawCenteredRect(cx, cy, batW+14, batH+6, 36, kos.PlxPackColor(baseGlow*0.6, r, g, b))
	DrawCenteredRect(cx, cy, batW+8, batH+3, 37, kos.PlxPackColor(baseGlow*0.8, r, g, b))
	DrawCenteredRect(cx, cy, batW+4, batH+1, 38, kos.PlxPackColor(baseGlow, r, g, b))

	// ===== TOP CAP (pill-shaped end) =====
	// Build rounded top with progressively wider layers
	// Row 1 (topmost, smallest)
	DrawRect(cx-3, topY, 6, 2, 50, kos.PlxPackColor(1, r*0.7, g*0.7, b*0.7))
	DrawRect(cx-1, topY, 2, 2, 51, kos.PlxPackColor(1, r*0.4+0.6, g*0.4+0.6, b*0.4+0.6))
	// Row 2
	DrawRect(cx-5, topY+2, 10, 2, 50, kos.PlxPackColor(1, r*0.8, g*0.8, b*0.8))
	DrawRect(cx-3, topY+2, 6, 2, 51, kos.PlxPackColor(1, r, g, b))
	DrawRect(cx-1, topY+2, 2, 2, 52, kos.PlxPackColor(1, 1, 1, 1))
	// Row 3
	DrawRect(cx-7, topY+4, 14, 2, 50, kos.PlxPackColor(1, r*0.85, g*0.85, b*0.85))
	DrawRect(cx-4, topY+4, 8, 2, 51, kos.PlxPackColor(1, r, g, b))
	DrawRect(cx-1, topY+4, 2, 2, 52, kos.PlxPackColor(1, 1, 1, 1))
	// Row 4 (full width transition to body)
	DrawRect(cx-batW/2+1, topY+6, batW-2, 3, 50, kos.PlxPackColor(1, r*0.9, g*0.9, b*0.9))
	DrawRect(cx-5, topY+6, 10, 3, 51, kos.PlxPackColor(1, r, g, b))
	DrawRect(cx-2, topY+6, 4, 3, 52, kos.PlxPackColor(1, r*0.3+0.7, g*0.3+0.7, b*0.3+0.7))
	// Row 5 (junction)
	DrawRect(cx-batW/2, topY+9, batW, 3, 50, kos.PlxPackColor(1, r*0.6, g*0.6, b*0.6))
	DrawRect(cx-4, topY+9, 8, 3, 51, kos.PlxPackColor(1, r*0.8, g*0.8, b*0.8))

	// ===== MAIN BODY (dark cylinder) =====
	// Outer shadow
	DrawRect(cx-batW/2-1, bodyTopY, batW+2, bodyH, 44, kos.PlxPackColor(1, 0.03, 0.03, 0.04))
	// Body gradient layers (dark edges, lighter center)
	DrawRect(cx-batW/2, bodyTopY, batW, bodyH, 45, kos.PlxPackColor(1, 0.1, 0.1, 0.12))
	DrawRect(cx-batW/2+1, bodyTopY, batW-2, bodyH, 46, kos.PlxPackColor(1, 0.18, 0.19, 0.21))
	DrawRect(cx-batW/2+2, bodyTopY, batW-4, bodyH, 47, kos.PlxPackColor(1, 0.26, 0.27, 0.3))
	DrawRect(cx-batW/2+3, bodyTopY, batW-6, bodyH, 48, kos.PlxPackColor(1, 0.32, 0.34, 0.37))
	DrawRect(cx-batW/2+4, bodyTopY, batW-8, bodyH, 49, kos.PlxPackColor(1, 0.38, 0.4, 0.44))

	// Left edge highlight (metallic reflection)
	DrawRect(cx-batW/2+1, bodyTopY+3, 1, bodyH-6, 53, kos.PlxPackColor(0.4, 0.55, 0.57, 0.6))
	DrawRect(cx-batW/2+2, bodyTopY+5, 1, bodyH-10, 54, kos.PlxPackColor(0.25, 0.65, 0.67, 0.7))

	// Right edge shadow
	DrawRect(cx+batW/2-2, bodyTopY+3, 1, bodyH-6, 53, kos.PlxPackColor(0.3, 0.08, 0.08, 0.1))

	// ===== BLADE STRIP (thin glowing line in center) =====
	bladeW := float32(3)
	// Blade glow (wider, dimmer)
	DrawRect(cx-bladeW/2-2, bodyTopY+1, bladeW+4, bodyH-2, 55, kos.PlxPackColor(0.5, r*0.7, g*0.7, b*0.7))
	// Blade outer
	DrawRect(cx-bladeW/2-1, bodyTopY+2, bladeW+2, bodyH-4, 56, kos.PlxPackColor(0.85, r*0.9, g*0.9, b*0.9))
	// Blade main
	DrawRect(cx-bladeW/2, bodyTopY+3, bladeW, bodyH-6, 57, kos.PlxPackColor(1, r, g, b))
	// Blade bright core
	DrawRect(cx-0.5, bodyTopY+4, 1, bodyH-8, 58, kos.PlxPackColor(1, r*0.2+0.8, g*0.2+0.8, b*0.2+0.8))

	// ===== BOTTOM CAP (pill-shaped end, mirrored) =====
	// Row 5 (junction from body)
	DrawRect(cx-batW/2, bodyBottomY, batW, 3, 50, kos.PlxPackColor(1, r*0.6, g*0.6, b*0.6))
	DrawRect(cx-4, bodyBottomY, 8, 3, 51, kos.PlxPackColor(1, r*0.8, g*0.8, b*0.8))
	// Row 4
	DrawRect(cx-batW/2+1, bottomY-9, batW-2, 3, 50, kos.PlxPackColor(1, r*0.9, g*0.9, b*0.9))
	DrawRect(cx-5, bottomY-9, 10, 3, 51, kos.PlxPackColor(1, r, g, b))
	DrawRect(cx-2, bottomY-9, 4, 3, 52, kos.PlxPackColor(1, r*0.3+0.7, g*0.3+0.7, b*0.3+0.7))
	// Row 3
	DrawRect(cx-7, bottomY-6, 14, 2, 50, kos.PlxPackColor(1, r*0.85, g*0.85, b*0.85))
	DrawRect(cx-4, bottomY-6, 8, 2, 51, kos.PlxPackColor(1, r, g, b))
	DrawRect(cx-1, bottomY-6, 2, 2, 52, kos.PlxPackColor(1, 1, 1, 1))
	// Row 2
	DrawRect(cx-5, bottomY-4, 10, 2, 50, kos.PlxPackColor(1, r*0.8, g*0.8, b*0.8))
	DrawRect(cx-3, bottomY-4, 6, 2, 51, kos.PlxPackColor(1, r, g, b))
	DrawRect(cx-1, bottomY-4, 2, 2, 52, kos.PlxPackColor(1, 1, 1, 1))
	// Row 1 (bottommost, smallest)
	DrawRect(cx-3, bottomY-2, 6, 2, 50, kos.PlxPackColor(1, r*0.7, g*0.7, b*0.7))
	DrawRect(cx-1, bottomY-2, 2, 2, 51, kos.PlxPackColor(1, r*0.4+0.6, g*0.4+0.6, b*0.4+0.6))

	// ===== IMPACT FLASH (on hit) =====
	if intensity > 0.2 {
		var impactX float32
		if player == 0 {
			impactX = cx + batW/2 + 8
		} else {
			impactX = cx - batW/2 - 8
		}

		// Expanding shockwave
		waveSize := 15 + intensity*40

		// Outer halo
		DrawCenteredRect(impactX, cy, waveSize*1.2, waveSize*2, 60, kos.PlxPackColor(intensity*0.2, 1, 1, 1))
		// Mid ring
		DrawCenteredRect(impactX, cy, waveSize*0.7, waveSize*1.3, 61, kos.PlxPackColor(intensity*0.35, r*0.5+0.5, g*0.5+0.5, b*0.5+0.5))
		// Inner flash
		DrawCenteredRect(impactX, cy, waveSize*0.35, waveSize*0.7, 62, kos.PlxPackColor(intensity*0.6, 1, 1, 1))
		// Core
		DrawCenteredRect(impactX, cy, waveSize*0.15, waveSize*0.3, 63, kos.PlxPackColor(intensity*0.9, 1, 1, 1))
	}
}

// DrawImage draws a textured image centered at (cx, cy)
func DrawImage(tex *kos.PlxTexture, cx, cy, w, h, z float32) {
	if tex == nil {
		return
	}
	x := cx - w/2
	y := cy - h/2
	// Full brightness white color (ARGB: 0xFFFFFFFF)
	fullWhite := uint32(0xFFFFFFFF)
	kos.PlxCxtInit()
	kos.PlxCxtTexture(tex.Ptr())
	kos.PlxCxtCulling(kos.PLX_CULL_NONE)
	kos.PlxCxtSend(int32(kos.PVR_LIST_TR_POLY))
	kos.PlxVertIfp(kos.PLX_VERT, x, y+h, z, fullWhite, 0, 1)
	kos.PlxVertIfp(kos.PLX_VERT, x, y, z, fullWhite, 0, 0)
	kos.PlxVertIfp(kos.PLX_VERT, x+w, y+h, z, fullWhite, 1, 1)
	kos.PlxVertIfp(kos.PLX_VERT_EOS, x+w, y, z, fullWhite, 1, 0)
}

// Render draws the main game screen
func Render() {
	kos.PvrSceneBegin()
	kos.PvrListBegin(kos.PVR_LIST_OP_POLY)

	// Apply screen shake offset
	shakeX := ScreenShakeX
	shakeY := ScreenShakeY

	// Deep space background (solid dark color)
	SetupColors()
	DrawRect(0, 0, WIDTH, HEIGHT, 1, ColorDark)

	// Hyperspace streak lines (when ball is fast)
	if StreaksActive {
		DrawStreakLines(shakeX, shakeY)
	}

	// Animated starfield
	DrawStars()

	// Ball trail effect
	DrawBallTrail(shakeX, shakeY)

	SetupColors()

	// Arena boundaries (with shake)
	DrawRect(shakeX, float32(ARENA_TOP-4)+shakeY, WIDTH, 4, 10, ColorGray)
	DrawRect(shakeX, float32(ARENA_BOTTOM)+shakeY, WIDTH, 4, 10, ColorGray)

	// Center dashed line (with shake)
	for y := float32(ARENA_TOP); y < float32(ARENA_BOTTOM); y += 25 {
		DrawRect(float32(HALF_WIDTH-2)+shakeX, y+shakeY, 4, 15, 10, ColorGray)
	}

	// Draw particles (behind bats)
	DrawParticles(shakeX, shakeY)

	// Draw lightning effect
	DrawLightning(shakeX, shakeY)

	// Draw lightsaber bats (textured if available, otherwise procedural)
	bat0Frame := 0
	if Bats[0].Timer > 6 {
		bat0Frame = 2
	} else if Bats[0].Timer > 3 {
		bat0Frame = 1
	}
	bat1Frame := 0
	if Bats[1].Timer > 6 {
		bat1Frame = 2
	} else if Bats[1].Timer > 3 {
		bat1Frame = 1
	}

	// Use textures if loaded, otherwise fall back to procedural
	if TexBat0[0] != nil {
		// Will draw in TR_POLY pass below
	} else {
		bat0Intensity := float32(0)
		if Bats[0].Timer > 0 {
			bat0Intensity = float32(Bats[0].Timer) / 10.0
		}
		DrawLightsaber(Bats[0].X+shakeX, Bats[0].Y+shakeY, 0, bat0Intensity)
	}
	if TexBat1[0] != nil {
		// Will draw in TR_POLY pass below
	} else {
		bat1Intensity := float32(0)
		if Bats[1].Timer > 0 {
			bat1Intensity = float32(Bats[1].Timer) / 10.0
		}
		DrawLightsaber(Bats[1].X+shakeX, Bats[1].Y+shakeY, 1, bat1Intensity)
	}

	// Ball (fallback if no texture) - with shake
	if !IsBallOut() && TexBall == nil {
		DrawCenteredRect(TheBall.X+shakeX, TheBall.Y+shakeY, BALL_SIZE, BALL_SIZE, 50, ColorWhite)
	}

	// Scores (green for player 1 / left, red for player 2 / right - matching lightsabers)
	ColorRed := kos.PlxPackColor(1, 1.0, 0.3, 0.3)
	DrawScore(float32(HALF_WIDTH-80), 8, Bats[0].Score, ColorGreen)
	DrawScore(float32(HALF_WIDTH+50), 8, Bats[1].Score, ColorRed)

	kos.PvrListFinish()

	// Textured sprites (bats, ball, impact effects, goal effects)
	hasTextures := TexBall != nil || TexImpact[0] != nil || TexEffect0 != nil || TexBat0[0] != nil
	if hasTextures {
		kos.PvrListBegin(kos.PVR_LIST_TR_POLY)

		// Boing-style bats - 128x128 square texture, paddle is ~96px tall inside
		// Scale so paddle height matches BAT_HEIGHT: 128 * (BAT_HEIGHT / 96) = 128 * 1.25 = 160
		batTexSize := float32(128) * float32(BAT_HEIGHT) / 96.0
		batTexH := batTexSize
		batTexW := batTexSize // Square texture, keep 1:1 aspect ratio
		if TexBat0[bat0Frame] != nil {
			DrawImage(TexBat0[bat0Frame], Bats[0].X+shakeX, Bats[0].Y+shakeY, batTexW, batTexH, 40)
		}
		if TexBat1[bat1Frame] != nil {
			DrawImage(TexBat1[bat1Frame], Bats[1].X+shakeX, Bats[1].Y+shakeY, batTexW, batTexH, 40)
		}

		// Impact effect (with shake)
		if ImpactTimer > 0 {
			impactFrame := (15 - ImpactTimer) / 3
			if impactFrame > 4 {
				impactFrame = 4
			}
			if TexImpact[impactFrame] != nil {
				scale := float32(ImpactTimer) / 15.0 * 64.0
				DrawImage(TexImpact[impactFrame], ImpactX+shakeX, ImpactY+shakeY, scale, scale, 45)
			}
		}

		// Textured ball (with shake)
		if !IsBallOut() && TexBall != nil {
			DrawImage(TexBall, TheBall.X+shakeX, TheBall.Y+shakeY, BALL_SIZE, BALL_SIZE, 50)
		}

		// Goal effect
		var effectTex *kos.PlxTexture
		if GoalEffectPlayer == 0 {
			effectTex = TexEffect0
		} else {
			effectTex = TexEffect1
		}
		if GoalEffectTimer > 0 && effectTex != nil {
			alpha := float32(GoalEffectTimer) / 30.0
			effectColor := kos.PlxPackColor(alpha, 1, 1, 1)
			kos.PlxCxtInit()
			kos.PlxCxtTexture(effectTex.Ptr())
			kos.PlxCxtCulling(kos.PLX_CULL_NONE)
			kos.PlxCxtSend(int32(kos.PVR_LIST_TR_POLY))
			kos.PlxVertIfp(kos.PLX_VERT, 0, HEIGHT, 200, effectColor, 0, 1)
			kos.PlxVertIfp(kos.PLX_VERT, 0, 0, 200, effectColor, 0, 0)
			kos.PlxVertIfp(kos.PLX_VERT, WIDTH, HEIGHT, 200, effectColor, 1, 1)
			kos.PlxVertIfp(kos.PLX_VERT_EOS, WIDTH, 0, 200, effectColor, 1, 0)
		}

		kos.PvrListFinish()
	}

	kos.PvrSceneFinish()
	kos.PvrWaitReady()
}

// DrawScore draws a two-digit score
func DrawScore(x, y float32, score int, color uint32) {
	tens := score / 10
	ones := score % 10
	DrawDigit(x, y, tens, color)
	DrawDigit(x+25, y, ones, color)
}

// DrawDigit draws a 7-segment style digit
func DrawDigit(x, y float32, digit int, color uint32) {
	w := float32(20)
	h := float32(28)
	s := float32(4)

	segments := [10][7]bool{
		{true, true, true, true, true, true, false},
		{false, true, true, false, false, false, false},
		{true, true, false, true, true, false, true},
		{true, true, true, true, false, false, true},
		{false, true, true, false, false, true, true},
		{true, false, true, true, false, true, true},
		{true, false, true, true, true, true, true},
		{true, true, true, false, false, false, false},
		{true, true, true, true, true, true, true},
		{true, true, true, true, false, true, true},
	}

	if digit < 0 || digit > 9 {
		return
	}

	seg := segments[digit]
	halfH := (h - s) / 2

	if seg[0] {
		DrawRect(x+s, y, w-s*2, s, 100, color)
	}
	if seg[1] {
		DrawRect(x+w-s, y+s, s, halfH-s, 100, color)
	}
	if seg[2] {
		DrawRect(x+w-s, y+halfH+s, s, halfH-s, 100, color)
	}
	if seg[3] {
		DrawRect(x+s, y+h-s, w-s*2, s, 100, color)
	}
	if seg[4] {
		DrawRect(x, y+halfH+s, s, halfH-s, 100, color)
	}
	if seg[5] {
		DrawRect(x, y+s, s, halfH-s, 100, color)
	}
	if seg[6] {
		DrawRect(x+s, y+halfH, w-s*2, s, 100, color)
	}
}

// DrawMenu draws the menu screen with animated starfield
func DrawMenu() {
	kos.PvrSceneBegin()
	kos.PvrListBegin(kos.PVR_LIST_OP_POLY)

	// Animated starfield background
	SetupColors()
	DrawRect(0, 0, WIDTH, HEIGHT, 1, ColorDark)
	DrawStars()

	// Menu overlay
	SetupColors()

	// Title box with glow effect
	titleGlow := kos.PlxPackColor(0.3, 0.2, 0.4, 0.8)
	DrawRect(140, 60, 360, 80, 50, titleGlow)

	titleBg := kos.PlxPackColor(1, 0.1, 0.15, 0.25)
	DrawRect(150, 70, 340, 60, 51, titleBg)

	// "SPACE PONG" title using colored bars
	titleColor := kos.PlxPackColor(1, 0.5, 0.8, 1.0)
	// S
	DrawRect(170, 85, 20, 5, 52, titleColor)
	DrawRect(170, 85, 5, 15, 52, titleColor)
	DrawRect(170, 97, 20, 5, 52, titleColor)
	DrawRect(185, 97, 5, 15, 52, titleColor)
	DrawRect(170, 110, 20, 5, 52, titleColor)
	// P
	DrawRect(200, 85, 5, 30, 52, titleColor)
	DrawRect(200, 85, 20, 5, 52, titleColor)
	DrawRect(215, 85, 5, 15, 52, titleColor)
	DrawRect(200, 97, 20, 5, 52, titleColor)
	// A
	DrawRect(230, 85, 5, 30, 52, titleColor)
	DrawRect(230, 85, 20, 5, 52, titleColor)
	DrawRect(245, 85, 5, 30, 52, titleColor)
	DrawRect(230, 100, 20, 5, 52, titleColor)
	// C
	DrawRect(260, 85, 20, 5, 52, titleColor)
	DrawRect(260, 85, 5, 30, 52, titleColor)
	DrawRect(260, 110, 20, 5, 52, titleColor)
	// E
	DrawRect(290, 85, 5, 30, 52, titleColor)
	DrawRect(290, 85, 20, 5, 52, titleColor)
	DrawRect(290, 97, 15, 5, 52, titleColor)
	DrawRect(290, 110, 20, 5, 52, titleColor)

	// PONG
	pongColor := kos.PlxPackColor(1, 0.3, 1.0, 0.5)
	// P
	DrawRect(330, 85, 5, 30, 52, pongColor)
	DrawRect(330, 85, 20, 5, 52, pongColor)
	DrawRect(345, 85, 5, 15, 52, pongColor)
	DrawRect(330, 97, 20, 5, 52, pongColor)
	// O
	DrawRect(360, 85, 20, 5, 52, pongColor)
	DrawRect(360, 85, 5, 30, 52, pongColor)
	DrawRect(375, 85, 5, 30, 52, pongColor)
	DrawRect(360, 110, 20, 5, 52, pongColor)
	// N
	DrawRect(390, 85, 5, 30, 52, pongColor)
	DrawRect(410, 85, 5, 30, 52, pongColor)
	DrawRect(393, 90, 5, 8, 52, pongColor)
	DrawRect(398, 95, 5, 8, 52, pongColor)
	DrawRect(403, 100, 5, 8, 52, pongColor)
	// G
	DrawRect(425, 85, 25, 5, 52, pongColor)
	DrawRect(425, 85, 5, 30, 52, pongColor)
	DrawRect(425, 110, 25, 5, 52, pongColor)
	DrawRect(445, 97, 5, 18, 52, pongColor)
	DrawRect(435, 97, 15, 5, 52, pongColor)

	// Menu options
	sel1 := ColorGray
	sel2 := ColorGray
	if NumPlayers == 1 {
		sel1 = ColorGreen
	} else {
		sel2 = kos.PlxPackColor(1, 1.0, 0.3, 0.3) // Red
	}

	// Option box background
	optBg := kos.PlxPackColor(1, 0.1, 0.12, 0.18)

	// 1 PLAYER option
	DrawRect(200, 200, 240, 40, 50, optBg)
	DrawRect(202, 202, 236, 36, 51, sel1)
	DrawRect(204, 204, 232, 32, 52, optBg)

	// Draw "1 PLAYER" text
	tx := float32(230)
	ty := float32(212)
	// "1"
	DrawRect(tx, ty, 4, 16, 53, sel1)
	DrawRect(tx-4, ty+2, 4, 4, 53, sel1)
	tx += 20
	// "P"
	DrawRect(tx, ty, 4, 16, 53, sel1)
	DrawRect(tx, ty, 12, 4, 53, sel1)
	DrawRect(tx+8, ty, 4, 8, 53, sel1)
	DrawRect(tx, ty+6, 12, 4, 53, sel1)
	tx += 18
	// "L"
	DrawRect(tx, ty, 4, 16, 53, sel1)
	DrawRect(tx, ty+12, 12, 4, 53, sel1)
	tx += 18
	// "A"
	DrawRect(tx, ty+4, 4, 12, 53, sel1)
	DrawRect(tx+8, ty+4, 4, 12, 53, sel1)
	DrawRect(tx, ty, 12, 4, 53, sel1)
	DrawRect(tx, ty+8, 12, 4, 53, sel1)
	tx += 18
	// "Y"
	DrawRect(tx, ty, 4, 8, 53, sel1)
	DrawRect(tx+8, ty, 4, 8, 53, sel1)
	DrawRect(tx+4, ty+6, 4, 10, 53, sel1)
	tx += 18
	// "E"
	DrawRect(tx, ty, 4, 16, 53, sel1)
	DrawRect(tx, ty, 12, 4, 53, sel1)
	DrawRect(tx, ty+6, 10, 4, 53, sel1)
	DrawRect(tx, ty+12, 12, 4, 53, sel1)
	tx += 18
	// "R"
	DrawRect(tx, ty, 4, 16, 53, sel1)
	DrawRect(tx, ty, 12, 4, 53, sel1)
	DrawRect(tx+8, ty, 4, 8, 53, sel1)
	DrawRect(tx, ty+6, 12, 4, 53, sel1)
	DrawRect(tx+6, ty+8, 4, 8, 53, sel1)

	// 2 PLAYERS option
	DrawRect(200, 260, 240, 40, 50, optBg)
	DrawRect(202, 262, 236, 36, 51, sel2)
	DrawRect(204, 264, 232, 32, 52, optBg)

	// Draw "2 PLAYERS" text
	tx = 225
	ty = 272
	// "2"
	DrawRect(tx, ty, 12, 4, 53, sel2)
	DrawRect(tx+8, ty, 4, 8, 53, sel2)
	DrawRect(tx, ty+6, 12, 4, 53, sel2)
	DrawRect(tx, ty+8, 4, 8, 53, sel2)
	DrawRect(tx, ty+12, 12, 4, 53, sel2)
	tx += 20
	// "P"
	DrawRect(tx, ty, 4, 16, 53, sel2)
	DrawRect(tx, ty, 12, 4, 53, sel2)
	DrawRect(tx+8, ty, 4, 8, 53, sel2)
	DrawRect(tx, ty+6, 12, 4, 53, sel2)
	tx += 18
	// "L"
	DrawRect(tx, ty, 4, 16, 53, sel2)
	DrawRect(tx, ty+12, 12, 4, 53, sel2)
	tx += 18
	// "A"
	DrawRect(tx, ty+4, 4, 12, 53, sel2)
	DrawRect(tx+8, ty+4, 4, 12, 53, sel2)
	DrawRect(tx, ty, 12, 4, 53, sel2)
	DrawRect(tx, ty+8, 12, 4, 53, sel2)
	tx += 18
	// "Y"
	DrawRect(tx, ty, 4, 8, 53, sel2)
	DrawRect(tx+8, ty, 4, 8, 53, sel2)
	DrawRect(tx+4, ty+6, 4, 10, 53, sel2)
	tx += 18
	// "E"
	DrawRect(tx, ty, 4, 16, 53, sel2)
	DrawRect(tx, ty, 12, 4, 53, sel2)
	DrawRect(tx, ty+6, 10, 4, 53, sel2)
	DrawRect(tx, ty+12, 12, 4, 53, sel2)
	tx += 18
	// "R"
	DrawRect(tx, ty, 4, 16, 53, sel2)
	DrawRect(tx, ty, 12, 4, 53, sel2)
	DrawRect(tx+8, ty, 4, 8, 53, sel2)
	DrawRect(tx, ty+6, 12, 4, 53, sel2)
	DrawRect(tx+6, ty+8, 4, 8, 53, sel2)
	tx += 18
	// "S"
	DrawRect(tx, ty, 12, 4, 53, sel2)
	DrawRect(tx, ty, 4, 8, 53, sel2)
	DrawRect(tx, ty+6, 12, 4, 53, sel2)
	DrawRect(tx+8, ty+6, 4, 10, 53, sel2)
	DrawRect(tx, ty+12, 12, 4, 53, sel2)

	kos.PvrListFinish()
	kos.PvrSceneFinish()
	kos.PvrWaitReady()
}

// DrawGameOver draws the game over screen with animated starfield
func DrawGameOver() {
	kos.PvrSceneBegin()
	kos.PvrListBegin(kos.PVR_LIST_OP_POLY)

	// Animated starfield background
	SetupColors()
	DrawRect(0, 0, WIDTH, HEIGHT, 1, ColorDark)
	DrawStars()

	// Determine if player won (player is always bat 0 in 1P mode)
	playerWon := Bats[0].Score >= WIN_SCORE

	// Box colors based on win/lose
	var boxGlow, titleColor uint32
	if playerWon {
		boxGlow = kos.PlxPackColor(0.4, 0.2, 0.5, 0.3)  // Green glow
		titleColor = kos.PlxPackColor(1, 0.3, 1.0, 0.4) // Green text
	} else {
		boxGlow = kos.PlxPackColor(0.4, 0.5, 0.2, 0.3)  // Red glow
		titleColor = kos.PlxPackColor(1, 1.0, 0.3, 0.3) // Red text
	}

	// Box with glow
	DrawRect(130, 100, 380, 180, 50, boxGlow)

	boxBg := kos.PlxPackColor(1, 0.08, 0.1, 0.15)
	DrawRect(150, 120, 340, 140, 51, boxBg)

	if playerWon {
		// "YOU WIN!" text
		tx := float32(185)
		ty := float32(140)
		// Y
		DrawRect(tx, ty, 5, 10, 52, titleColor)
		DrawRect(tx+15, ty, 5, 10, 52, titleColor)
		DrawRect(tx+5, ty+8, 5, 5, 52, titleColor)
		DrawRect(tx+10, ty+8, 5, 5, 52, titleColor)
		DrawRect(tx+7, ty+12, 6, 13, 52, titleColor)
		tx += 30
		// O
		DrawRect(tx, ty, 20, 5, 52, titleColor)
		DrawRect(tx, ty, 5, 25, 52, titleColor)
		DrawRect(tx+15, ty, 5, 25, 52, titleColor)
		DrawRect(tx, ty+20, 20, 5, 52, titleColor)
		tx += 30
		// U
		DrawRect(tx, ty, 5, 25, 52, titleColor)
		DrawRect(tx+15, ty, 5, 25, 52, titleColor)
		DrawRect(tx, ty+20, 20, 5, 52, titleColor)
		tx += 40
		// W
		DrawRect(tx, ty, 5, 25, 52, titleColor)
		DrawRect(tx+20, ty, 5, 25, 52, titleColor)
		DrawRect(tx+7, ty+15, 5, 10, 52, titleColor)
		DrawRect(tx+13, ty+15, 5, 10, 52, titleColor)
		DrawRect(tx+10, ty+10, 5, 8, 52, titleColor)
		tx += 35
		// I
		DrawRect(tx, ty, 15, 5, 52, titleColor)
		DrawRect(tx+5, ty, 5, 25, 52, titleColor)
		DrawRect(tx, ty+20, 15, 5, 52, titleColor)
		tx += 25
		// N
		DrawRect(tx, ty, 5, 25, 52, titleColor)
		DrawRect(tx+15, ty, 5, 25, 52, titleColor)
		DrawRect(tx+5, ty+5, 5, 6, 52, titleColor)
		DrawRect(tx+8, ty+9, 5, 6, 52, titleColor)
		DrawRect(tx+11, ty+13, 5, 6, 52, titleColor)
		tx += 28
		// !
		DrawRect(tx, ty, 5, 16, 52, titleColor)
		DrawRect(tx, ty+20, 5, 5, 52, titleColor)
	} else {
		// "YOU LOSE" text
		tx := float32(175)
		ty := float32(140)
		// Y
		DrawRect(tx, ty, 5, 10, 52, titleColor)
		DrawRect(tx+15, ty, 5, 10, 52, titleColor)
		DrawRect(tx+5, ty+8, 5, 5, 52, titleColor)
		DrawRect(tx+10, ty+8, 5, 5, 52, titleColor)
		DrawRect(tx+7, ty+12, 6, 13, 52, titleColor)
		tx += 30
		// O
		DrawRect(tx, ty, 20, 5, 52, titleColor)
		DrawRect(tx, ty, 5, 25, 52, titleColor)
		DrawRect(tx+15, ty, 5, 25, 52, titleColor)
		DrawRect(tx, ty+20, 20, 5, 52, titleColor)
		tx += 30
		// U
		DrawRect(tx, ty, 5, 25, 52, titleColor)
		DrawRect(tx+15, ty, 5, 25, 52, titleColor)
		DrawRect(tx, ty+20, 20, 5, 52, titleColor)
		tx += 40
		// L
		DrawRect(tx, ty, 5, 25, 52, titleColor)
		DrawRect(tx, ty+20, 18, 5, 52, titleColor)
		tx += 28
		// O
		DrawRect(tx, ty, 20, 5, 52, titleColor)
		DrawRect(tx, ty, 5, 25, 52, titleColor)
		DrawRect(tx+15, ty, 5, 25, 52, titleColor)
		DrawRect(tx, ty+20, 20, 5, 52, titleColor)
		tx += 30
		// S
		DrawRect(tx, ty, 18, 5, 52, titleColor)
		DrawRect(tx, ty, 5, 12, 52, titleColor)
		DrawRect(tx, ty+10, 18, 5, 52, titleColor)
		DrawRect(tx+13, ty+10, 5, 15, 52, titleColor)
		DrawRect(tx, ty+20, 18, 5, 52, titleColor)
		tx += 28
		// E
		DrawRect(tx, ty, 5, 25, 52, titleColor)
		DrawRect(tx, ty, 18, 5, 52, titleColor)
		DrawRect(tx, ty+10, 14, 5, 52, titleColor)
		DrawRect(tx, ty+20, 18, 5, 52, titleColor)
	}

	// "PRESS START" instruction
	pressColor := kos.PlxPackColor(1, 0.5, 0.5, 0.6)
	DrawRect(200, 200, 240, 25, 52, pressColor)

	// Final scores
	DrawScore(200, 300, Bats[0].Score, ColorGreen)
	ColorRed := kos.PlxPackColor(1, 1.0, 0.3, 0.3)
	DrawScore(380, 300, Bats[1].Score, ColorRed)

	kos.PvrListFinish()
	kos.PvrSceneFinish()
	kos.PvrWaitReady()
}
