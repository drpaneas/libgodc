//go:build !gccgo

package kos

const (
	PERF_COUNTER_DISABLE   = 0
	PERF_COUNTER_CYCLES    = 1
	PERF_COUNTER_OC_MISS   = 2
	PERF_COUNTER_OC_HIT    = 3
	PERF_COUNTER_IC_MISS   = 4
	PERF_COUNTER_IC_HIT    = 5
	PERF_COUNTER_BRANCH    = 6
	PERF_COUNTER_UTLB_MISS = 7
	PERF_COUNTER_UTLB_HIT  = 8
)

func perfGetCycles(counter int32) uint64                    { return 0 }
func perfCntrStart(counter int32, mode int32, start uint64) {}
func perfCntrStop(counter int32) uint64                     { return 0 }
func perfCntrClear(counter int32)                           {}
func PerfCntrStart()                                        {}
func PerfCntrStop() uint64                                  { return 0 }
func PerfCntrCycles() uint64                                { return 0 }
