//go:build !gccgo

package kos

import "unsafe"

const (
	PVR_LIST_OP_POLY uint32 = 0
	PVR_LIST_OP_MOD  uint32 = 1
	PVR_LIST_TR_POLY uint32 = 2
	PVR_LIST_TR_MOD  uint32 = 3
	PVR_LIST_PT_POLY uint32 = 4
)

const (
	PVR_BINSIZE_0  int32 = 0
	PVR_BINSIZE_8  int32 = 8
	PVR_BINSIZE_16 int32 = 16
	PVR_BINSIZE_32 int32 = 32
)

type PvrInitParams struct {
	OpbSizes              [5]int32
	VertexBufSize         int32
	DmaEnabled            int32
	FsaaEnabled           int32
	AutosortDisabled      int32
	OpbOverflowCount      int32
	VbufDoublebufDisabled int32
}

func pvrInit(params unsafe.Pointer) int32 { panic("kos: not on Dreamcast") }
func PvrInit(params *PvrInitParams) int32 { panic("kos: not on Dreamcast") }
func PvrInitDefaults() int32              { panic("kos: not on Dreamcast") }
func PvrShutdown() int32                  { panic("kos: not on Dreamcast") }

func PvrSceneBegin()           { panic("kos: not on Dreamcast") }
func PvrSceneFinish() int32    { panic("kos: not on Dreamcast") }
func PvrWaitReady() int32      { panic("kos: not on Dreamcast") }
func PvrCheckReady() int32     { panic("kos: not on Dreamcast") }
func PvrWaitRenderDone() int32 { panic("kos: not on Dreamcast") }

func PvrListBegin(list uint32) int32 { panic("kos: not on Dreamcast") }
func PvrListFinish() int32           { panic("kos: not on Dreamcast") }
func PvrListFlush(list uint32) int32 { panic("kos: not on Dreamcast") }

func pvrPrim(data unsafe.Pointer, size uint32) int32 { panic("kos: not on Dreamcast") }
func PvrPrimBytes(data []byte) int32                 { panic("kos: not on Dreamcast") }
func pvrListPrim(list uint32, data unsafe.Pointer, size uint32) int32 {
	panic("kos: not on Dreamcast")
}
func PvrListPrim(list uint32, data unsafe.Pointer, size uint32) int32 {
	panic("kos: not on Dreamcast")
}

type PvrDrState struct {
	data [4]uint32
}

func pvrDrInit(state unsafe.Pointer) { panic("kos: not on Dreamcast") }
func (s *PvrDrState) Init()          { panic("kos: not on Dreamcast") }
func PvrDrFinish()                   { panic("kos: not on Dreamcast") }

func PvrVertexDmaEnabled() int32 { panic("kos: not on Dreamcast") }

type PvrPtr uintptr

func PvrMemMalloc(size uint32) PvrPtr { panic("kos: not on Dreamcast") }
func PvrMemFree(ptr PvrPtr)           { panic("kos: not on Dreamcast") }
func PvrMemAvailable() uint32         { panic("kos: not on Dreamcast") }
func PvrMemReset()                    { panic("kos: not on Dreamcast") }
func PvrMemPrintList()                { panic("kos: not on Dreamcast") }
func PvrMemStats()                    { panic("kos: not on Dreamcast") }
func PvrGetFrontBuffer() PvrPtr       { panic("kos: not on Dreamcast") }
func PvrSetBgColor(r, g, b float32)   { panic("kos: not on Dreamcast") }

type PvrStats struct {
	FrameLastTime    uint64
	RegLastTime      uint64
	RndLastTime      uint64
	BufLastTime      uint64
	FrameCount       uint32
	VblCount         uint32
	VtxBufferUsed    uint32
	VtxBufferUsedMax uint32
	FrameRate        float32
	EnabledListMask  uint32
}

func pvrGetStats(stat unsafe.Pointer) int32 { panic("kos: not on Dreamcast") }
func PvrGetStats(stat *PvrStats) int32      { panic("kos: not on Dreamcast") }
func PvrGetVblCount() int32                 { panic("kos: not on Dreamcast") }

func pvrSetVertbuf(list uint32, buffer unsafe.Pointer, length uint32) unsafe.Pointer {
	panic("kos: not on Dreamcast")
}
func PvrSetVertbuf(list uint32, buffer unsafe.Pointer, length uint32) unsafe.Pointer {
	panic("kos: not on Dreamcast")
}
func PvrVertbufTail(list uint32) unsafe.Pointer { panic("kos: not on Dreamcast") }
func PvrVertbufWritten(list uint32, amt uint32) { panic("kos: not on Dreamcast") }

func SqLock(dest unsafe.Pointer) unsafe.Pointer { panic("kos: not on Dreamcast") }
func SqUnlock()                                 { panic("kos: not on Dreamcast") }
func sqFlush(sq unsafe.Pointer)                 { panic("kos: not on Dreamcast") }
func SqFlush(sq unsafe.Pointer)                 { panic("kos: not on Dreamcast") }

func MatTransformSq(input, output unsafe.Pointer, veccnt int32) { panic("kos: not on Dreamcast") }

func AlignedAlloc(alignment, size uint32) unsafe.Pointer { panic("kos: not on Dreamcast") }
func SqMaskDestAddr(addr unsafe.Pointer) unsafe.Pointer  { panic("kos: not on Dreamcast") }
