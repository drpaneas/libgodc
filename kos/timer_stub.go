//go:build !gccgo

package kos

const (
	TMU0 = 0
	TMU1 = 1
	TMU2 = 2
)

func TimerMsEnable()           { panic("kos: not on Dreamcast") }
func TimerMsDisable()          { panic("kos: not on Dreamcast") }
func TimerMsGettime64() uint64 { panic("kos: not on Dreamcast") }
func TimerUsGettime64() uint64 { panic("kos: not on Dreamcast") }
func TimerNsGettime64() uint64 { panic("kos: not on Dreamcast") }

func TimerSpinSleep(ms int32)    { panic("kos: not on Dreamcast") }
func TimerSpinDelayUs(us uint16) { panic("kos: not on Dreamcast") }
func TimerSpinDelayNs(ns uint16) { panic("kos: not on Dreamcast") }
func ThdSleep(ms uint32)         { panic("kos: not on Dreamcast") }

func TimerPrime(channel int32, speed uint32, interrupts int32) int32 {
	panic("kos: not on Dreamcast")
}
func TimerStart(channel int32) int32       { panic("kos: not on Dreamcast") }
func TimerStop(channel int32) int32        { panic("kos: not on Dreamcast") }
func TimerRunning(channel int32) int32     { panic("kos: not on Dreamcast") }
func TimerCount(channel int32) uint32      { panic("kos: not on Dreamcast") }
func TimerClear(channel int32) int32       { panic("kos: not on Dreamcast") }
func TimerEnableInts(channel int32)        { panic("kos: not on Dreamcast") }
func TimerDisableInts(channel int32)       { panic("kos: not on Dreamcast") }
func TimerIntsEnabled(channel int32) int32 { panic("kos: not on Dreamcast") }

func TimerInit() int32 { panic("kos: not on Dreamcast") }
func TimerShutdown()   { panic("kos: not on Dreamcast") }
