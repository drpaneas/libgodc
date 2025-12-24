package main

import "kos"

func abs32(x float32) float32 {
	if x < 0 {
		return -x
	}
	return x
}

func drawRect(x, y, w, h, z float32, color uint32) {
	kos.PlxVertInp(kos.PLX_VERT, x, y+h, z, color)
	kos.PlxVertInp(kos.PLX_VERT, x, y, z, color)
	kos.PlxVertInp(kos.PLX_VERT, x+w, y+h, z, color)
	kos.PlxVertInp(kos.PLX_VERT_EOS, x+w, y, z, color)
}

func drawLine(x1, y1, x2, y2, z, thickness float32, color uint32) {
	if x1 == x2 {
		drawRect(x1-thickness/2, y1, thickness, y2-y1, z, color)
	} else {
		drawRect(x1, y1-thickness/2, x2-x1, thickness, z, color)
	}
}

