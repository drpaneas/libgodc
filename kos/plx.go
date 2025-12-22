//go:build dreamcast
// +build dreamcast

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
	PLX_VERT     = PVR_CMD_VERTEX
	PLX_VERT_EOS = PVR_CMD_VERTEX_EOL
)

type PlxMatrix [4][4]float32

type PlxVector struct {
	X, Y, Z, W float32
}

type PlxPoint = PlxVector

//extern mat_store
func plxMatStore(dst unsafe.Pointer)

//extern mat_load
func plxMatLoad(src unsafe.Pointer)

//extern mat_identity
func PlxMatIdentity()

//extern mat_apply
func plxMatApply(src unsafe.Pointer)

//extern __go_mat_trans_single
func plxMatTransSingle(x, y, z *float32)

func PlxMatStore(dst *PlxMatrix) {
	plxMatStore(unsafe.Pointer(dst))
}

func PlxMatLoad(src *PlxMatrix) {
	plxMatLoad(unsafe.Pointer(src))
}

func PlxMatApply(src *PlxMatrix) {
	plxMatApply(unsafe.Pointer(src))
}

func PlxMatTfip3D(x, y, z *float32) {
	plxMatTransSingle(x, y, z)
}

//extern plx_mat3d_init
func PlxMat3DInit()

//extern plx_mat3d_mode
func PlxMat3DMode(mode int32)

//extern plx_mat3d_identity
func PlxMat3DIdentity()

//extern plx_mat3d_load
func plxMat3dLoad(src unsafe.Pointer)

//extern plx_mat3d_store
func plxMat3dStore(dst unsafe.Pointer)

//extern plx_mat3d_perspective
func PlxMat3DPerspective(angle, aspect, znear, zfar float32)

//extern plx_mat3d_frustum
func PlxMat3DFrustum(left, right, bottom, top, znear, zfar float32)

//extern plx_mat3d_push
func PlxMat3DPush()

//extern plx_mat3d_pop
func PlxMat3DPop()

//extern plx_mat3d_peek
func PlxMat3DPeek()

//extern plx_mat3d_rotate
func PlxMat3DRotate(angle, x, y, z float32)

//extern plx_mat3d_scale
func PlxMat3DScale(x, y, z float32)

//extern plx_mat3d_translate
func PlxMat3DTranslate(x, y, z float32)

//extern plx_mat3d_apply
func PlxMat3DApply(mode int32)

//extern plx_mat3d_apply_all
func PlxMat3DApplyAll()

func PlxMat3DLoad(src *PlxMatrix) {
	plxMat3dLoad(unsafe.Pointer(src))
}

func PlxMat3DStore(dst *PlxMatrix) {
	plxMat3dStore(unsafe.Pointer(dst))
}

//extern plx_cxt_init
func PlxCxtInit()

//extern plx_cxt_texture
func plxCxtTexture(txr unsafe.Pointer)

//extern plx_cxt_blending
func PlxCxtBlending(src, dst int32)

//extern plx_cxt_culling
func PlxCxtCulling(ctype int32)

//extern plx_cxt_fog
func PlxCxtFog(ftype int32)

//extern plx_cxt_specular
func PlxCxtSpecular(stype int32)

//extern plx_cxt_send
func PlxCxtSend(list int32)

func PlxCxtTexture(txr unsafe.Pointer) {
	plxCxtTexture(txr)
}

//extern __go_plx_dr_init
func plxDrInit(state unsafe.Pointer)

//extern __go_plx_dr_finish
func PlxDrFinish()

func PlxDrInit(state *PvrDrState) {
	plxDrInit(unsafe.Pointer(state))
}

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

func PlxVertFnp(flags uint32, x, y, z, a, r, g, b float32) {
	var vert PvrVertex
	vert.Flags = flags
	vert.X = x
	vert.Y = y
	vert.Z = z
	vert.U = 0
	vert.V = 0
	vert.ARGB = PlxPackColor(a, r, g, b)
	vert.OARGB = 0
	PvrPrimVertex(&vert)
}

