//go:build !gccgo

package kos

import "unsafe"

const (
	SPU_RAM_BASE = 0x00800000
)

func SpuInit() int32     { panic("kos: not on Dreamcast") }
func SpuShutdown() int32 { panic("kos: not on Dreamcast") }
func SpuEnable()         { panic("kos: not on Dreamcast") }
func SpuDisable()        { panic("kos: not on Dreamcast") }
func SpuResetChans()     { panic("kos: not on Dreamcast") }

func SpuCddaVolume(leftVolume, rightVolume int32) { panic("kos: not on Dreamcast") }
func SpuCddaPan(leftPan, rightPan int32)          { panic("kos: not on Dreamcast") }
func SpuMasterMixer(volume, stereo int32)         { panic("kos: not on Dreamcast") }

func spuMemload(to uintptr, from unsafe.Pointer, length uint32)   { panic("kos: not on Dreamcast") }
func SpuMemload(to uint32, data []byte)                           { panic("kos: not on Dreamcast") }
func spuMemloadSq(to uintptr, from unsafe.Pointer, length uint32) { panic("kos: not on Dreamcast") }
func SpuMemloadSq(to uint32, data []byte)                         { panic("kos: not on Dreamcast") }
func spuMemloadDma(to uintptr, from unsafe.Pointer, length uint32) {
	panic("kos: not on Dreamcast")
}
func SpuMemloadDma(to uint32, data []byte)                      { panic("kos: not on Dreamcast") }
func spuMemread(to unsafe.Pointer, from uintptr, length uint32) { panic("kos: not on Dreamcast") }
func SpuMemread(from uint32, length int) []byte                 { panic("kos: not on Dreamcast") }
func SpuMemset(to uintptr, what uint32, length uint32)          { panic("kos: not on Dreamcast") }
func SpuMemsetSq(to uintptr, what uint32, length uint32)        { panic("kos: not on Dreamcast") }

type SfxHandle uint32

const SFXHND_INVALID SfxHandle = 0

func sndSfxLoad(fn uintptr) uint32         { panic("kos: not on Dreamcast") }
func SndSfxLoad(filename string) SfxHandle { panic("kos: not on Dreamcast") }
func sndSfxLoadEx(fn uintptr, rate uint32, bitsize uint16, channels uint16) uint32 {
	panic("kos: not on Dreamcast")
}
func SndSfxLoadEx(filename string, rate uint32, bitsize, channels uint16) SfxHandle {
	panic("kos: not on Dreamcast")
}
func SndSfxUnload(idx SfxHandle)                     { panic("kos: not on Dreamcast") }
func SndSfxUnloadAll()                               { panic("kos: not on Dreamcast") }
func SndSfxPlay(idx SfxHandle, vol, pan int32) int32 { panic("kos: not on Dreamcast") }
func SndSfxPlayChn(chn int32, idx SfxHandle, vol, pan int32) int32 {
	panic("kos: not on Dreamcast")
}
func SndSfxStop(chn int32)    { panic("kos: not on Dreamcast") }
func SndSfxStopAll()          { panic("kos: not on Dreamcast") }
func SndSfxChnAlloc() int32   { panic("kos: not on Dreamcast") }
func SndSfxChnFree(chn int32) { panic("kos: not on Dreamcast") }

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

func sndSfxPlayEx(data uintptr) int32      { panic("kos: not on Dreamcast") }
func SndSfxPlayEx(data *SfxPlayData) int32 { panic("kos: not on Dreamcast") }
