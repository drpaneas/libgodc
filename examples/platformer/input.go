package main

import "kos"

var (
	buttonsNow  uint32
	buttonsPrev uint32
)

func readInput() {
	buttonsPrev = buttonsNow

	ctrl := kos.MapleEnumType(0, kos.MAPLE_FUNC_CONTROLLER)
	if ctrl == nil {
		buttonsNow = 0
		return
	}

	state := ctrl.ContState()
	if state == nil {
		buttonsNow = 0
		return
	}

	buttonsNow = state.Buttons
}

func isHeld(btn uint32) bool {
	return (buttonsNow & btn) != 0
}

func justPressed(btn uint32) bool {
	return (buttonsNow&btn) != 0 && (buttonsPrev&btn) == 0
}

