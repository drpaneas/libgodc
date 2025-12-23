// game.go - Game logic and update functions
package main

import "kos"

// === PARTICLE SYSTEM FUNCTIONS ===

// SpawnParticles creates an explosion of particles at a position
func SpawnParticles(x, y float32, count int, r, g, b float32, speed float32) {
	for i := 0; i < count && ParticleCount < MAX_PARTICLES; i++ {
		// Random angle
		angle := float32(RandInt(360)) * 3.14159 / 180.0
		vel := speed * (0.5 + float32(RandInt(100))/100.0)

		Particles[ParticleCount] = Particle{
			X:       x,
			Y:       y,
			VX:      vel * Cos32(angle),
			VY:      vel * Sin32(angle),
			Life:    20 + RandInt(20),
			MaxLife: 40,
			Size:    2 + float32(RandInt(4)),
			R:       r,
			G:       g,
			B:       b,
		}
		ParticleCount++
	}
}

// SpawnDirectionalParticles creates particles moving in a direction
func SpawnDirectionalParticles(x, y, dirX, dirY float32, count int, r, g, b float32) {
	for i := 0; i < count && ParticleCount < MAX_PARTICLES; i++ {
		spread := float32(RandInt(60)-30) * 3.14159 / 180.0
		vel := 3.0 + float32(RandInt(40))/10.0

		Particles[ParticleCount] = Particle{
			X:       x + float32(RandInt(10)-5),
			Y:       y + float32(RandInt(10)-5),
			VX:      dirX*vel + Cos32(spread)*2,
			VY:      dirY*vel + Sin32(spread)*2,
			Life:    15 + RandInt(15),
			MaxLife: 30,
			Size:    1 + float32(RandInt(3)),
			R:       r,
			G:       g,
			B:       b,
		}
		ParticleCount++
	}
}

// UpdateParticles moves and ages all particles
func UpdateParticles() {
	newCount := 0
	for i := 0; i < ParticleCount; i++ {
		p := &Particles[i]
		p.Life--
		if p.Life > 0 {
			p.X += p.VX
			p.Y += p.VY
			p.VX *= 0.96 // Friction
			p.VY *= 0.96
			p.Size *= 0.98 // Shrink

			// Keep alive particles
			if newCount != i {
				Particles[newCount] = *p
			}
			newCount++
		}
	}
	ParticleCount = newCount
}

// UpdateBallTrail creates rocket exhaust particles behind the ball
func UpdateBallTrail() {
	if IsBallOut() {
		return
	}

	// Spawn fire particles every frame
	for i := 0; i < 2; i++ {
		if ParticleCount < MAX_PARTICLES {
			// Position behind ball (opposite of movement)
			px := TheBall.X - TheBall.DX*8 + float32(RandInt(8)-4)
			py := TheBall.Y - TheBall.DY*8 + float32(RandInt(8)-4)

			// Velocity away from ball movement
			vx := -TheBall.DX*1.5 + float32(RandInt(20)-10)/10.0
			vy := -TheBall.DY*1.5 + float32(RandInt(20)-10)/10.0

			// Fire colors: yellow to orange
			r := float32(1.0)
			g := 0.7 + float32(RandInt(30))/100.0 // 0.7-1.0 (more yellow)
			b := 0.1 + float32(RandInt(20))/100.0 // 0.1-0.3

			Particles[ParticleCount] = Particle{
				X:       px,
				Y:       py,
				VX:      vx,
				VY:      vy,
				Life:    8 + RandInt(10),
				MaxLife: 18,
				Size:    3 + float32(RandInt(4)),
				R:       r,
				G:       g,
				B:       b,
			}
			ParticleCount++
		}
	}

	// Occasionally spawn smoke particles
	if RandInt(3) == 0 && ParticleCount < MAX_PARTICLES {
		px := TheBall.X - TheBall.DX*12 + float32(RandInt(6)-3)
		py := TheBall.Y - TheBall.DY*12 + float32(RandInt(6)-3)

		gray := 0.3 + float32(RandInt(20))/100.0

		Particles[ParticleCount] = Particle{
			X:       px,
			Y:       py,
			VX:      -TheBall.DX*0.5 + float32(RandInt(10)-5)/10.0,
			VY:      -TheBall.DY*0.5 + float32(RandInt(10)-5)/10.0,
			Life:    12 + RandInt(8),
			MaxLife: 20,
			Size:    4 + float32(RandInt(3)),
			R:       gray,
			G:       gray,
			B:       gray,
		}
		ParticleCount++
	}
}