func PlxVertInp(flags uint32, x, y, z float32, color uint32) {
	var vert PvrVertex
	vert.Flags = flags
	vert.X = x
	vert.Y = y
	vert.Z = z
	vert.U = 0
	vert.V = 0
	vert.ARGB = color
	vert.OARGB = 0
	PvrPrimVertex(&vert)
}

func PlxVertInpm3(flags uint32, x, y, z float32, color uint32) {
	PlxMatTfip3D(&x, &y, &z)
	PlxVertInp(flags, x, y, z, color)
}

func PlxVertIndm3(state *PvrDrState, flags uint32, x, y, z float32, color uint32) {
	PlxMatTfip3D(&x, &y, &z)
	PlxVertInp(flags, x, y, z, color)
}

func PlxVertFnd(state *PvrDrState, flags uint32, x, y, z, a, r, g, b float32) {
	PlxVertFnp(flags, x, y, z, a, r, g, b)
}

func PlxVertIfp(flags uint32, x, y, z float32, color uint32, u, v float32) {
	var vert PvrVertex
	vert.Flags = flags
	vert.X = x
	vert.Y = y
	vert.Z = z
	vert.U = u
	vert.V = v
	vert.ARGB = color
	vert.OARGB = 0
	PvrPrimVertex(&vert)
}

func PlxVertFfp(flags uint32, x, y, z, a, r, g, b, u, v float32) {
	var vert PvrVertex
	vert.Flags = flags
	vert.X = x
	vert.Y = y
	vert.Z = z
	vert.U = u
	vert.V = v
	vert.ARGB = PlxPackColor(a, r, g, b)
	vert.OARGB = 0
	PvrPrimVertex(&vert)
}

func PlxSprInp(wi, hi, x, y, z float32, color uint32) {
	w := wi / 2.0
	h := hi / 2.0
	PlxVertIfp(PLX_VERT, x-w, y+h, z, color, 0.0, 1.0)
	PlxVertIfp(PLX_VERT, x-w, y-h, z, color, 0.0, 0.0)
	PlxVertIfp(PLX_VERT, x+w, y+h, z, color, 1.0, 1.0)
	PlxVertIfp(PLX_VERT_EOS, x+w, y-h, z, color, 1.0, 0.0)
}

type PlxTexture struct {
	ptr unsafe.Pointer
}

const (
	PLX_FILTER_NONE     = 0
	PLX_FILTER_BILINEAR = 2
)

const (
	PLX_UV_REPEAT = 0
	PLX_UV_CLAMP  = 1
)

//extern plx_txr_load
func plxTxrLoad(fn unsafe.Pointer, useAlpha, flags int32) unsafe.Pointer

//extern plx_txr_canvas
func plxTxrCanvas(w, h, fmt int32) unsafe.Pointer

//extern plx_txr_destroy
func plxTxrDestroy(txr unsafe.Pointer)

//extern plx_txr_send_hdr
func plxTxrSendHdr(txr unsafe.Pointer, list, flush int32)

//extern plx_txr_setfilter
func plxTxrSetFilter(txr unsafe.Pointer, mode int32)

//extern plx_txr_setuvclamp
func plxTxrSetUVClamp(txr unsafe.Pointer, umode, vmode int32)

//extern plx_txr_flush_hdrs
func plxTxrFlushHdrs(txr unsafe.Pointer)

func PlxTxrLoad(filename string, useAlpha bool, flags int32) *PlxTexture {
	b := make([]byte, len(filename)+1)
	copy(b, filename)
	alpha := int32(0)
	if useAlpha {
		alpha = 1
	}
	ptr := plxTxrLoad(unsafe.Pointer(&b[0]), alpha, flags)
	if ptr == nil {
		return nil
	}
	return &PlxTexture{ptr: ptr}
}

func PlxTxrCanvas(w, h, fmt int32) *PlxTexture {
	ptr := plxTxrCanvas(w, h, fmt)
	if ptr == nil {
		return nil
	}
	return &PlxTexture{ptr: ptr}
}

func (t *PlxTexture) Destroy() {
	if t.ptr != nil {
		plxTxrDestroy(t.ptr)
		t.ptr = nil
	}
}

