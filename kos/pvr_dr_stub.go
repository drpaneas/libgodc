//go:build !dreamcast
// +build !dreamcast

package kos

type DirectRenderer struct {
	initialized bool
}

var DR DirectRenderer

func (d *DirectRenderer) Init() error                                                              { return nil }
func (d *DirectRenderer) Shutdown()                                                                {}
func (d *DirectRenderer) BeginFrame()                                                              {}
func (d *DirectRenderer) EndFrame()                                                                {}
func (d *DirectRenderer) GetVertex() *PvrVertex                                                    { return nil }
func (d *DirectRenderer) GetVertexAt(index int) *PvrVertex                                         { return nil }
func (d *DirectRenderer) GetVertexCount() int                                                      { return 0 }
func (d *DirectRenderer) SetVertexCount(count int)                                                 {}
func (d *DirectRenderer) GetHeader() *PvrPolyHdr                                                   { return nil }
func (d *DirectRenderer) SubmitHeader(hdr *PvrPolyHdr)                                             {}
func (d *DirectRenderer) SubmitVertex(v *PvrVertex)                                                {}
func (d *DirectRenderer) SubmitVertexXYZC(flags uint32, x, y, z float32, argb uint32)              {}
func (d *DirectRenderer) SubmitVertexFull(flags uint32, x, y, z, u, v float32, argb, oargb uint32) {}
func (d *DirectRenderer) SubmitVertices(start, end int)                                            {}
func (d *DirectRenderer) SubmitAllVertices()                                                       {}
func (d *DirectRenderer) SubmitStrip(hdr *PvrPolyHdr, vertices []PvrVertex)                        {}
func (d *DirectRenderer) Triangle(hdr *PvrPolyHdr, x1, y1, x2, y2, x3, y3 float32, z float32, color uint32) {
}
func (d *DirectRenderer) Quad(hdr *PvrPolyHdr, x1, y1, x2, y2, x3, y3, x4, y4 float32, z float32, color uint32) {
}
func (d *DirectRenderer) CheckAlignment() error    { return nil }
func (d *DirectRenderer) GetVertexBuffer() uintptr { return 0 }
func (d *DirectRenderer) GetHeaderBuffer() uintptr { return 0 }

type DRError struct {
	Code    int
	Message string
}

func (e *DRError) Error() string { return e.Message }
