//go:build dreamcast
// +build dreamcast

package kos

import "unsafe"

//extern __go_dr_init
func goDRInit() int32

//extern __go_dr_shutdown
func goDRShutdown()

//extern __go_dr_begin_frame
func goDRBeginFrame()

//extern __go_dr_end_frame
func goDREndFrame()

//extern __go_dr_get_vertex
func goDRGetVertex() unsafe.Pointer

//extern __go_dr_get_vertex_at
func goDRGetVertexAt(index int32) unsafe.Pointer

//extern __go_dr_get_vertex_count
func goDRGetVertexCount() int32

//extern __go_dr_set_vertex_count
func goDRSetVertexCount(count int32)

//extern __go_dr_get_header
func goDRGetHeader() unsafe.Pointer

//extern __go_dr_submit_header
func goDRSubmitHeader(hdr unsafe.Pointer)

//extern __go_dr_submit_vertex
func goDRSubmitVertex(vtx unsafe.Pointer)

//extern __go_dr_submit_vertices
func goDRSubmitVertices(start, end int32)

//extern __go_dr_submit_all_vertices
func goDRSubmitAllVertices()

//extern __go_dr_submit_vertex_xyzc
func goDRSubmitVertexXYZC(flags uint32, x, y, z float32, argb uint32)

//extern __go_dr_submit_vertex_full
func goDRSubmitVertexFull(flags uint32, x, y, z, u, v float32, argb, oargb uint32)

//extern __go_dr_submit_strip
func goDRSubmitStrip(hdr, vertices unsafe.Pointer, count int32)

//extern __go_dr_check_alignment
func goDRCheckAlignment() int32

//extern __go_dr_get_vertex_buffer
func goDRGetVertexBuffer() unsafe.Pointer

//extern __go_dr_get_header_buffer
func goDRGetHeaderBuffer() unsafe.Pointer

type DirectRenderer struct {
	initialized bool
}

var DR DirectRenderer

func (d *DirectRenderer) Init() error {
	if d.initialized {
		return nil
	}
	result := goDRInit()
	if result != 0 {
		return &DRError{Code: int(result), Message: "failed to allocate DR buffers"}
	}
	if goDRCheckAlignment() != 0 {
		return &DRError{Code: -3, Message: "DR buffers not properly aligned"}
	}
	d.initialized = true
	return nil
}

func (d *DirectRenderer) Shutdown() {
	if d.initialized {
		goDRShutdown()
		d.initialized = false
	}
}

func (d *DirectRenderer) BeginFrame() {
	goDRBeginFrame()
}

func (d *DirectRenderer) EndFrame() {
	goDREndFrame()
}

func (d *DirectRenderer) GetVertex() *PvrVertex {
	ptr := goDRGetVertex()
	if ptr == nil {
		return nil
	}
	return (*PvrVertex)(ptr)
}

func (d *DirectRenderer) GetVertexAt(index int) *PvrVertex {
	ptr := goDRGetVertexAt(int32(index))
	if ptr == nil {
		return nil
	}
	return (*PvrVertex)(ptr)
}

func (d *DirectRenderer) GetVertexCount() int {
	return int(goDRGetVertexCount())
}

func (d *DirectRenderer) SetVertexCount(count int) {
	goDRSetVertexCount(int32(count))
}

func (d *DirectRenderer) GetHeader() *PvrPolyHdr {
	ptr := goDRGetHeader()
	if ptr == nil {
		return nil
	}
	return (*PvrPolyHdr)(ptr)
}

func (d *DirectRenderer) SubmitHeader(hdr *PvrPolyHdr) {
	goDRSubmitHeader(unsafe.Pointer(hdr))
}

func (d *DirectRenderer) SubmitVertex(v *PvrVertex) {
	goDRSubmitVertex(unsafe.Pointer(v))
}

func (d *DirectRenderer) SubmitVertexXYZC(flags uint32, x, y, z float32, argb uint32) {
	goDRSubmitVertexXYZC(flags, x, y, z, argb)
}

func (d *DirectRenderer) SubmitVertexFull(flags uint32, x, y, z, u, v float32, argb, oargb uint32) {
	goDRSubmitVertexFull(flags, x, y, z, u, v, argb, oargb)
}

func (d *DirectRenderer) SubmitVertices(start, end int) {
	goDRSubmitVertices(int32(start), int32(end))
}

func (d *DirectRenderer) SubmitAllVertices() {
	goDRSubmitAllVertices()
}

func (d *DirectRenderer) SubmitStrip(hdr *PvrPolyHdr, vertices []PvrVertex) {
	if len(vertices) == 0 {
		return
	}
	goDRSubmitStrip(unsafe.Pointer(hdr), unsafe.Pointer(&vertices[0]), int32(len(vertices)))
}

func (d *DirectRenderer) Triangle(hdr *PvrPolyHdr,
	x1, y1, x2, y2, x3, y3 float32, z float32, color uint32) {
	goDRSubmitHeader(unsafe.Pointer(hdr))
	goDRSubmitVertexXYZC(PVR_CMD_VERTEX, x1, y1, z, color)
	goDRSubmitVertexXYZC(PVR_CMD_VERTEX, x2, y2, z, color)
	goDRSubmitVertexXYZC(PVR_CMD_VERTEX_EOL, x3, y3, z, color)
}

func (d *DirectRenderer) Quad(hdr *PvrPolyHdr,
	x1, y1, x2, y2, x3, y3, x4, y4 float32, z float32, color uint32) {
	goDRSubmitHeader(unsafe.Pointer(hdr))
	goDRSubmitVertexXYZC(PVR_CMD_VERTEX, x1, y1, z, color)
	goDRSubmitVertexXYZC(PVR_CMD_VERTEX, x2, y2, z, color)
	goDRSubmitVertexXYZC(PVR_CMD_VERTEX, x3, y3, z, color)
	goDRSubmitVertexXYZC(PVR_CMD_VERTEX_EOL, x4, y4, z, color)
}

func (d *DirectRenderer) CheckAlignment() error {
	result := goDRCheckAlignment()
	if result == 0 {
		return nil
	}
	return &DRError{Code: int(result), Message: "buffer alignment check failed"}
}

func (d *DirectRenderer) GetVertexBuffer() uintptr {
	return uintptr(goDRGetVertexBuffer())
}

func (d *DirectRenderer) GetHeaderBuffer() uintptr {
	return uintptr(goDRGetHeaderBuffer())
}

type DRError struct {
	Code    int
	Message string
}

func (e *DRError) Error() string {
	return e.Message
}
