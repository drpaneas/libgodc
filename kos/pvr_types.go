//go:build gccgo

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

func (v *PvrVertex) SetColor(a, r, g, b uint8) {
	v.ARGB = uint32(a)<<24 | uint32(r)<<16 | uint32(g)<<8 | uint32(b)
}

func (v *PvrVertex) SetColorF(a, r, g, b float32) {
	v.SetColor(uint8(a*255), uint8(r*255), uint8(g*255), uint8(b*255))
}

type PvrPolyCxtGen struct {
	Alpha        int32
	Shading      int32
	FogType      int32
	Culling      int32
	ColorClamp   int32
	ClipMode     int32
	ModifierMode int32
	Specular     int32
	Alpha2       int32
	FogType2     int32
	ColorClamp2  int32
}

type PvrPolyCxtBlend struct {
	Src        int32
	Dst        int32
	SrcEnable  int32
	DstEnable  int32
	Src2       int32
	Dst2       int32
	SrcEnable2 int32
	DstEnable2 int32
}

type PvrPolyCxtFmt struct {
	Color    int32
	UV       int32
	Modifier int32
}

type PvrPolyCxtDepth struct {
	Comparison int32
	Write      int32
}

type PvrPolyCxtTxr struct {
	Enable     int32
	Filter     int32
	Mipmap     int32
	MipmapBias int32
	UVFlip     int32
	UVClamp    int32
	Alpha      int32
	Env        int32
	Width      int32
	Height     int32
	Format     int32
	Base       uintptr
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

//extern pvr_poly_cxt_col
func pvrPolyCxtCol(dst uintptr, list uint32)

func PvrPolyCxtCol(cxt *PvrPolyCxt, list uint32) {
	pvrPolyCxtCol(uintptr(unsafe.Pointer(cxt)), list)
}

//extern pvr_poly_cxt_txr
func pvrPolyCxtTxr(dst uintptr, list uint32, format int32, w int32, h int32, base uintptr, filter int32)

func PvrPolyCxtInitTxr(cxt *PvrPolyCxt, list uint32, format int32, w, h int32, base PvrPtr, filter int32) {
	pvrPolyCxtTxr(uintptr(unsafe.Pointer(cxt)), list, format, w, h, uintptr(base), filter)
}

//extern pvr_poly_compile
func pvrPolyCompile(dst uintptr, src uintptr)

func PvrPolyCompile(hdr *PvrPolyHdr, cxt *PvrPolyCxt) {
	pvrPolyCompile(uintptr(unsafe.Pointer(hdr)), uintptr(unsafe.Pointer(cxt)))
}

//extern __go_pvr_prim_hdr
func goPvrPrimHdr(data unsafe.Pointer) int32

//extern __go_pvr_prim_vertex
func goPvrPrimVertex(data unsafe.Pointer) int32

//extern __go_pvr_prim_vertex_fast
func goPvrPrimVertexFast(data unsafe.Pointer) int32

func PvrPrim(data *PvrPolyHdr) int32 {
	return goPvrPrimHdr(unsafe.Pointer(data))
}

func PvrPrimVertex(v *PvrVertex) int32 {
	return goPvrPrimVertex(unsafe.Pointer(v))
}

func PvrPrimVertexFast(v *PvrVertex) int32 {
	return goPvrPrimVertexFast(unsafe.Pointer(v))
}

//extern __go_aligned_pool_reset
func goAlignedPoolReset()

//extern __go_aligned_pool_get
func goAlignedPoolGet() unsafe.Pointer

//extern __go_aligned_pool_index
func goAlignedPoolIndex() int32

//extern __go_aligned_pool_base
func goAlignedPoolBase() unsafe.Pointer

func AlignedPoolReset() {
	goAlignedPoolReset()
}

func AlignedPoolGetVertex() *PvrVertex {
	ptr := goAlignedPoolGet()
	if ptr == nil {
		return nil
	}
	return (*PvrVertex)(ptr)
}

func MakeARGB(a, r, g, b uint8) uint32 {
	return uint32(a)<<24 | uint32(r)<<16 | uint32(g)<<8 | uint32(b)
}

func MakeRGB(r, g, b uint8) uint32 {
	return 0xFF000000 | uint32(r)<<16 | uint32(g)<<8 | uint32(b)
}