func (t *PlxTexture) SendHdr(list int32, flush bool) {
	f := int32(0)
	if flush {
		f = 1
	}
	plxTxrSendHdr(t.ptr, list, f)
}

func (t *PlxTexture) SetFilter(mode int32) {
	plxTxrSetFilter(t.ptr, mode)
}

func (t *PlxTexture) SetUVClamp(umode, vmode int32) {
	plxTxrSetUVClamp(t.ptr, umode, vmode)
}

func (t *PlxTexture) FlushHdrs() {
	plxTxrFlushHdrs(t.ptr)
}

func (t *PlxTexture) Ptr() unsafe.Pointer {
	return t.ptr
}

type PlxFont struct {
	ptr unsafe.Pointer
}

type PlxFcxt struct {
	ptr unsafe.Pointer
}

//extern plx_font_load
func plxFontLoad(fn unsafe.Pointer) unsafe.Pointer

//extern plx_font_destroy
func plxFontDestroy(fnt unsafe.Pointer)

//extern plx_fcxt_create
func plxFcxtCreate(fnt unsafe.Pointer, list int32) unsafe.Pointer

//extern plx_fcxt_destroy
func plxFcxtDestroy(cxt unsafe.Pointer)

//extern plx_fcxt_setpos
func plxFcxtSetPos(cxt unsafe.Pointer, x, y, z float32)

//extern plx_fcxt_setpos_pnt
func plxFcxtSetPosPnt(cxt unsafe.Pointer, pos unsafe.Pointer)

//extern plx_fcxt_setsize
func plxFcxtSetSize(cxt unsafe.Pointer, size float32)

//extern plx_fcxt_setcolor4f
func plxFcxtSetColor4f(cxt unsafe.Pointer, a, r, g, b float32)

//extern plx_fcxt_begin
func plxFcxtBegin(cxt unsafe.Pointer)

//extern plx_fcxt_draw
func plxFcxtDraw(cxt unsafe.Pointer, str unsafe.Pointer)

//extern plx_fcxt_end
func plxFcxtEnd(cxt unsafe.Pointer)

func PlxFontLoad(filename string) *PlxFont {
	b := make([]byte, len(filename)+1)
	copy(b, filename)
	ptr := plxFontLoad(unsafe.Pointer(&b[0]))
	if ptr == nil {
		return nil
	}
	return &PlxFont{ptr: ptr}
}

func (f *PlxFont) Destroy() {
	if f.ptr != nil {
		plxFontDestroy(f.ptr)
		f.ptr = nil
	}
}

func PlxFcxtCreate(font *PlxFont, list int32) *PlxFcxt {
	ptr := plxFcxtCreate(font.ptr, list)
	if ptr == nil {
		return nil
	}
	return &PlxFcxt{ptr: ptr}
}

func (c *PlxFcxt) Destroy() {
	if c.ptr != nil {
		plxFcxtDestroy(c.ptr)
		c.ptr = nil
	}
}

func (c *PlxFcxt) SetPos(x, y, z float32) {
	plxFcxtSetPos(c.ptr, x, y, z)
}

func (c *PlxFcxt) SetSize(size float32) {
	plxFcxtSetSize(c.ptr, size)
}

func (c *PlxFcxt) SetColor4f(a, r, g, b float32) {
	plxFcxtSetColor4f(c.ptr, a, r, g, b)
}

func (c *PlxFcxt) SetColor(color uint32) {
	a := float32((color>>24)&0xff) / 255.0
	r := float32((color>>16)&0xff) / 255.0
	g := float32((color>>8)&0xff) / 255.0
	b := float32(color&0xff) / 255.0
	plxFcxtSetColor4f(c.ptr, a, r, g, b)
}

func (c *PlxFcxt) Begin() {
	plxFcxtBegin(c.ptr)
}

func (c *PlxFcxt) Draw(str string) {
	b := make([]byte, len(str)+1)
	copy(b, str)
	plxFcxtDraw(c.ptr, unsafe.Pointer(&b[0]))
}

func (c *PlxFcxt) End() {
	plxFcxtEnd(c.ptr)
}
