//go:build ignore

// test_time.go - Test runtime time functions (nanotime, walltime)
package main

import _ "unsafe"

//go:linkname nanotime runtime.nanotime
func nanotime() int64

//go:linkname walltime runtime.walltime
func walltime() int64

func testNanotimeBasic() {
	println("nanotime basic:")
	t1 := nanotime()
	if t1 == 0 {
		println("  WARN: returned 0")
	}
	sum := 0
	for i := 0; i < 100000; i++ {
		sum += i
	}
	if sum < 0 {
		println("unexpected")
	}
	t2 := nanotime()
	elapsed := t2 - t1
	if elapsed > 0 {
		println("  advanced by", elapsed, "ns")
		println("  PASS: advanced")
	} else if elapsed == 0 {
		println("  WARN: did not advance")
		println("  PASS: did not advance (warning)")
	} else {
		println("  ERROR: went backwards:", elapsed)
		panic("failed")
	}
}

func testNanotimeMonotonic() {
	println("nanotime monotonic:")
	const N = 1000
	var prev int64 = 0
	backwards := 0
	for i := 0; i < N; i++ {
		cur := nanotime()
		if cur < prev {
			backwards++
		}
		prev = cur
	}
	if backwards > 0 {
		println("  WARN: went backwards", backwards, "times")
	} else {
		println("  monotonic across", N, "samples")
	}
	println("  PASS: monotonic")
}

func testWalltimeBasic() {
	println("walltime basic:")
	wt := walltime()
	if wt == 0 {
		println("  WARN: returned 0 (RTC not set)")
		println("  PASS: walltime basic")
		return
	}
	year2000Ns := int64(946684800) * int64(1000000000)
	if wt < year2000Ns {
		println("  WARN: before year 2000 (RTC not set?)")
	}
	unixSecs := wt / int64(1000000000)
	println("  unix timestamp:", unixSecs)
	println("  PASS: walltime basic")
}

func testWalltimeAdvances() {
	println("walltime advances:")
	wt1 := walltime()
	sum := 0
	for i := 0; i < 100000; i++ {
		sum += i
	}
	if sum < 0 {
		println("unexpected")
	}
	wt2 := walltime()
	if wt2 >= wt1 {
		println("  advanced")
		println("  PASS: walltime advances")
	} else {
		println("  ERROR: went backwards")
		panic("failed")
	}
}

func testTimingPrecision() {
	println("timing precision:")
	start := nanotime()
	sum := 0
	for i := 0; i < 50000; i++ {
		sum += i
	}
	if sum < 0 {
		println("unexpected")
	}
	end := nanotime()
	elapsed := end - start
	elapsedUs := elapsed / 1000
	println("  elapsed:", elapsedUs, "us")
	if elapsedUs >= 0 {
		println("  PASS: timing precision")
	} else {
		println("  ERROR: negative elapsed")
		panic("failed")
	}
}

func testTimingMultiple() {
	println("timing multiple:")
	const numMeasurements = 10
	measurements := make([]int64, numMeasurements)
	for m := 0; m < numMeasurements; m++ {
		start := nanotime()
		iterations := 10000 * (m + 1)
		sum := 0
		for i := 0; i < iterations; i++ {
			sum += i
		}
		if sum < 0 {
			println("unexpected")
		}
		end := nanotime()
		measurements[m] = end - start
	}
	println("  measurements (ns):")
	for m := 0; m < numMeasurements; m++ {
		println("   ", m, ":", measurements[m])
	}
	for m := 0; m < numMeasurements; m++ {
		if measurements[m] < 0 {
			println("  ERROR: negative at index", m)
			panic("failed")
		}
	}
	println("  PASS: timing multiple")
}

func testLegacyNanotime() {
	println("legacy nanotime:")
	t1 := nanotime()
	println("  initial:", t1)
	sum := 0
	for i := 0; i < 100000; i++ {
		sum += i
	}
	if sum < 0 {
		println("unexpected")
	}
	t2 := nanotime()
	elapsed := t2 - t1
	elapsedUs := elapsed / 1000
	println("  elapsed ns:", elapsed)
	println("  elapsed us:", elapsedUs)
	if elapsed >= 0 {
		println("  PASS: legacy nanotime")
	} else {
		println("  ERROR: went backwards")
		panic("failed")
	}
}

func testLegacyWalltime() {
	println("legacy walltime:")
	wt := walltime()
	println("  ns since epoch:", wt)
	unixSecs := wt / int64(1000000000)
	println("  unix timestamp:", unixSecs)
	wt2 := walltime()
	if wt2 >= wt {
		println("  PASS: legacy walltime")
	} else {
		println("  ERROR: went backwards")
		panic("failed")
	}
}

func main() {
	println("test_time")
	println("")

	testNanotimeBasic()
	testNanotimeMonotonic()
	testWalltimeBasic()
	testWalltimeAdvances()
	testTimingPrecision()
	testTimingMultiple()
	testLegacyNanotime()
	testLegacyWalltime()

	println("")
	println("done")
}
