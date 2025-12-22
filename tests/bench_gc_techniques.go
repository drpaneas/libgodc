//go:build ignore

// bench_gc_techniques.go - GC optimization techniques benchmark
package main

import _ "unsafe"

//go:linkname nanotime runtime.nanotime
func nanotime() int64

//go:linkname forceGC runtime.GC
func forceGC()

type Bullet struct {
	X, Y   float32
	VX, VY float32
	Active bool
}

var bulletPool []*Bullet

func getBullet() *Bullet {
	if len(bulletPool) > 0 {
		b := bulletPool[len(bulletPool)-1]
		bulletPool = bulletPool[:len(bulletPool)-1]
		return b
	}
	return new(Bullet)
}

func returnBullet(b *Bullet) {
	b.X, b.Y, b.VX, b.VY = 0, 0, 0, 0
	b.Active = false
	bulletPool = append(bulletPool, b)
}

func benchPreallocSlice() {
	println("pre-allocate slices:")
	const iterations = 1000

	start := nanotime()
	for i := 0; i < iterations; i++ {
		var items []int
		for j := 0; j < 100; j++ {
			items = append(items, j)
		}
		_ = items
	}
	growingTime := nanotime() - start
	forceGC()

	start = nanotime()
	for i := 0; i < iterations; i++ {
		items := make([]int, 0, 100)
		for j := 0; j < 100; j++ {
			items = append(items, j)
		}
		_ = items
	}
	preallocTime := nanotime() - start
	forceGC()

	growingNs := growingTime / int64(iterations)
	preallocNs := preallocTime / int64(iterations)
	speedup := (growingTime * 100) / preallocTime

	println("  growing slice:  ", growingNs, "ns/iter")
	println("  pre-allocated:  ", preallocNs, "ns/iter")
	println("  speedup:        ", speedup, "% (", speedup-100, "% faster)")
}

func benchObjectPool() {
	println("object pools:")
	const iterations = 1000

	start := nanotime()
	for i := 0; i < iterations; i++ {
		b := new(Bullet)
		b.X = float32(i)
		b.Active = true
		_ = b
	}
	newAllocTime := nanotime() - start
	forceGC()

	bulletPool = nil
	for i := 0; i < 100; i++ {
		bulletPool = append(bulletPool, new(Bullet))
	}

	start = nanotime()
	for i := 0; i < iterations; i++ {
		b := getBullet()
		b.X = float32(i)
		b.Active = true
		returnBullet(b)
	}
	poolTime := nanotime() - start
	forceGC()

	newAllocNs := newAllocTime / int64(iterations)
	poolNs := poolTime / int64(iterations)
	poolSpeedup := (newAllocTime * 100) / poolTime

	println("  new() each:     ", newAllocNs, "ns/iter")
	println("  pool reuse:     ", poolNs, "ns/iter")
	println("  speedup:        ", poolSpeedup, "% (", poolSpeedup-100, "% faster)")
}

func benchSliceReuse() {
	println("slice reuse:")
	const iterations = 1000

	start := nanotime()
	for i := 0; i < iterations; i++ {
		items := make([]int, 50)
		for j := 0; j < 50; j++ {
			items[j] = j
		}
		_ = items
	}
	newSliceTime := nanotime() - start
	forceGC()

	items := make([]int, 50)
	start = nanotime()
	for i := 0; i < iterations; i++ {
		for j := 0; j < 50; j++ {
			items[j] = j
		}
	}
	_ = items
	reuseTime := nanotime() - start
	forceGC()

	newSliceNs := newSliceTime / int64(iterations)
	reuseNs := reuseTime / int64(iterations)
	reuseSpeedup := (newSliceTime * 100) / reuseTime

	println("  new slice each: ", newSliceNs, "ns/iter")
	println("  reuse slice:    ", reuseNs, "ns/iter")
	println("  speedup:        ", reuseSpeedup, "% (", reuseSpeedup-100, "% faster)")
}

func benchManualGC() {
	println("manual gc trigger:")
	for i := 0; i < 50; i++ {
		_ = make([]byte, 1024)
	}

	start := nanotime()
	forceGC()
	gcTime := nanotime() - start

	println("  gc time:        ", gcTime/1000, "us")
}

func main() {
	println("bench_gc_techniques")
	println("")

	forceGC()

	benchPreallocSlice()
	benchObjectPool()
	benchSliceReuse()
	benchManualGC()

	println("")
	println("done")
}
