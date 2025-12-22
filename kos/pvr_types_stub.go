//go:build !gccgo

package kos

import "unsafe"

type PvrPolyHdr struct {
	Cmd   uint32
	Mode1 uint32
	Mode2 uint32
	Mode3 uint32
	D1    uint32
	D2    uint32
	D3    uint32
	D4    uint32
}

type PvrVertex struct {
	Flags uint32
	X     float32
	Y     float32
	Z     float32
	U     float32
	V     float32
	ARGB  uint32
	OARGB uint32
}

func (v *PvrVertex) SetColor(a, r, g, b uint8)    { panic("kos: not on Dreamcast") }
func (v *PvrVertex) SetColorF(a, r, g, b float32) { panic("kos: not on Dreamcast") }

type PvrPolyCxtGen struct {
	Alpha, Shading, FogType, Culling, ColorClamp, ClipMode int32
	ModifierMode, Specular, Alpha2, FogType2, ColorClamp2  int32
}

type PvrPolyCxtBlend struct {
	Src, Dst, SrcEnable, DstEnable     int32
	Src2, Dst2, SrcEnable2, DstEnable2 int32
}

type PvrPolyCxtFmt struct {
	Color, UV, Modifier int32
}

type PvrPolyCxtDepth struct {
	Comparison, Write int32
}

type PvrPolyCxtTxr struct {
	Enable, Filter, Mipmap, MipmapBias int32
	UVFlip, UVClamp, Alpha, Env        int32
	Width, Height, Format              int32
	Base                               uintptr
}

type PvrPolyCxt struct {
	ListType int32
	Gen      PvrPolyCxtGen
	Blend    PvrPolyCxtBlend
	Fmt      PvrPolyCxtFmt
	Depth    PvrPolyCxtDepth
	Txr      PvrPolyCxtTxr
	Txr2     PvrPolyCxtTxr
}

func pvrPolyCxtCol(dst uintptr, list uint32)     { panic("kos: not on Dreamcast") }
func PvrPolyCxtCol(cxt *PvrPolyCxt, list uint32) { panic("kos: not on Dreamcast") }

func pvrPolyCxtTxr(dst uintptr, list uint32, format int32, w int32, h int32, base uintptr, filter int32) {
	panic("kos: not on Dreamcast")
}
func PvrPolyCxtInitTxr(cxt *PvrPolyCxt, list uint32, format int32, w, h int32, base PvrPtr, filter int32) {
	panic("kos: not on Dreamcast")
}

func pvrPolyCompile(dst uintptr, src uintptr)         { panic("kos: not on Dreamcast") }
func PvrPolyCompile(hdr *PvrPolyHdr, cxt *PvrPolyCxt) { panic("kos: not on Dreamcast") }

func goPvrPrimHdr(data unsafe.Pointer) int32        { panic("kos: not on Dreamcast") }
func goPvrPrimVertex(data unsafe.Pointer) int32     { panic("kos: not on Dreamcast") }
func goPvrPrimVertexFast(data unsafe.Pointer) int32 { panic("kos: not on Dreamcast") }

func PvrPrim(data *PvrPolyHdr) int32       { panic("kos: not on Dreamcast") }
func PvrPrimVertex(v *PvrVertex) int32     { panic("kos: not on Dreamcast") }
func PvrPrimVertexFast(v *PvrVertex) int32 { panic("kos: not on Dreamcast") }

func goAlignedPoolReset()               { panic("kos: not on Dreamcast") }
func goAlignedPoolGet() unsafe.Pointer  { panic("kos: not on Dreamcast") }
func goAlignedPoolIndex() int32         { panic("kos: not on Dreamcast") }
func goAlignedPoolBase() unsafe.Pointer { panic("kos: not on Dreamcast") }

func AlignedPoolReset()                { panic("kos: not on Dreamcast") }
func AlignedPoolGetVertex() *PvrVertex { panic("kos: not on Dreamcast") }

func MakeARGB(a, r, g, b uint8) uint32 {
	return uint32(a)<<24 | uint32(r)<<16 | uint32(g)<<8 | uint32(b)
}

func MakeRGB(r, g, b uint8) uint32 {
	return 0xFF000000 | uint32(r)<<16 | uint32(g)<<8 | uint32(b)
}
