//go:build ignore

// bench_gc_pause.go - GC pause time benchmarks
package main

import _ "unsafe"

//go:linkname nanotime runtime.nanotime
func nanotime() int64

//go:linkname forceGC runtime.GC
func forceGC()

func benchBaseline() {
	println("baseline (minimal live data):")
	forceGC()
	forceGC()

	for i := 0; i < 5; i++ {
		start := nanotime()
		forceGC()
		elapsed := nanotime() - start
		pauseUs := elapsed / 1000
		println("  cycle", i+1, ":", pauseUs, "us")
	}
}

func benchWith32KB() {
	println("with 32KB live data:")
	live := make([]*[32]byte, 1024)
	for i := 0; i < 1024; i++ {
		live[i] = new([32]byte)
		live[i][0] = byte(i)
	}

	for i := 0; i < 3; i++ {
		start := nanotime()
		forceGC()
		elapsed := nanotime() - start
		pauseUs := elapsed / 1000
		println("  cycle", i+1, ":", pauseUs, "us")
	}
	_ = live[0]
}

func benchWith128KB() {
	println("with 128KB live data:")
	live := make([]*[128]byte, 1024)
	for i := 0; i < 1024; i++ {
		live[i] = new([128]byte)
		live[i][0] = byte(i)
	}

	for i := 0; i < 3; i++ {
		start := nanotime()
		forceGC()
		elapsed := nanotime() - start
		pauseUs := elapsed / 1000
		println("  cycle", i+1, ":", pauseUs, "us")
	}
	_ = live[0]
}

func benchWithGarbage() {
	println("after allocating garbage:")
	for i := 0; i < 100; i++ {
		_ = make([]byte, 4096)
	}

	for i := 0; i < 3; i++ {
		start := nanotime()
		forceGC()
		elapsed := nanotime() - start
		pauseUs := elapsed / 1000
		println("  cycle", i+1, ":", pauseUs, "us")
	}
}

func main() {
	println("bench_gc_pause")
	println("")

	benchBaseline()
	benchWith32KB()
	benchWith128KB()
	benchWithGarbage()

	println("")
	println("done")
}
