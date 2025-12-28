//go:build gccgo

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
	// Generic modes (auto-detect output type)
	DM_320x240 int32 = 0x1000 // 320x240 resolution
	DM_640x480 int32 = 0x1001 // 640x480 resolution
	DM_256x256 int32 = 0x1002 // 256x256 resolution
	DM_768x480 int32 = 0x1003 // 768x480 resolution
	DM_768x576 int32 = 0x1004 // 768x576 resolution

	// Specific modes
	DM_INVALID         int32 = 0  // Invalid display mode
	DM_320x240_VGA     int32 = 1  // 320x240 VGA 60Hz
	DM_320x240_NTSC    int32 = 2  // 320x240 NTSC 60Hz
	DM_640x480_VGA     int32 = 3  // 640x480 VGA 60Hz
	DM_640x480_NTSC_IL int32 = 4  // 640x480 NTSC Interlaced 60Hz
	DM_640x480_PAL_IL  int32 = 5  // 640x480 PAL Interlaced 50Hz
	DM_256x256_PAL_IL  int32 = 6  // 256x256 PAL Interlaced 50Hz
	DM_768x480_NTSC_IL int32 = 7  // 768x480 NTSC Interlaced 60Hz
	DM_768x576_PAL_IL  int32 = 8  // 768x576 PAL Interlaced 50Hz
	DM_768x480_PAL_IL  int32 = 9  // 768x480 PAL Interlaced 50Hz
	DM_320x240_PAL     int32 = 10 // 320x240 PAL 50Hz

	DM_MULTIBUFFER int32 = 0x2000 // Enable multi-buffering
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
	PVR_FILTER_NONE       uint32 = 0 // No filtering (point sample)
	PVR_FILTER_NEAREST    uint32 = 0 // Alias for PVR_FILTER_NONE
	PVR_FILTER_BILINEAR   uint32 = 1 // Bilinear interpolation
	PVR_FILTER_TRILINEAR1 uint32 = 2 // Trilinear interpolation pass 1
	PVR_FILTER_TRILINEAR2 uint32 = 3 // Trilinear interpolation pass 2
)

const (
	PVR_SHADE_FLAT    uint32 = 0
	PVR_SHADE_GOURAUD uint32 = 1
)

const (
	PVR_CMD_POLYHDR    uint32 = 0x80840000 // PVR polygon header
	PVR_CMD_VERTEX     uint32 = 0xE0000000 // PVR vertex data
	PVR_CMD_VERTEX_EOL uint32 = 0xF0000000 // PVR vertex, end of strip
	PVR_CMD_USERCLIP   uint32 = 0x20000000 // PVR user clipping area
	PVR_CMD_MODIFIER   uint32 = 0x80000000 // PVR modifier volume
	PVR_CMD_SPRITE     uint32 = 0xA0000000 // PVR sprite header
)

// Fog types
const (
	PVR_FOG_TABLE   uint32 = 0 // Table fog
	PVR_FOG_VERTEX  uint32 = 1 // Vertex fog
	PVR_FOG_DISABLE uint32 = 2 // Disable fog
	PVR_FOG_TABLE2  uint32 = 3 // Table fog mode 2
)

// Texture environment modes
const (
	PVR_TXRENV_REPLACE       uint32 = 0 // px = ARGB(tex)
	PVR_TXRENV_MODULATE      uint32 = 1 // px = A(tex) + RGB(col) * RGB(tex)
	PVR_TXRENV_DECAL         uint32 = 2 // px = A(col) + RGB(tex) * A(tex) + RGB(col) * (1 - A(tex))
	PVR_TXRENV_MODULATEALPHA uint32 = 3 // px = ARGB(col) * ARGB(tex)
)

// UV flip modes
const (
	PVR_UVFLIP_NONE uint32 = 0 // No flipped coordinates
	PVR_UVFLIP_V    uint32 = 1 // Flip V only
	PVR_UVFLIP_U    uint32 = 2 // Flip U only
	PVR_UVFLIP_UV   uint32 = 3 // Flip U and V
)

// UV clamp modes
const (
	PVR_UVCLAMP_NONE uint32 = 0 // Disable clamping
	PVR_UVCLAMP_V    uint32 = 1 // Clamp V only
	PVR_UVCLAMP_U    uint32 = 2 // Clamp U only
	PVR_UVCLAMP_UV   uint32 = 3 // Clamp U and V
)