// UpdateScreenShake applies and decays screen shake
func UpdateScreenShake() {
	if ScreenShakeTime > 0 {
		intensity := float32(ScreenShakeTime) * 0.5
		ScreenShakeX = float32(RandInt(int(intensity*2)+1)) - intensity
		ScreenShakeY = float32(RandInt(int(intensity*2)+1)) - intensity
		ScreenShakeTime--
	} else {
		ScreenShakeX = 0
		ScreenShakeY = 0
	}
}

// TriggerScreenShake starts a screen shake effect
func TriggerScreenShake(intensity int) {
	if intensity > ScreenShakeTime {
		ScreenShakeTime = intensity
	}
}

// UpdateStreakLines manages hyperspace streak effect
func UpdateStreakLines() {
	// Activate streaks at high ball speeds
	StreaksActive = TheBall.Speed > 12

	if StreaksActive {
		for i := range StreakLines {
			StreakLines[i].X -= StreakLines[i].Speed
			if StreakLines[i].X < -StreakLines[i].Length {
				// Reset streak
				StreakLines[i].X = float32(WIDTH) + float32(RandInt(100))
				StreakLines[i].Y = float32(RandInt(HEIGHT))
				StreakLines[i].Length = 30 + float32(RandInt(70))
				StreakLines[i].Speed = 8 + float32(RandInt(12))
				StreakLines[i].Brightness = 0.3 + float32(RandInt(40))/100.0
			}
		}
	}
}

// InitStreakLines sets up hyperspace streaks
func InitStreakLines() {
	for i := range StreakLines {
		StreakLines[i] = StreakLine{
			X:          float32(RandInt(WIDTH)),
			Y:          float32(RandInt(HEIGHT)),
			Length:     30 + float32(RandInt(70)),
			Speed:      8 + float32(RandInt(12)),
			Brightness: 0.3 + float32(RandInt(40))/100.0,
		}
	}
}

// Cos32 returns cosine of angle in radians
func Cos32(angle float32) float32 {
	// Simple Taylor series approximation
	angle = angle - float32(int(angle/(2*3.14159)))*2*3.14159
	if angle < 0 {
		angle = -angle
	}
	x2 := angle * angle
	return 1 - x2/2 + x2*x2/24 - x2*x2*x2/720
}

// Sin32 returns sine of angle in radians
func Sin32(angle float32) float32 {
	return Cos32(angle - 3.14159/2)
}

// InitStars initializes the starfield for animated background
func InitStars() {
	if StarsInitialized {
		return
	}

	idx := 0

	// Far stars (slow, dim, small)
	for i := 0; i < NUM_STARS_FAR; i++ {
		Stars[idx] = Star{
			X:      float32(RandInt(WIDTH)),
			Y:      float32(RandInt(HEIGHT)),
			Speed:  0.2 + float32(RandInt(30))/100.0, // 0.2-0.5
			Size:   1,
			Bright: uint8(80 + RandInt(60)), // 80-140
		}
		idx++
	}

	// Mid stars (medium speed and brightness)
	for i := 0; i < NUM_STARS_MID; i++ {
		Stars[idx] = Star{
			X:      float32(RandInt(WIDTH)),
			Y:      float32(RandInt(HEIGHT)),
			Speed:  0.5 + float32(RandInt(50))/100.0, // 0.5-1.0
			Size:   1,
			Bright: uint8(140 + RandInt(60)), // 140-200
		}
		idx++
	}

	// Near stars (fast, bright, can be larger)
	for i := 0; i < NUM_STARS_NEAR; i++ {
		Stars[idx] = Star{
			X:      float32(RandInt(WIDTH)),
			Y:      float32(RandInt(HEIGHT)),
			Speed:  1.0 + float32(RandInt(100))/100.0, // 1.0-2.0
			Size:   float32(1 + RandInt(2)),           // 1-2
			Bright: uint8(200 + RandInt(55)),          // 200-255
		}
		idx++
	}

	StarsInitialized = true
}

