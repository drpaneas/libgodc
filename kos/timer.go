//go:build gccgo

package kos

const (
	TMU0 = 0
	TMU1 = 1
	TMU2 = 2
)

//extern timer_ms_enable
func TimerMsEnable()

//extern timer_ms_disable
func TimerMsDisable()

//extern timer_ms_gettime64
func TimerMsGettime64() uint64

//extern timer_us_gettime64
func TimerUsGettime64() uint64

//extern timer_ns_gettime64
func TimerNsGettime64() uint64

//extern timer_spin_sleep
func TimerSpinSleep(ms int32)

//extern timer_spin_delay_us
func TimerSpinDelayUs(us uint16)

//extern timer_spin_delay_ns
func TimerSpinDelayNs(ns uint16)

//extern thd_sleep
func ThdSleep(ms uint32)

//extern timer_prime
func TimerPrime(channel int32, speed uint32, interrupts int32) int32

//extern timer_start
func TimerStart(channel int32) int32

//extern timer_stop
func TimerStop(channel int32) int32

//extern timer_running
func TimerRunning(channel int32) int32

//extern timer_count
func TimerCount(channel int32) uint32

//extern timer_clear
func TimerClear(channel int32) int32

//extern timer_enable_ints
func TimerEnableInts(channel int32)

//extern timer_disable_ints
func TimerDisableInts(channel int32)

//extern timer_ints_enabled
func TimerIntsEnabled(channel int32) int32

//extern timer_init
func TimerInit() int32

//extern timer_shutdown
func TimerShutdown()
