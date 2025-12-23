// math.go - Math utility functions
package main

// Random number generator state
var randState uint32 = 12345

// RandInt returns a random integer in [0, max)
func RandInt(max int) int {
	randState = randState*1103515245 + 12345
	return int((randState>>16)&0x7FFF) % max
}

// Abs32 returns absolute value of a float32
func Abs32(x float32) float32 {
	if x < 0 {
		return -x
	}
	return x
}

// Clamp32 clamps a value between lo and hi
func Clamp32(v, lo, hi float32) float32 {
	if v < lo {
		return lo
	}
	if v > hi {
		return hi
	}
	return v
}

// Min32 returns the minimum of two float32 values
func Min32(a, b float32) float32 {
	if a < b {
		return a
	}
	return b
}

// Sqrt32 computes square root using Newton's method
func Sqrt32(x float32) float32 {
	if x <= 0 {
		return 0
	}
	guess := x / 2
	for i := 0; i < 8; i++ {
		guess = (guess + x/guess) / 2
	}
	return guess
}

// Normalize returns a unit vector in the same direction
func Normalize(x, y float32) (float32, float32) {
	length := Sqrt32(x*x + y*y)
	if length < 0.0001 {
		return 1, 0
	}
	return x / length, y / length
}

