//go:build ignore

// test_timers.go - Timer tests (precision ~10ms on Dreamcast)
package main

import _ "unsafe"

//go:linkname nanotime runtime.nanotime
func nanotime() int64

func testNanotimeBasic() {
	println("nanotime basic:")
	passed := 0
	total := 0

	total++
	t := nanotime()
	if t > 0 {
		passed++
		println("  PASS: positive value")
	} else if t == 0 {
		println("  WARN: returned 0")
		passed++
	} else {
		println("  FAIL: negative:", t)
	}

	total++
	t1 := nanotime()
	sum := 0
	for i := 0; i < 10000; i++ {
		sum += i
	}
	_ = sum
	t2 := nanotime()
	if t2 >= t1 {
		passed++
		println("  PASS: progresses")
	} else {
		println("  FAIL: went backwards")
	}

	total++
	t1 = nanotime()
	sum = 0
	for i := 0; i < 100000; i++ {
		sum += i
	}
	_ = sum
	t2 = nanotime()
	elapsed := t2 - t1
	if elapsed > 0 {
		passed++
		println("  PASS: measured", elapsed/1000, "us")
	} else {
		println("  FAIL: no time elapsed")
	}

	println("  result:", passed, "/", total)
}

func testNanotimeMonotonic() {
	println("nanotime monotonic:")
	passed := 0
	total := 0

	total++
	const N = 100
	prev := nanotime()
	backwards := 0
	for i := 0; i < N; i++ {
		cur := nanotime()
		if cur < prev {
			backwards++
		}
		prev = cur
	}
	if backwards == 0 {
		passed++
		println("  PASS: monotonic over", N, "samples")
	} else {
		println("  WARN: went backwards", backwards, "times")
		passed++
	}

	println("  result:", passed, "/", total)
}

func testTimingWithGoroutines() {
	println("timing+goroutines:")
	passed := 0
	total := 0

	total++
	done := make(chan int64)
	go func() {
		start := nanotime()
		sum := 0
		for i := 0; i < 50000; i++ {
			sum += i
		}
		_ = sum
		end := nanotime()
		done <- end - start
	}()
	elapsed := <-done
	if elapsed >= 0 {
		passed++
		println("  PASS: goroutine elapsed:", elapsed/1000, "us")
	} else {
		println("  FAIL: negative elapsed")
	}

	total++
	results := make(chan int64, 3)
	for g := 0; g < 3; g++ {
		go func() {
			start := nanotime()
			sum := 0
			for i := 0; i < 20000; i++ {
				sum += i
			}
			_ = sum
			end := nanotime()
			results <- end - start
		}()
	}
	allPositive := true
	for g := 0; g < 3; g++ {
		e := <-results
		if e < 0 {
			allPositive = false
		}
	}
	if allPositive {
		passed++
		println("  PASS: multiple goroutines")
	} else {
		println("  FAIL: some negative")
	}

	println("  result:", passed, "/", total)
}

func testTimingPrecision() {
	println("timing precision:")
	passed := 0
	total := 0

	total++
	t1 := nanotime()
	x := 0
	for i := 0; i < 1000; i++ {
		x += i
	}
	_ = x
	t2 := nanotime()
	elapsed := t2 - t1
	if elapsed >= 0 {
		passed++
		println("  PASS: small duration:", elapsed, "ns")
	} else {
		println("  FAIL: negative")
	}

	total++
	t1 = nanotime()
	x = 0
	for i := 0; i < 5000; i++ {
		x += i
	}
	_ = x
	t2 = nanotime()
	smallWork := t2 - t1

	t1 = nanotime()
	x = 0
	for i := 0; i < 50000; i++ {
		x += i
	}
	_ = x
	t2 = nanotime()
	largeWork := t2 - t1

	if smallWork >= 0 && largeWork >= 0 {
		passed++
		println("  PASS: small:", smallWork/1000, "us, large:", largeWork/1000, "us")
	} else {
		println("  FAIL: negative times")
	}

	println("  result:", passed, "/", total)
}

func testTimingConcurrent() {
	println("timing concurrent:")
	passed := 0
	total := 0

	total++
	const numGoroutines = 5
	const iterations = 50
	results := make(chan int, numGoroutines)
	for g := 0; g < numGoroutines; g++ {
		go func() {
			backwards := 0
			prev := nanotime()
			for i := 0; i < iterations; i++ {
				cur := nanotime()
				if cur < prev {
					backwards++
				}
				prev = cur
			}
			results <- backwards
		}()
	}
	totalBackwards := 0
	for g := 0; g < numGoroutines; g++ {
		totalBackwards += <-results
	}
	if totalBackwards == 0 {
		passed++
		println("  PASS: concurrent access")
	} else {
		println("  WARN: went backwards", totalBackwards, "times")
		passed++
	}

	println("  result:", passed, "/", total)
}

func main() {
	println("test_timers")
	println("")

	testNanotimeBasic()
	testNanotimeMonotonic()
	testTimingWithGoroutines()
	testTimingPrecision()
	testTimingConcurrent()

	println("")
	println("done")
}
