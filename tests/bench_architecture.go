//go:build ignore

// bench_architecture.go - Architecture validation benchmarks
package main

import _ "unsafe"

//go:linkname nanotime runtime.nanotime
func nanotime() int64

//go:linkname forceGC runtime.GC
func forceGC()

//go:linkname goroutineStackSize runtime.goroutineStackSize
func goroutineStackSize() int32

//go:linkname largeObjectThreshold runtime.largeObjectThreshold
func largeObjectThreshold() int32

//extern runtime_gosched
func gosched()

func heavyFrame(depth int, accumulator int) int {
	if depth <= 0 {
		return accumulator
	}
	var arr [100]int
	arr[0] = depth
	arr[99] = accumulator
	result := heavyFrame(depth-1, accumulator+arr[0])
	if arr[99] != accumulator {
		return -1
	}
	return result
}

func lightFrame(depth int, accumulator int) int {
	if depth <= 0 {
		return accumulator
	}
	var arr [50]int
	arr[0] = depth
	return lightFrame(depth-1, accumulator+arr[0])
}

func tinyFrame(depth int, accumulator int) int {
	if depth <= 0 {
		return accumulator
	}
	return tinyFrame(depth-1, accumulator+depth)
}

func benchStackDepth() {
	stackKB := goroutineStackSize() / 1024
	print("stack depth (", stackKB, "KB per goroutine):\n")
	println("  heavy frames (~450 bytes):")
	for _, depth := range []int{10, 20, 25, 30, 32, 35} {
		expected := depth * (depth + 1) / 2
		ch := make(chan int, 1)
		go func(d int) {
			ch <- heavyFrame(d, 0)
		}(depth)
		result := <-ch
		status := "PASS"
		if result != expected {
			status = "FAIL"
		}
		println("    depth", depth, "->", result, "(expect", expected, ")", status)
	}

	println("  light frames (~220 bytes):")
	for _, depth := range []int{20, 40, 50, 60, 70} {
		expected := depth * (depth + 1) / 2
		ch := make(chan int, 1)
		go func(d int) {
			ch <- lightFrame(d, 0)
		}(depth)
		result := <-ch
		status := "PASS"
		if result != expected {
			status = "FAIL"
		}
		println("    depth", depth, "->", result, "(expect", expected, ")", status)
	}

	println("  tiny frames (~32 bytes):")
	for _, depth := range []int{100, 200, 300, 400} {
		expected := depth * (depth + 1) / 2
		ch := make(chan int, 1)
		go func(d int) {
			ch <- tinyFrame(d, 0)
		}(depth)
		result := <-ch
		status := "PASS"
		if result != expected {
			status = "FAIL"
		}
		println("    depth", depth, "->", result, "(expect", expected, ")", status)
	}
}

func benchContextSwitch() {
	println("context switch:")
	const iterations = 500
	ping := make(chan bool)
	pong := make(chan bool)
	done := make(chan bool)

	go func() {
		for i := 0; i < iterations; i++ {
			<-ping
			pong <- true
		}
		done <- true
	}()

	start := nanotime()
	for i := 0; i < iterations; i++ {
		ping <- true
		<-pong
	}
	<-done
	elapsed := nanotime() - start

	switchCount := iterations * 2
	nsPerSwitch := elapsed / int64(switchCount)
	println("  iterations:", iterations)
	println("  switches:", switchCount)
	println("  total:", elapsed/1000000, "ms")
	println("  per switch:", nsPerSwitch, "ns")
}

func benchGoroutineSpawn() {
	println("goroutine spawn:")
	const iterations = 50
	ch := make(chan int, 1)

	start := nanotime()
	for i := 0; i < iterations; i++ {
		go func(n int) {
			ch <- n * 2
		}(i)
		<-ch
	}
	elapsed := nanotime() - start

	nsPerSpawn := elapsed / int64(iterations)
	println("  iterations:", iterations)
	println("  total:", elapsed/1000000, "ms")
	println("  per spawn:", nsPerSpawn, "ns")

	start = nanotime()
	sum := 0
	for i := 0; i < iterations; i++ {
		sum += i * 2
	}
	directElapsed := nanotime() - start
	_ = sum

	nsPerCall := directElapsed / int64(iterations)
	if nsPerCall == 0 {
		nsPerCall = 1
	}
	overhead := nsPerSpawn / nsPerCall
	println("  direct call:", nsPerCall, "ns")
	println("  overhead:", overhead, "x")
}

func benchChannelOps() {
	println("channel ops:")
	const iterations = 1000

	bufCh := make(chan int, 100)
	start := nanotime()
	for i := 0; i < iterations; i++ {
		bufCh <- i
		<-bufCh
	}
	bufElapsed := nanotime() - start
	bufNsPerOp := bufElapsed / int64(iterations*2)
	println("  buffered:", bufNsPerOp, "ns/op")

	unbufCh := make(chan int)
	done := make(chan bool)

	go func() {
		for i := 0; i < iterations; i++ {
			v := <-unbufCh
			unbufCh <- v + 1
		}
		done <- true
	}()

	start = nanotime()
	for i := 0; i < iterations; i++ {
		unbufCh <- i
		<-unbufCh
	}
	<-done
	unbufElapsed := nanotime() - start
	unbufNsPerTrip := unbufElapsed / int64(iterations)
	println("  unbuffered roundtrip:", unbufNsPerTrip, "ns")
}

func benchGosched() {
	println("gosched:")
	const iterations = 2000

	start := nanotime()
	for i := 0; i < iterations; i++ {
		gosched()
	}
	elapsed := nanotime() - start

	nsPerYield := elapsed / int64(iterations)
	println("  iterations:", iterations)
	println("  total:", elapsed/1000, "us")
	println("  per yield:", nsPerYield, "ns")
}

var gcRetainedData []byte

func benchGCPause() {
	println("gc pause:")
	sizes := []int{32, 64, 128, 256, 512, 1024}

	for _, sizeKB := range sizes {
		gcRetainedData = make([]byte, sizeKB*1024)
		for i := range gcRetainedData {
			gcRetainedData[i] = byte(i)
		}
		for i := 0; i < 200; i++ {
			_ = make([]byte, 1024)
		}
		start := nanotime()
		forceGC()
		elapsed := nanotime() - start
		pauseUs := elapsed / 1000
		println(" ", sizeKB, "KB:", pauseUs, "us")
		gcRetainedData = nil
	}
}

func benchMemoryLayout() {
	println("memory layout:")
	stackKB := goroutineStackSize() / 1024
	largeKB := largeObjectThreshold() / 1024
	print("  stack: ", stackKB, "KB, context: 64B, header: 8B, large obj: ", largeKB, "KB\n")
	large := make([]byte, 100*1024)
	large[0] = 1
	large[len(large)-1] = 2
	if large[0] == 1 && large[len(large)-1] == 2 {
		println("  100KB alloc: PASS")
	} else {
		println("  100KB alloc: FAIL")
	}
}

func main() {
	println("bench_architecture")
	println("Dreamcast SH-4 @ 200MHz, M:1 cooperative")
	println("")

	forceGC()
	_ = heavyFrame(10, 0)

	benchStackDepth()
	benchContextSwitch()
	benchGoroutineSpawn()
	benchChannelOps()
	benchGosched()
	benchGCPause()
	benchMemoryLayout()

	println("")
	println("done")
}
