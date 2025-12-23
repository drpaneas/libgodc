//go:build !dreamcast
// +build !dreamcast

package kos

import "unsafe"

const (
	PLX_MAT_PROJECTION = 0
	PLX_MAT_MODELVIEW  = 1
	PLX_MAT_SCREENVIEW = 2
	PLX_MAT_SCRATCH    = 3
	PLX_MAT_WORLDVIEW  = 4
)

const (
	PLX_CULL_NONE = 0
	PLX_CULL_CW   = 1
	PLX_CULL_CCW  = 2
)

const (
	PLX_VERT     = 0xe0000000
	PLX_VERT_EOS = 0xf0000000
)

type PlxMatrix [4][4]float32

type PlxVector struct {
	X, Y, Z, W float32
}

type PlxPoint = PlxVector

func PlxMatStore(dst *PlxMatrix)    {}
func PlxMatLoad(src *PlxMatrix)     {}
func PlxMatIdentity()               {}
func PlxMatApply(src *PlxMatrix)    {}
func PlxMatTfip3D(x, y, z *float32) {}

func PlxMat3DInit()                                                 {}
func PlxMat3DMode(mode int32)                                       {}
func PlxMat3DIdentity()                                             {}
func PlxMat3DPerspective(angle, aspect, znear, zfar float32)        {}
func PlxMat3DFrustum(left, right, bottom, top, znear, zfar float32) {}
func PlxMat3DPush()                                                 {}
func PlxMat3DPop()                                                  {}
func PlxMat3DPeek()                                                 {}
func PlxMat3DRotate(angle, x, y, z float32)                         {}
func PlxMat3DScale(x, y, z float32)                                 {}
func PlxMat3DTranslate(x, y, z float32)                             {}
func PlxMat3DApply(mode int32)                                      {}
func PlxMat3DApplyAll()                                             {}
func PlxMat3DLoad(src *PlxMatrix)                                   {}
func PlxMat3DStore(dst *PlxMatrix)                                  {}

func PlxCxtInit()                      {}
func PlxCxtTexture(txr unsafe.Pointer) {}
func PlxCxtBlending(src, dst int32)    {}
func PlxCxtCulling(ctype int32)        {}
func PlxCxtFog(ftype int32)            {}
func PlxCxtSpecular(stype int32)       {}
func PlxCxtSend(list int32)            {}

func PlxDrInit(state *PvrDrState) {}
func PlxDrFinish()                {}

func PlxPackColor(a, r, g, b float32) uint32 {
	if a < 0 {
		a = 0
	} else if a > 1 {
		a = 1
	}
	if r < 0 {
		r = 0
	} else if r > 1 {
		r = 1
	}
	if g < 0 {
		g = 0
	} else if g > 1 {
		g = 1
	}
	if b < 0 {
		b = 0
	} else if b > 1 {
		b = 1
	}
	ai := uint32(a * 255)
	ri := uint32(r * 255)
	gi := uint32(g * 255)
	bi := uint32(b * 255)
	return (ai << 24) | (ri << 16) | (gi << 8) | bi
}

func PlxVertFnp(flags uint32, x, y, z, a, r, g, b float32)                        {}
func PlxVertInp(flags uint32, x, y, z float32, color uint32)                      {}
func PlxVertInpm3(flags uint32, x, y, z float32, color uint32)                    {}
func PlxVertIndm3(state *PvrDrState, flags uint32, x, y, z float32, color uint32) {}
func PlxVertFnd(state *PvrDrState, flags uint32, x, y, z, a, r, g, b float32)     {}
func PlxVertIfp(flags uint32, x, y, z float32, color uint32, u, v float32)        {}
func PlxVertFfp(flags uint32, x, y, z, a, r, g, b, u, v float32)                  {}

func PlxSprInp(wi, hi, x, y, z float32, color uint32) {}

const (
	PLX_FILTER_NONE     = 0
	PLX_FILTER_BILINEAR = 2
)

const (
	PLX_UV_REPEAT = 0
	PLX_UV_CLAMP  = 1
)

type PlxTexture struct {
	ptr unsafe.Pointer
}

func PlxTxrLoad(filename string, useAlpha bool, flags int32) *PlxTexture { return nil }
func PlxTxrCanvas(w, h, fmt int32) *PlxTexture                           { return nil }
func (t *PlxTexture) Destroy()                                           {}
func (t *PlxTexture) SendHdr(list int32, flush bool)                     {}
func (t *PlxTexture) SetFilter(mode int32)                               {}
func (t *PlxTexture) SetUVClamp(umode, vmode int32)                      {}
func (t *PlxTexture) FlushHdrs()                                         {}
func (t *PlxTexture) Ptr() unsafe.Pointer                                { return nil }

type PlxFont struct {
	ptr unsafe.Pointer
}

func PlxFontLoad(filename string) *PlxFont { return nil }
func (f *PlxFont) Destroy()                {}

type PlxFcxt struct {
	ptr unsafe.Pointer
}

func PlxFcxtCreate(font *PlxFont, list int32) *PlxFcxt { return nil }
func (c *PlxFcxt) Destroy()                            {}
func (c *PlxFcxt) SetPos(x, y, z float32)              {}
func (c *PlxFcxt) SetSize(size float32)                {}
func (c *PlxFcxt) SetColor4f(a, r, g, b float32)       {}
func (c *PlxFcxt) SetColor(color uint32)               {}
func (c *PlxFcxt) Begin()                              {}
func (c *PlxFcxt) Draw(str string)                     {}
func (c *PlxFcxt) End()                                {}
