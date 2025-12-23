//go:build ignore

// bench_detailed.go - Detailed performance benchmarks
package main

import _ "unsafe"

//go:linkname nanotime runtime.nanotime
func nanotime() int64

func benchAllocPatterns() {
	println("allocation patterns:")
	const iterations = 500

	start := nanotime()
	for i := 0; i < iterations; i++ {
		p := new(int)
		*p = i
		_ = *p
	}
	newTime := nanotime() - start

	start = nanotime()
	var reused int
	for i := 0; i < iterations; i++ {
		reused = i
		_ = reused
	}
	reuseTime := nanotime() - start

	start = nanotime()
	prealloc := make([]int, iterations)
	for i := 0; i < iterations; i++ {
		prealloc[i] = i
	}
	preallocTime := nanotime() - start

	start = nanotime()
	var growing []int
	for i := 0; i < iterations; i++ {
		growing = append(growing, i)
	}
	growTime := nanotime() - start

	println("  new() each:", newTime/1000, "us")
	println("  reuse var:", reuseTime/1000, "us")
	println("  prealloc:", preallocTime/1000, "us")
	println("  growing:", growTime/1000, "us")
}

type PoolableObject struct {
	ID   int
	Data [64]byte
}

var objectPool = make(chan *PoolableObject, 100)

func getObject() *PoolableObject {
	select {
	case obj := <-objectPool:
		return obj
	default:
		return new(PoolableObject)
	}
}

func putObject(obj *PoolableObject) {
	select {
	case objectPool <- obj:
	default:
	}
}

func benchObjectPool() {
	println("object pool:")
	const iterations = 500

	start := nanotime()
	for i := 0; i < iterations; i++ {
		obj := new(PoolableObject)
		obj.ID = i
		_ = obj.ID
	}
	newTime := nanotime() - start

	start = nanotime()
	for i := 0; i < iterations; i++ {
		obj := getObject()
		obj.ID = i
		_ = obj.ID
		putObject(obj)
	}
	poolTime := nanotime() - start

	println("  always new:", newTime/1000, "us")
	println("  pooled:", poolTime/1000, "us")
	speedup := float64(newTime) / float64(poolTime)
	println("  speedup:", int(speedup*10)/10, "x")
}

func benchChannelLatency() {
	println("channel latency:")
	const samples = 100
	latencies := make([]int64, samples)

	ch := make(chan int)
	done := make(chan bool)

	go func() {
		for i := 0; i < samples; i++ {
			<-ch
			ch <- i
		}
		done <- true
	}()

	for i := 0; i < samples; i++ {
		start := nanotime()
		ch <- i
		<-ch
		latencies[i] = nanotime() - start
	}
	<-done

	var sum, min, max int64
	min = latencies[0]
	for _, lat := range latencies {
		sum += lat
		if lat < min {
			min = lat
		}
		if lat > max {
			max = lat
		}
	}
	avg := sum / int64(samples)

	println("  min:", min, "ns")
	println("  max:", max, "ns")
	println("  avg:", avg, "ns")

	buckets := [5]int{}
	for _, lat := range latencies {
		us := lat / 1000
		switch {
		case us < 10:
			buckets[0]++
		case us < 50:
			buckets[1]++
		case us < 100:
			buckets[2]++
		case us < 500:
			buckets[3]++
		default:
			buckets[4]++
		}
	}
	println("  <10us:", buckets[0], "10-50us:", buckets[1], "50-100us:", buckets[2], "100-500us:", buckets[3], ">500us:", buckets[4])
}

func benchGCImpact() {
	println("gc impact:")
	const samples = 50

	ch := make(chan int)
	done := make(chan bool)
	latencies := make([]int64, samples)

	go func() {
		for i := 0; i < samples; i++ {
			<-ch
			ch <- i
		}
		done <- true
	}()

	for i := 0; i < samples; i++ {
		for j := 0; j < 10; j++ {
			_ = make([]byte, 1024)
		}
		start := nanotime()
		ch <- i
		<-ch
		latencies[i] = nanotime() - start
	}
	<-done

	var sum, max int64
	spikes := 0
	for _, lat := range latencies {
		sum += lat
		if lat > max {
			max = lat
		}
		if lat > 100000 {
			spikes++
		}
	}
	avg := sum / int64(samples)

	println("  avg:", avg/1000, "us")
	println("  max:", max/1000, "us")
	println("  spikes (>100us):", spikes)
}

func benchWriteBarrier() {
	println("write barrier:")
	const iterations = 1000

	start := nanotime()
	data := make([]int, iterations)
	for i := 0; i < iterations; i++ {
		data[i] = i
	}
	intTime := nanotime() - start

	start = nanotime()
	ptrs := make([]*int, iterations)
	for i := 0; i < iterations; i++ {
		v := i
		ptrs[i] = &v
	}
	ptrTime := nanotime() - start

	println("  int writes:", intTime, "ns")
	println("  ptr writes:", ptrTime, "ns")
	overhead := float64(ptrTime-intTime) / float64(iterations)
	println("  barrier overhead:", int(overhead), "ns/write")
}

func benchStructVsPointer() {
	println("struct vs pointer:")

	type SmallStruct struct {
		A, B int
	}
	type LargeStruct struct {
		Data [256]byte
	}

	const iterations = 500

	sumSmallVal := func(s SmallStruct) int { return s.A + s.B }
	start := nanotime()
	for i := 0; i < iterations; i++ {
		s := SmallStruct{A: i, B: i * 2}
		_ = sumSmallVal(s)
	}
	smallValTime := nanotime() - start

	sumSmallPtr := func(s *SmallStruct) int { return s.A + s.B }
	start = nanotime()
	for i := 0; i < iterations; i++ {
		s := &SmallStruct{A: i, B: i * 2}
		_ = sumSmallPtr(s)
	}
	smallPtrTime := nanotime() - start

	processLargeVal := func(s LargeStruct) byte { return s.Data[0] }
	start = nanotime()
	for i := 0; i < iterations; i++ {
		s := LargeStruct{}
		s.Data[0] = byte(i)
		_ = processLargeVal(s)
	}
	largeValTime := nanotime() - start

	processLargePtr := func(s *LargeStruct) byte { return s.Data[0] }
	start = nanotime()
	for i := 0; i < iterations; i++ {
		s := &LargeStruct{}
		s.Data[0] = byte(i)
		_ = processLargePtr(s)
	}
	largePtrTime := nanotime() - start

	println("  small (16B) val:", smallValTime/1000, "us")
	println("  small (16B) ptr:", smallPtrTime/1000, "us")
	println("  large (256B) val:", largeValTime/1000, "us")
	println("  large (256B) ptr:", largePtrTime/1000, "us")
}

func main() {
	println("bench_detailed")
	println("Dreamcast SH-4 @ 200MHz")
	println("")

	benchAllocPatterns()
	benchObjectPool()
	benchChannelLatency()
	benchGCImpact()
	benchWriteBarrier()
	benchStructVsPointer()

	println("")
	println("done")
}
