//go:build !gccgo

package kos

const (
	CONT_C           uint32 = 1 << 0
	CONT_B           uint32 = 1 << 1
	CONT_A           uint32 = 1 << 2
	CONT_START       uint32 = 1 << 3
	CONT_DPAD_UP     uint32 = 1 << 4
	CONT_DPAD_DOWN   uint32 = 1 << 5
	CONT_DPAD_LEFT   uint32 = 1 << 6
	CONT_DPAD_RIGHT  uint32 = 1 << 7
	CONT_Z           uint32 = 1 << 8
	CONT_Y           uint32 = 1 << 9
	CONT_X           uint32 = 1 << 10
	CONT_D           uint32 = 1 << 11
	CONT_DPAD2_UP    uint32 = 1 << 12
	CONT_DPAD2_DOWN  uint32 = 1 << 13
	CONT_DPAD2_LEFT  uint32 = 1 << 14
	CONT_DPAD2_RIGHT uint32 = 1 << 15

	CONT_RESET_BUTTONS = CONT_A | CONT_B | CONT_X | CONT_Y | CONT_START
)

const (
	MAPLE_FUNC_PURUPURU   uint32 = 0x00010000
	MAPLE_FUNC_MOUSE      uint32 = 0x00020000
	MAPLE_FUNC_CAMERA     uint32 = 0x00080000
	MAPLE_FUNC_CONTROLLER uint32 = 0x01000000
	MAPLE_FUNC_MEMCARD    uint32 = 0x02000000
	MAPLE_FUNC_LCD        uint32 = 0x04000000
	MAPLE_FUNC_CLOCK      uint32 = 0x08000000
	MAPLE_FUNC_MICROPHONE uint32 = 0x10000000
	MAPLE_FUNC_ARGUN      uint32 = 0x20000000
	MAPLE_FUNC_KEYBOARD   uint32 = 0x40000000
	MAPLE_FUNC_LIGHTGUN   uint32 = 0x80000000
)

const (
	DM_320x240     int32 = 1
	DM_640x480     int32 = 2
	DM_800x608     int32 = 3
	DM_256x256     int32 = 4
	DM_768x480     int32 = 5
	DM_768x576     int32 = 6
	DM_MULTIBUFFER int32 = 0x1000
)

const (
	PM_RGB555  int32 = 0
	PM_RGB565  int32 = 1
	PM_RGB888P int32 = 2
	PM_RGB0888 int32 = 3
)

const (
	PVR_TXRFMT_NONE        uint32 = 0
	PVR_TXRFMT_VQ_ENABLE   uint32 = 1 << 30
	PVR_TXRFMT_ARGB1555    uint32 = 0 << 27
	PVR_TXRFMT_RGB565      uint32 = 1 << 27
	PVR_TXRFMT_ARGB4444    uint32 = 2 << 27
	PVR_TXRFMT_YUV422      uint32 = 3 << 27
	PVR_TXRFMT_BUMP        uint32 = 4 << 27
	PVR_TXRFMT_PAL4BPP     uint32 = 5 << 27
	PVR_TXRFMT_PAL8BPP     uint32 = 6 << 27
	PVR_TXRFMT_TWIDDLED    uint32 = 0 << 26
	PVR_TXRFMT_NONTWIDDLED uint32 = 1 << 26
	PVR_TXRFMT_NOSTRIDE    uint32 = 0 << 21
	PVR_TXRFMT_STRIDE      uint32 = 1 << 21
)

const (
	PVR_BLEND_ZERO         uint32 = 0
	PVR_BLEND_ONE          uint32 = 1
	PVR_BLEND_DESTCOLOR    uint32 = 2
	PVR_BLEND_INVDESTCOLOR uint32 = 3
	PVR_BLEND_SRCALPHA     uint32 = 4
	PVR_BLEND_INVSRCALPHA  uint32 = 5
	PVR_BLEND_DESTALPHA    uint32 = 6
	PVR_BLEND_INVDESTALPHA uint32 = 7
)

const (
	PVR_DEPTHCMP_NEVER    uint32 = 0
	PVR_DEPTHCMP_LESS     uint32 = 1
	PVR_DEPTHCMP_EQUAL    uint32 = 2
	PVR_DEPTHCMP_LEQUAL   uint32 = 3
	PVR_DEPTHCMP_GREATER  uint32 = 4
	PVR_DEPTHCMP_NOTEQUAL uint32 = 5
	PVR_DEPTHCMP_GEQUAL   uint32 = 6
	PVR_DEPTHCMP_ALWAYS   uint32 = 7
)

const (
	PVR_CULLING_NONE  uint32 = 0
	PVR_CULLING_SMALL uint32 = 1
	PVR_CULLING_CCW   uint32 = 2
	PVR_CULLING_CW    uint32 = 3
)

const (
	PVR_FILTER_NONE       uint32 = 0
	PVR_FILTER_BILINEAR   uint32 = 2
	PVR_FILTER_TRILINEAR1 uint32 = 4
	PVR_FILTER_TRILINEAR2 uint32 = 6
)

const (
	PVR_SHADE_FLAT    uint32 = 0
	PVR_SHADE_GOURAUD uint32 = 1
)

const (
	PVR_CMD_VERTEX     uint32 = 0xE0000000
	PVR_CMD_VERTEX_EOL uint32 = 0xF0000000
)

func NewVertex() *PvrVertex             { panic("kos: not on Dreamcast") }
func NewVertices(count int) []PvrVertex { panic("kos: not on Dreamcast") }
