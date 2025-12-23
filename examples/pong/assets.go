// assets.go - Asset loading (textures and sounds)
package main

import "kos"

// InitColors initializes color values (must be called after PVR init)
func InitColors() {
	ColorWhite = kos.PlxPackColor(1, 1, 1, 1)
	ColorGreen = kos.PlxPackColor(1, 0.2, 0.8, 0.2)
	ColorBlue = kos.PlxPackColor(1, 0.2, 0.4, 0.9)
	ColorYellow = kos.PlxPackColor(1, 0.9, 0.9, 0.2)
	ColorGray = kos.PlxPackColor(1, 0.5, 0.5, 0.5)
	ColorDark = kos.PlxPackColor(1, 0.05, 0.02, 0.12) // Dark blue-purple for deep space
}

// LoadSounds loads all sound effects from romdisk
func LoadSounds() {
	println("Loading sounds...")
	kos.SndStreamInit()

	// Bounce sounds (ball hitting wall)
	SndBounce[0] = kos.SndSfxLoad("/rd/bounce0.wav")
	SndBounce[1] = kos.SndSfxLoad("/rd/bounce1.wav")
	SndBounce[2] = kos.SndSfxLoad("/rd/bounce2.wav")
	SndBounce[3] = kos.SndSfxLoad("/rd/bounce3.wav")
	SndBounce[4] = kos.SndSfxLoad("/rd/bounce4.wav")
	SndBounceSynth = kos.SndSfxLoad("/rd/bounce_synth0.wav")

	// Hit sounds (ball hitting bat)
	SndHit[0] = kos.SndSfxLoad("/rd/hit0.wav")
	SndHit[1] = kos.SndSfxLoad("/rd/hit1.wav")
	SndHit[2] = kos.SndSfxLoad("/rd/hit2.wav")
	SndHit[3] = kos.SndSfxLoad("/rd/hit3.wav")
	SndHit[4] = kos.SndSfxLoad("/rd/hit4.wav")
	SndHitSlow = kos.SndSfxLoad("/rd/hit_slow0.wav")
	SndHitMedium = kos.SndSfxLoad("/rd/hit_medium0.wav")
	SndHitFast = kos.SndSfxLoad("/rd/hit_fast0.wav")
	SndHitVeryFast = kos.SndSfxLoad("/rd/hit_veryfast0.wav")
	SndHitSynth = kos.SndSfxLoad("/rd/hit_synth0.wav")

	// Other sounds
	SndScoreGoal = kos.SndSfxLoad("/rd/score_goal0.wav")
	SndUp = kos.SndSfxLoad("/rd/up.wav")
	SndDown = kos.SndSfxLoad("/rd/down.wav")

	println("Sounds loaded!")
}

// LoadTextures loads all textures from romdisk
func LoadTextures() {
	println("Loading textures...")

	// Goal effect textures
	TexEffect0 = kos.PlxTxrLoad("/rd/effect0.png", true, 0)
	TexEffect1 = kos.PlxTxrLoad("/rd/effect1.png", true, 0)

	// Ball texture
	TexBall = kos.PlxTxrLoad("/rd/ball.png", true, 0)

	// Bat textures (lightsabers)
	// Player 0 (left) - green lightsaber
	TexBat0[0] = kos.PlxTxrLoad("/rd/bat00.png", true, 0)
	TexBat0[1] = kos.PlxTxrLoad("/rd/bat01.png", true, 0)
	TexBat0[2] = kos.PlxTxrLoad("/rd/bat02.png", true, 0)
	// Player 1 (right) - red lightsaber
	TexBat1[0] = kos.PlxTxrLoad("/rd/bat10.png", true, 0)
	TexBat1[1] = kos.PlxTxrLoad("/rd/bat11.png", true, 0)
	TexBat1[2] = kos.PlxTxrLoad("/rd/bat12.png", true, 0)

	// Impact effect animation frames
	TexImpact[0] = kos.PlxTxrLoad("/rd/impact0.png", true, 0)
	TexImpact[1] = kos.PlxTxrLoad("/rd/impact1.png", true, 0)
	TexImpact[2] = kos.PlxTxrLoad("/rd/impact2.png", true, 0)
	TexImpact[3] = kos.PlxTxrLoad("/rd/impact3.png", true, 0)
	TexImpact[4] = kos.PlxTxrLoad("/rd/impact4.png", true, 0)

	println("Textures loaded:", TexBall != nil, TexBat0[0] != nil, TexBat1[0] != nil)
}

// PlaySound plays a sound effect
func PlaySound(snd kos.SfxHandle) {
	if snd != kos.SFXHND_INVALID {
		kos.SndSfxPlay(snd, 200, 128)
	}
}

// PlaySoundRandom plays a random sound from a slice
func PlaySoundRandom(sounds []kos.SfxHandle) {
	if len(sounds) > 0 {
		idx := RandInt(len(sounds))
		PlaySound(sounds[idx])
	}
}

