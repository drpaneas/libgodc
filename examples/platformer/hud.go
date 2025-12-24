package main

const (
	HUDBarWidth    float32 = 140
	HUDBarHeight   float32 = 20
	HUDBarPadding  float32 = 2
	HUDPanelWidth  float32 = 200
	HUDPanelHeight float32 = 30
	HUDPanelY      float32 = 40
	HUDPanelMargin float32 = 8
	HUDStateWidth  float32 = 120
	HUDStateHeight float32 = 28
	HUDStateY      float32 = 8
)

func drawHUD() {
	drawVelocityBars()
	drawStateIndicator()
}

func drawVelocityBars() {
	panelX := HUDPanelMargin
	barX := panelX + (HUDPanelWidth-HUDBarWidth)/2
	drawRect(panelX, HUDPanelY, HUDPanelWidth, HUDPanelHeight, ZHUDBackground, colorPanel)
	drawVelocityBar(barX, HUDPanelY+5, HUDBarWidth, player.vx, physics.runMax)

	panelX = ScreenWidth - HUDPanelWidth - HUDPanelMargin
	barX = panelX + (HUDPanelWidth-HUDBarWidth)/2
	drawRect(panelX, HUDPanelY, HUDPanelWidth, HUDPanelHeight, ZHUDBackground, colorPanel)
	drawVelocityBar(barX, HUDPanelY+5, HUDBarWidth, player.vy, physics.jumpVelFast)
}

func drawVelocityBar(x, y, w float32, val, maxVal float32) {
	drawRect(x, y, w, HUDBarHeight, ZHUDForeground, colorBarBg)

	bar := (val / maxVal) * (w / 2)
	col := colorBarFill
	if abs32(val) > maxVal*HighVelocityRatio {
		col = colorBarHigh
	}

	center := x + w/2
	if bar > 0 {
		drawRect(center, y+HUDBarPadding, bar, HUDBarHeight-HUDBarPadding*2, ZHUDBar, col)
	} else {
		drawRect(center+bar, y+HUDBarPadding, -bar, HUDBarHeight-HUDBarPadding*2, ZHUDBar, col)
	}

	drawRect(center-1, y, 2, HUDBarHeight, ZHUDLine, colorBarLine)
}

func drawStateIndicator() {
	x := ScreenWidth/2 - HUDStateWidth/2
	drawRect(x, HUDStateY, HUDStateWidth, HUDStateHeight, ZHUDBackground, colorState)
}