// Mipmap bias values
const (
	PVR_MIPBIAS_0_25   uint32 = 1
	PVR_MIPBIAS_0_50   uint32 = 2
	PVR_MIPBIAS_0_75   uint32 = 3
	PVR_MIPBIAS_1_00   uint32 = 4 // Normal (default)
	PVR_MIPBIAS_NORMAL uint32 = 4 // Alias for PVR_MIPBIAS_1_00
	PVR_MIPBIAS_1_25   uint32 = 5
	PVR_MIPBIAS_1_50   uint32 = 6
	PVR_MIPBIAS_1_75   uint32 = 7
	PVR_MIPBIAS_2_00   uint32 = 8
	PVR_MIPBIAS_2_25   uint32 = 9
	PVR_MIPBIAS_2_50   uint32 = 10
	PVR_MIPBIAS_2_75   uint32 = 11
	PVR_MIPBIAS_3_00   uint32 = 12
	PVR_MIPBIAS_3_25   uint32 = 13
	PVR_MIPBIAS_3_50   uint32 = 14
	PVR_MIPBIAS_3_75   uint32 = 15
)

// Depth write toggle
const (
	PVR_DEPTHWRITE_ENABLE  uint32 = 0 // Update the Z value
	PVR_DEPTHWRITE_DISABLE uint32 = 1 // Do not update the Z value
)

// Texture toggle
const (
	PVR_TEXTURE_DISABLE uint32 = 0 // Disable texturing
	PVR_TEXTURE_ENABLE  uint32 = 1 // Enable texturing
)

// Specular/offset color toggle
const (
	PVR_SPECULAR_DISABLE uint32 = 0 // Disable offset colors
	PVR_SPECULAR_ENABLE  uint32 = 1 // Enable offset colors
)

// Alpha blending toggle
const (
	PVR_ALPHA_DISABLE uint32 = 0 // Disable alpha blending
	PVR_ALPHA_ENABLE  uint32 = 1 // Enable alpha blending
)

// Texture alpha toggle (note: inverted from PVR_ALPHA_*)
const (
	PVR_TXRALPHA_ENABLE  uint32 = 0 // Enable texture alpha
	PVR_TXRALPHA_DISABLE uint32 = 1 // Disable texture alpha
)

// Blend toggle
const (
	PVR_BLEND_DISABLE uint32 = 0 // Disable blending
	PVR_BLEND_ENABLE  uint32 = 1 // Enable blending
)

// Mipmap toggle
const (
	PVR_MIPMAP_DISABLE uint32 = 0 // Disable mipmap processing
	PVR_MIPMAP_ENABLE  uint32 = 1 // Enable mipmap processing
)

// Modifier toggle
const (
	PVR_MODIFIER_DISABLE uint32 = 0 // Disable modifier effects
	PVR_MODIFIER_ENABLE  uint32 = 1 // Enable modifier effects
)

// Color clamping toggle
const (
	PVR_CLRCLAMP_DISABLE uint32 = 0 // Disable color clamping
	PVR_CLRCLAMP_ENABLE  uint32 = 1 // Enable color clamping
)

// Color formats
const (
	PVR_CLRFMT_ARGBPACKED     uint32 = 0 // 32-bit integer ARGB
	PVR_CLRFMT_4FLOATS        uint32 = 1 // 4 floating point values
	PVR_CLRFMT_INTENSITY      uint32 = 2 // Intensity color
	PVR_CLRFMT_INTENSITY_PREV uint32 = 3 // Use last intensity
)

// UV formats
const (
	PVR_UVFMT_32BIT uint32 = 0 // 32-bit floating point U/V
	PVR_UVFMT_16BIT uint32 = 1 // 16-bit floating point U/V
)

// User clip modes
const (
	PVR_USERCLIP_DISABLE uint32 = 0 // Disable clipping
	PVR_USERCLIP_INSIDE  uint32 = 2 // Enable clipping inside area
	PVR_USERCLIP_OUTSIDE uint32 = 3 // Enable clipping outside area
)

func NewVertex() *PvrVertex {
	return new(PvrVertex)
}

func NewVertices(count int) []PvrVertex {
	return make([]PvrVertex, count)
}
