//go:build ignore

// bench_goroutine_usecase.go - Goroutine use case benchmarks
package main

import _ "unsafe"

//go:linkname nanotime runtime.nanotime
func nanotime() int64

//go:linkname forceGC runtime.GC
func forceGC()

//extern runtime_gosched
func gosched()

var workResult int

//go:noinline
func doWork(amount int) int {
	result := 0
	for i := 0; i < amount; i++ {
		result += i
	}
	return result
}

func runWithoutGoroutines(items int) (int64, int) {
	result := 0
	start := nanotime()
	for i := 0; i < items; i++ {
		work := i * 2
		result += doWork(work % 50)
	}
	return nanotime() - start, result
}

func runWithGoroutines(items int) (int64, int) {
	result := 0
	start := nanotime()
	workChan := make(chan int, 10)
	done := make(chan int)

	go func() {
		sum := 0
		for work := range workChan {
			sum += doWork(work % 50)
		}
		done <- sum
	}()

	for i := 0; i < items; i++ {
		workChan <- i * 2
	}
	close(workChan)
	result = <-done

	return nanotime() - start, result
}

func runTasksSequential(iterations int) (int64, int) {
	result := 0
	start := nanotime()
	for i := 0; i < iterations; i++ {
		result += doWork(10)
		result += doWork(20)
		result += doWork(15)
	}
	return nanotime() - start, result
}

func runTasksWithGoroutines(iterations int) (int64, int) {
	start := nanotime()
	results := make(chan int, 3)

	go func() {
		sum := 0
		for i := 0; i < iterations; i++ {
			sum += doWork(10)
			gosched()
		}
		results <- sum
	}()

	go func() {
		sum := 0
		for i := 0; i < iterations; i++ {
			sum += doWork(20)
			gosched()
		}
		results <- sum
	}()

	go func() {
		sum := 0
		for i := 0; i < iterations; i++ {
			sum += doWork(15)
			gosched()
		}
		results <- sum
	}()

	total := <-results + <-results + <-results
	return nanotime() - start, total
}

func runPingPongDirect(iterations int) int64 {
	start := nanotime()
	for i := 0; i < iterations; i++ {
		workResult += i
	}
	return nanotime() - start
}

func runPingPongChannel(iterations int) int64 {
	ping := make(chan int)
	pong := make(chan int)
	done := make(chan bool)

	go func() {
		for v := range ping {
			pong <- v + 1
		}
		done <- true
	}()

	start := nanotime()
	for i := 0; i < iterations; i++ {
		ping <- i
		workResult += <-pong
	}
	close(ping)
	<-done

	return nanotime() - start
}

func benchProducerConsumer() {
	println("producer-consumer:")
	const items = 200

	seqTime, seqResult := runWithoutGoroutines(items)
	forceGC()
	goTime, goResult := runWithGoroutines(items)
	forceGC()

	println("  sequential:  ", seqTime/1000, "us (result:", seqResult, ")")
	println("  goroutines:  ", goTime/1000, "us (result:", goResult, ")")
	if seqTime > 0 {
		overhead := ((goTime - seqTime) * 100) / seqTime
		println("  overhead:    ", overhead, "%")
	}
}

func benchMultipleTasks() {
	println("multiple tasks:")
	const taskIters = 100

	seqTaskTime, seqTaskResult := runTasksSequential(taskIters)
	forceGC()
	goTaskTime, goTaskResult := runTasksWithGoroutines(taskIters)
	forceGC()

	println("  sequential:  ", seqTaskTime/1000, "us (result:", seqTaskResult, ")")
	println("  goroutines:  ", goTaskTime/1000, "us (result:", goTaskResult, ")")
	if seqTaskTime > 0 {
		overhead := ((goTaskTime - seqTaskTime) * 100) / seqTaskTime
		println("  overhead:    ", overhead, "%")
	}
}

func benchChannelOverhead() {
	println("channel overhead:")
	const pingIters = 100

	directTime := runPingPongDirect(pingIters)
	forceGC()
	chanTime := runPingPongChannel(pingIters)
	forceGC()

	println("  direct loop: ", directTime/1000, "us")
	println("  via channel: ", chanTime/1000, "us")
	println("  per round:   ", chanTime/int64(pingIters)/1000, "us")
}

func main() {
	println("bench_goroutine_usecase")
	println("")

	forceGC()

	benchProducerConsumer()
	println("")
	benchMultipleTasks()
	println("")
	benchChannelOverhead()

	println("")
	println("done")
}