// UpdateStars moves stars for parallax scrolling effect
// Stars move faster as the game gets more intense (based on ball speed)
func UpdateStars() {
	// Calculate speed multiplier based on ball speed
	// Ball starts at speed 5, gets faster with each hit
	// At speed 5: multiplier = 1.0
	// At speed 15: multiplier = 2.0
	// At speed 25+: multiplier = 3.0
	StarSpeedMult = float32(1.0)
	if TheBall.Speed > 5 {
		StarSpeedMult = 1.0 + float32(TheBall.Speed-5)*0.1
		if StarSpeedMult > 3.0 {
			StarSpeedMult = 3.0
		}
	}

	for i := range Stars {
		// Move star to the left (simulating forward motion through space)
		Stars[i].X -= Stars[i].Speed * StarSpeedMult

		// Wrap around when off screen
		if Stars[i].X < -Stars[i].Size {
			Stars[i].X = float32(WIDTH) + Stars[i].Size
			Stars[i].Y = float32(RandInt(HEIGHT))
		}
	}
}

// InitGame initializes the game state
func InitGame(p1Human, p2Human bool) {
	// Initialize starfield and effects
	InitStars()
	InitStreakLines()
	ParticleCount = 0
	// Initialize bats
	Bats[0].X = float32(BAT_LEFT_X)
	Bats[0].Y = float32(HALF_HEIGHT)
	Bats[0].Score = 0
	Bats[0].Timer = 0
	Bats[0].IsHuman = p1Human

	Bats[1].X = float32(BAT_RIGHT_X)
	Bats[1].Y = float32(HALF_HEIGHT)
	Bats[1].Score = 0
	Bats[1].Timer = 0
	Bats[1].IsHuman = p2Human

	// Initialize ball
	InitBall(-1)
	AIOffset = 0
}

// InitBall initializes the ball position and direction
func InitBall(direction float32) {
	TheBall.X = float32(HALF_WIDTH)
	TheBall.Y = float32(HALF_HEIGHT)
	TheBall.DX = direction
	TheBall.DY = 0
	TheBall.Speed = BALL_START_SPEED
}

// UpdateGame updates all game entities
func UpdateGame() {
	// Update starfield
	UpdateStars()

	// Update advanced effects
	UpdateParticles()
	UpdateBallTrail()
	UpdateScreenShake()
	UpdateStreakLines()
	LightningTimer++

	// Update bats
	UpdateBat(&Bats[0], 0)
	UpdateBat(&Bats[1], 1)

	// Update ball
	UpdateBall()

	// Update effects
	if ImpactTimer > 0 {
		ImpactTimer--
	}
	if GoalEffectTimer > 0 {
		GoalEffectTimer--
	}
}

// UpdateBat updates a single bat
func UpdateBat(bat *Bat, player int) {
	bat.Timer--

	var movement float32
	if bat.IsHuman {
		movement = GetPlayerMovement(player)
	} else {
		movement = GetAIMovement(bat)
	}

	bat.Y = Clamp32(bat.Y+movement, float32(ARENA_TOP+BAT_HEIGHT/2), float32(ARENA_BOTTOM-BAT_HEIGHT/2))
}

// GetPlayerMovement returns movement for human player
func GetPlayerMovement(player int) float32 {
	if player == 0 {
		// Player 1 always uses controller 1 D-Pad
		if IsHeld(kos.CONT_DPAD_DOWN) {
			return PLAYER_SPEED
		}
		if IsHeld(kos.CONT_DPAD_UP) {
			return -PLAYER_SPEED
		}
	} else {
		// Player 2 uses controller 2 D-Pad in 2P mode
		if IsHeld2(kos.CONT_DPAD_DOWN) {
			return PLAYER_SPEED
		}
		if IsHeld2(kos.CONT_DPAD_UP) {
			return -PLAYER_SPEED
		}
	}
	return 0
}

