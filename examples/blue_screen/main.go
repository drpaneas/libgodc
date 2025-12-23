// Minimal graphics - just a blue background
package main

import "kos"

func main() {
	kos.PvrInitDefaults()
	kos.PvrSetBgColor(0.0, 0.0, 1.0)

	for {
		kos.PvrWaitReady()
		kos.PvrSceneBegin()
		kos.PvrSceneFinish()
	}
}
