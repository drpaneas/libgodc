//go:build !gccgo

package kos

const F_PI float32 = 3.14159265358979323846

func Fsin(r float32) float32   { panic("kos: not on Dreamcast") }
func Fcos(r float32) float32   { panic("kos: not on Dreamcast") }
func Ftan(r float32) float32   { panic("kos: not on Dreamcast") }
func Fisin(d int32) float32    { panic("kos: not on Dreamcast") }
func Ficos(d int32) float32    { panic("kos: not on Dreamcast") }
func Fitan(d int32) float32    { panic("kos: not on Dreamcast") }
func Fsqrt(f float32) float32  { panic("kos: not on Dreamcast") }
func Frsqrt(f float32) float32 { panic("kos: not on Dreamcast") }

func Fipr(x, y, z, w, a, b, c, d float32) float32 { panic("kos: not on Dreamcast") }
func FiprMagnitudeSqr(x, y, z, w float32) float32 { panic("kos: not on Dreamcast") }

func fsincos(f float32, s *float32, c *float32)  { panic("kos: not on Dreamcast") }
func Fsincos(f float32) (sin, cos float32)       { panic("kos: not on Dreamcast") }
func fsincosr(f float32, s *float32, c *float32) { panic("kos: not on Dreamcast") }
func Fsincosr(f float32) (sin, cos float32)      { panic("kos: not on Dreamcast") }