// GetAIMovement returns movement for AI player
// Made easier to beat with noticeable imperfections
func GetAIMovement(bat *Bat) float32 {
	xDist := Abs32(TheBall.X - bat.X)

	// Target 1: center of screen (when ball is far, we don't know where it will end up)
	targetY1 := float32(HALF_HEIGHT)

	// Target 2: ball position + random offset (when ball is close)
	targetY2 := TheBall.Y + float32(AIOffset)

	// Weighted average: when ball is FAR, weight1 is HIGH (go to center)
	// when ball is CLOSE, weight1 is LOW (track ball position)
	weight1 := Min32(1, xDist/float32(HALF_WIDTH))
	weight2 := 1 - weight1

	targetY := weight1*targetY1 + weight2*targetY2

	diff := targetY - bat.Y

	// === IMPERFECTIONS TO MAKE AI BEATABLE ===

	// 1. Reaction delay: AI is much slower when ball moving away
	if (TheBall.DX > 0 && bat.X > float32(HALF_WIDTH)) ||
		(TheBall.DX < 0 && bat.X < float32(HALF_WIDTH)) {
		// Ball moving toward this bat - react normally
	} else {
		// Ball moving away - barely move
		diff = (float32(HALF_HEIGHT) - bat.Y) * 0.2
	}

	// 2. Random hesitation (10% chance to pause)
	if RandInt(100) < 10 {
		return 0
	}

	// 3. Reduced max speed (80% of player speed)
	aiSpeed := float32(MAX_AI_SPEED) * 0.80

	return Clamp32(diff, -aiSpeed, aiSpeed)
}

