// input.go - Controller input handling
package main

import "kos"

// Controller 2 state (Player 2 in 2P mode)
// Note: ButtonsNow/ButtonsPrev for controller 1 are in state.go
var (
	Buttons2Now  uint32
	Buttons2Prev uint32
)

func ReadInput() {
	ButtonsPrev = ButtonsNow
	Buttons2Prev = Buttons2Now

	// Read controller 1
	ctrl := kos.MapleEnumType(0, kos.MAPLE_FUNC_CONTROLLER)
	if ctrl == nil {
		ButtonsNow = 0
	} else {
		state := ctrl.ContState()
		if state == nil {
			ButtonsNow = 0
		} else {
			ButtonsNow = state.Buttons
		}
	}

	// Read controller 2 only in 2P mode
	if NumPlayers == 2 {
		ctrl2 := kos.MapleEnumType(1, kos.MAPLE_FUNC_CONTROLLER)
		if ctrl2 == nil {
			Buttons2Now = 0
		} else {
			state2 := ctrl2.ContState()
			if state2 == nil {
				Buttons2Now = 0
			} else {
				Buttons2Now = state2.Buttons
			}
		}
	} else {
		Buttons2Now = 0
	}
}

func JustPressed(btn uint32) bool {
	return (ButtonsNow&btn) != 0 && (ButtonsPrev&btn) == 0
}

func IsHeld(btn uint32) bool {
	return (ButtonsNow & btn) != 0
}

func JustPressed2(btn uint32) bool {
	return (Buttons2Now&btn) != 0 && (Buttons2Prev&btn) == 0
}

func IsHeld2(btn uint32) bool {
	return (Buttons2Now & btn) != 0
}
