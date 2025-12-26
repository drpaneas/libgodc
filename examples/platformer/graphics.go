package main

import "kos"

const (
	ZBackground    float32 = 1
	ZWorld         float32 = 10
	ZHUDBackground float32 = 40
	ZHUDForeground float32 = 50
	ZHUDBar        float32 = 51
	ZHUDLine       float32 = 52
	ZSpeedLines    float32 = 99
	ZPlayer        float32 = 100
	ZPlayerEye     float32 = 101
	ZIndicator     float32 = 102
)

func initGraphics() {
	kos.PvrInitDefaults()
	kos.PvrSetBgColor(0.05, 0.05, 0.15)
}

func render() {
	kos.PvrSceneBegin()
	kos.PvrListBegin(kos.PVR_LIST_OP_POLY)

	kos.PlxCxtInit()
	kos.PlxCxtTexture(nil)
	kos.PlxCxtCulling(kos.PLX_CULL_NONE)
	kos.PlxCxtSend(int32(kos.PVR_LIST_OP_POLY))

	drawWorld()
	drawPlayer()
	drawHUD()

	kos.PvrListFinish()
	kos.PvrSceneFinish()
	kos.PvrWaitReady()
}