// UpdateBall updates ball position and handles collisions
func UpdateBall() {
	for i := 0; i < TheBall.Speed; i++ {
		originalX := TheBall.X

		TheBall.X += TheBall.DX
		TheBall.Y += TheBall.DY

		// Bat collision
		if Abs32(TheBall.X-float32(HALF_WIDTH)) >= COLLISION_X &&
			Abs32(originalX-float32(HALF_WIDTH)) < COLLISION_X {

			var bat *Bat
			if TheBall.X < float32(HALF_WIDTH) {
				bat = &Bats[0]
			} else {
				bat = &Bats[1]
			}

			diffY := TheBall.Y - bat.Y
			if diffY > -60 && diffY < 60 {
				// Hit!
				TheBall.DX = -TheBall.DX
				TheBall.DY += diffY / 128
				TheBall.DY = Clamp32(TheBall.DY, -1, 1)
				TheBall.DX, TheBall.DY = Normalize(TheBall.DX, TheBall.DY)
				TheBall.Speed++
				AIOffset = RandInt(51) - 25 // Larger offset makes AI miss more
				bat.Timer = 10

				// Trigger impact effect
				ImpactX = TheBall.X
				ImpactY = TheBall.Y
				ImpactTimer = 15

				// === PARTICLE EXPLOSION ===
				// Spawn particles based on ball speed
				particleCount := 15 + TheBall.Speed*2
				if particleCount > 50 {
					particleCount = 50
				}
				// Color based on which bat hit
				if bat.X < float32(HALF_WIDTH) {
					// Green bat hit - green particles
					SpawnParticles(TheBall.X, TheBall.Y, particleCount, 0.2, 1.0, 0.4, 4.0)
				} else {
					// Red bat hit - red particles
					SpawnParticles(TheBall.X, TheBall.Y, particleCount, 1.0, 0.3, 0.3, 4.0)
				}

				// Screen shake on hard hits
				if TheBall.Speed > 10 {
					TriggerScreenShake(TheBall.Speed / 3)
				}

				// Play hit sounds
				PlaySoundRandom(SndHit[:])
				PlaySound(SndHitSynth)
				if TheBall.Speed <= 10 {
					PlaySound(SndHitSlow)
				} else if TheBall.Speed <= 12 {
					PlaySound(SndHitMedium)
				} else if TheBall.Speed <= 16 {
					PlaySound(SndHitFast)
				} else {
					PlaySound(SndHitVeryFast)
				}
			}
		}

		// Wall collision
		if TheBall.Y < float32(ARENA_TOP) || TheBall.Y > float32(ARENA_BOTTOM) {
			TheBall.DY = -TheBall.DY
			TheBall.Y += TheBall.DY

			// Wall spark particles
			SpawnDirectionalParticles(TheBall.X, TheBall.Y, 0, -TheBall.DY, 8, 0.6, 0.8, 1.0)

			PlaySoundRandom(SndBounce[:])
			PlaySound(SndBounceSynth)
		}
	}

	// Check scoring
	if TheBall.X < 0 || TheBall.X > float32(WIDTH) {
		var scoringPlayer, losingPlayer int
		if TheBall.X < float32(HALF_WIDTH) {
			scoringPlayer = 1
			losingPlayer = 0
		} else {
			scoringPlayer = 0
			losingPlayer = 1
		}

		if Bats[losingPlayer].Timer < 0 {
			Bats[scoringPlayer].Score++
			Bats[losingPlayer].Timer = 20
			GoalEffectTimer = 30
			GoalEffectPlayer = losingPlayer

			// === GOAL EXPLOSION ===
			// Big screen shake
			TriggerScreenShake(15)

			// Massive particle burst at ball position
			goalX := TheBall.X
			if goalX < 0 {
				goalX = 20
			}
			if goalX > float32(WIDTH) {
				goalX = float32(WIDTH) - 20
			}

			// Explosion of particles
			for wave := 0; wave < 3; wave++ {
				if scoringPlayer == 0 {
					SpawnParticles(goalX, TheBall.Y, 30, 0.2, 1.0, 0.4, 6.0+float32(wave)*2)
				} else {
					SpawnParticles(goalX, TheBall.Y, 30, 1.0, 0.3, 0.3, 6.0+float32(wave)*2)
				}
			}

			PlaySound(SndScoreGoal)
		} else if Bats[losingPlayer].Timer == 0 {
			var dir float32 = -1
			if losingPlayer == 1 {
				dir = 1
			}
			InitBall(dir)
		}
	}
}

// IsBallOut returns true if ball is off screen
func IsBallOut() bool {
	return TheBall.X < 0 || TheBall.X > float32(WIDTH)
}

// UpdateMenu handles menu state
func UpdateMenu() {
	// Always update stars for animated background
	UpdateStars()

	if JustPressed(kos.CONT_DPAD_UP) && NumPlayers == 2 {
		NumPlayers = 1
		PlaySound(SndUp)
	}
	if JustPressed(kos.CONT_DPAD_DOWN) && NumPlayers == 1 {
		NumPlayers = 2
		PlaySound(SndDown)
	}

	if JustPressed(kos.CONT_START) || JustPressed(kos.CONT_A) {
		GameState = STATE_PLAY
		InitGame(true, NumPlayers == 2)
		PlaySound(SndHit[0])
	}
}

// UpdatePlay handles play state
func UpdatePlay() {
	// Pause
	if JustPressed(kos.CONT_START) {
		for {
			ReadInput()
			if JustPressed(kos.CONT_START) {
				break
			}
			Render()
		}
	}

	// Check win
	if Bats[0].Score >= WIN_SCORE || Bats[1].Score >= WIN_SCORE {
		GameState = STATE_GAME_OVER
		return
	}

	UpdateGame()
}

// UpdateGameOver handles game over state
func UpdateGameOver() {
	// Always update stars for animated background
	UpdateStars()

	if JustPressed(kos.CONT_START) || JustPressed(kos.CONT_A) {
		GameState = STATE_MENU
		NumPlayers = 1
		InitGame(false, false)
	}
}
