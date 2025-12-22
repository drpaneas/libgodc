//go:build gccgo

package kos

const F_PI float32 = 3.14159265358979323846

//extern fsin
func Fsin(r float32) float32

//extern fcos
func Fcos(r float32) float32

//extern ftan
func Ftan(r float32) float32

//extern fisin
func Fisin(d int32) float32

//extern ficos
func Ficos(d int32) float32

//extern fitan
func Fitan(d int32) float32

//extern fsqrt
func Fsqrt(f float32) float32

//extern frsqrt
func Frsqrt(f float32) float32

//extern fipr
func Fipr(x, y, z, w, a, b, c, d float32) float32

//extern fipr_magnitude_sqr
func FiprMagnitudeSqr(x, y, z, w float32) float32

//extern fsincos
func fsincos(f float32, s *float32, c *float32)

func Fsincos(f float32) (sin, cos float32) {
	fsincos(f, &sin, &cos)
	return
}

//extern fsincosr
func fsincosr(f float32, s *float32, c *float32)

func Fsincosr(f float32) (sin, cos float32) {
	fsincosr(f, &sin, &cos)
	return
}
