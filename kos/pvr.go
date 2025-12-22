//go:build gccgo

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

//extern pvr_init
func pvrInit(params unsafe.Pointer) int32

func PvrInit(params *PvrInitParams) int32 {
	return pvrInit(unsafe.Pointer(params))
}

//extern pvr_init_defaults
func PvrInitDefaults() int32

//extern pvr_shutdown
func PvrShutdown() int32

//extern pvr_scene_begin
func PvrSceneBegin()

//extern pvr_scene_finish
func PvrSceneFinish() int32

//extern pvr_wait_ready
func PvrWaitReady() int32

//extern pvr_check_ready
func PvrCheckReady() int32

//extern pvr_wait_render_done
func PvrWaitRenderDone() int32

//extern pvr_list_begin
func PvrListBegin(list uint32) int32

//extern pvr_list_finish
func PvrListFinish() int32

//extern pvr_list_flush
func PvrListFlush(list uint32) int32

//extern pvr_prim
func pvrPrim(data unsafe.Pointer, size uint32) int32

func PvrPrimBytes(data []byte) int32 {
	if len(data) == 0 {
		return -1
	}
	return pvrPrim(unsafe.Pointer(&data[0]), uint32(len(data)))
}

//extern pvr_list_prim
func pvrListPrim(list uint32, data unsafe.Pointer, size uint32) int32

func PvrListPrim(list uint32, data unsafe.Pointer, size uint32) int32 {
	return pvrListPrim(list, data, size)
}

type PvrDrState struct {
	data [4]uint32
}

//extern pvr_dr_init
func pvrDrInit(state unsafe.Pointer)

func (s *PvrDrState) Init() {
	pvrDrInit(unsafe.Pointer(s))
}

//extern pvr_dr_finish
func PvrDrFinish()

//extern pvr_vertex_dma_enabled
func PvrVertexDmaEnabled() int32

type PvrPtr uintptr

//extern pvr_mem_malloc
func PvrMemMalloc(size uint32) PvrPtr

//extern pvr_mem_free
func PvrMemFree(ptr PvrPtr)

//extern pvr_mem_available
func PvrMemAvailable() uint32

//extern pvr_mem_reset
func PvrMemReset()

//extern pvr_mem_print_list
func PvrMemPrintList()

//extern pvr_mem_stats
func PvrMemStats()

//extern pvr_get_front_buffer
func PvrGetFrontBuffer() PvrPtr

//extern pvr_set_bg_color
func PvrSetBgColor(r, g, b float32)

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

//extern pvr_get_stats
func pvrGetStats(stat unsafe.Pointer) int32

func PvrGetStats(stat *PvrStats) int32 {
	return pvrGetStats(unsafe.Pointer(stat))
}

//extern pvr_get_vbl_count
func PvrGetVblCount() int32

//extern pvr_set_vertbuf
func pvrSetVertbuf(list uint32, buffer unsafe.Pointer, length uint32) unsafe.Pointer

func PvrSetVertbuf(list uint32, buffer unsafe.Pointer, length uint32) unsafe.Pointer {
	return pvrSetVertbuf(list, buffer, length)
}

//extern pvr_vertbuf_tail
func PvrVertbufTail(list uint32) unsafe.Pointer

//extern pvr_vertbuf_written
func PvrVertbufWritten(list uint32, amt uint32)

//extern sq_lock
func SqLock(dest unsafe.Pointer) unsafe.Pointer

//extern sq_unlock
func SqUnlock()

//extern __go_sq_flush
func sqFlush(sq unsafe.Pointer)

func SqFlush(sq unsafe.Pointer) {
	sqFlush(sq)
}

//extern mat_transform_sq
func MatTransformSq(input, output unsafe.Pointer, veccnt int32)

//extern aligned_alloc
func AlignedAlloc(alignment, size uint32) unsafe.Pointer

func SqMaskDestAddr(addr unsafe.Pointer) unsafe.Pointer {
	return unsafe.Pointer(uintptr(addr) | 0xe0000000)
}
