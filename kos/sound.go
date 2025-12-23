//go:build gccgo

package kos

import "unsafe"

const (
	SPU_RAM_BASE = 0x00800000
)

//extern spu_init
func SpuInit() int32

//extern spu_shutdown
func SpuShutdown() int32

//extern spu_enable
func SpuEnable()

//extern spu_disable
func SpuDisable()

//extern spu_reset_chans
func SpuResetChans()

//extern spu_cdda_volume
func SpuCddaVolume(leftVolume, rightVolume int32)

//extern spu_cdda_pan
func SpuCddaPan(leftPan, rightPan int32)

//extern spu_master_mixer
func SpuMasterMixer(volume, stereo int32)

//extern spu_memload
func spuMemload(to uintptr, from unsafe.Pointer, length uint32)

func SpuMemload(to uint32, data []byte) {
	if len(data) == 0 {
		return
	}
	spuMemload(uintptr(to), unsafe.Pointer(&data[0]), uint32(len(data)))
}

//extern spu_memload_sq
func spuMemloadSq(to uintptr, from unsafe.Pointer, length uint32)

func SpuMemloadSq(to uint32, data []byte) {
	if len(data) == 0 {
		return
	}
	spuMemloadSq(uintptr(to), unsafe.Pointer(&data[0]), uint32(len(data)))
}

//extern spu_memload_dma
func spuMemloadDma(to uintptr, from unsafe.Pointer, length uint32)

func SpuMemloadDma(to uint32, data []byte) {
	if len(data) == 0 {
		return
	}
	spuMemloadDma(uintptr(to), unsafe.Pointer(&data[0]), uint32(len(data)))
}

//extern spu_memread
func spuMemread(to unsafe.Pointer, from uintptr, length uint32)

func SpuMemread(from uint32, length int) []byte {
	if length <= 0 {
		return nil
	}
	data := make([]byte, length)
	spuMemread(unsafe.Pointer(&data[0]), uintptr(from), uint32(length))
	return data
}

//extern spu_memset
func SpuMemset(to uintptr, what uint32, length uint32)

//extern spu_memset_sq
func SpuMemsetSq(to uintptr, what uint32, length uint32)

type SfxHandle uint32

const SFXHND_INVALID SfxHandle = 0

//extern snd_sfx_load
func sndSfxLoad(fn uintptr) uint32

func SndSfxLoad(filename string) SfxHandle {
	cstr := make([]byte, len(filename)+1)
	copy(cstr, filename)
	return SfxHandle(sndSfxLoad(uintptr(unsafe.Pointer(&cstr[0]))))
}

//extern snd_sfx_load_ex
func sndSfxLoadEx(fn uintptr, rate uint32, bitsize uint16, channels uint16) uint32

func SndSfxLoadEx(filename string, rate uint32, bitsize, channels uint16) SfxHandle {
	cstr := make([]byte, len(filename)+1)
	copy(cstr, filename)
	return SfxHandle(sndSfxLoadEx(uintptr(unsafe.Pointer(&cstr[0])), rate, bitsize, channels))
}

//extern snd_sfx_unload
func SndSfxUnload(idx SfxHandle)

//extern snd_sfx_unload_all
func SndSfxUnloadAll()

//extern snd_sfx_play
func SndSfxPlay(idx SfxHandle, vol, pan int32) int32

//extern snd_sfx_play_chn
func SndSfxPlayChn(chn int32, idx SfxHandle, vol, pan int32) int32

//extern snd_sfx_stop
func SndSfxStop(chn int32)

//extern snd_sfx_stop_all
func SndSfxStopAll()

//extern snd_sfx_chn_alloc
func SndSfxChnAlloc() int32

//extern snd_sfx_chn_free
func SndSfxChnFree(chn int32)

type SfxPlayData struct {
	Chn       int32
	Idx       SfxHandle
	Vol       int32
	Pan       int32
	Loop      int32
	Freq      int32
	LoopStart uint32
	LoopEnd   uint32
}

//extern snd_sfx_play_ex
func sndSfxPlayEx(data uintptr) int32

func SndSfxPlayEx(data *SfxPlayData) int32 {
	return sndSfxPlayEx(uintptr(unsafe.Pointer(data)))
}
