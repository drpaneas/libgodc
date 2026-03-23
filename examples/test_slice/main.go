//go:build gccgo

// !!! DISCLAIMER: THIS EXAMPLE IS WRITTEN WITH AI.
// free_float32_slice - Demonstrates FreeFloat32Slice on Dreamcast.
//
// Allocates a large []float32 (bypasses GC via external allocator),
// frees it safely with FreeFloat32Slice (which calls freeSlice) then
// prints the result to the screen via BIOS font.
package main

import (
	"kos"
	"unsafe"
)

// freeSlice calls runtime.FreeSlice in the C runtime (gc_heap.c).
// It frees large slice that was allocated outside the GC (large objects).
//
//go:linkname freeSlice runtime.FreeSlice
func freeSlice(ptr unsafe.Pointer)

// FreeFloat32Slice wraps the freeSlice
// function to free a large []float32 slice.
func FreeFloat32Slice(s *[]float32) {
	freeSlice(unsafe.Pointer(s))
}

// row returns a VRAM pointer for the given text row (0-indexed).
func row(n int) unsafe.Pointer {
	return kos.VramSOffset(n * kos.BFONT_HEIGHT * 640)
}

func main() {
	const width = 640
	const LEN = 32 * 1024 // 32KB
	// Allocate a large []float32 — on Dreamcast this exceeds the GC
	// threshold and goes through KOS malloc (external allocator).
	vertices := make([]float32, LEN) // 4 * LEN KB (Currently 128 KB)

	// Write sentinel values so we can prove the slice was live.
	for i := range vertices {
		vertices[i] = float32(i) * 1.5
	}

	beforeLen := len(vertices)
	beforeNil := vertices == nil

	// Free and nil in one safe call.
	FreeFloat32Slice(&vertices)

	afterLen := len(vertices)
	afterNil := vertices == nil

	// --- Print results to screen via BIOS font ---

	kos.BfontDrawStr(row(1), width, true, "FreeFloat32Slice demo")

	if !beforeNil && beforeLen == LEN {
		kos.BfontDrawStr(row(3), width, true, "PASS: slice allocated (len=1024)")
	} else {
		kos.BfontDrawStr(row(3), width, true, "FAIL: allocation")
	}

	if afterNil && afterLen == 0 {
		kos.BfontDrawStr(row(5), width, true, "PASS: slice is nil after free")
	} else {
		kos.BfontDrawStr(row(5), width, true, "FAIL: slice not nil after free")
	}

	for {
		kos.TimerSpinSleep(100)
	}
}
