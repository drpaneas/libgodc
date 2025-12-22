//go:build gccgo

package kos

import "unsafe"

type WavStreamHnd int32

const WavStreamInvalid WavStreamHnd = -1

//extern wav_init
func WavInit() int32

//extern wav_shutdown
func WavShutdown()

//extern wav_create
func wavCreate(filename uintptr, loop int32) int32

func WavCreate(filename string, loop bool) WavStreamHnd {
	cstr := make([]byte, len(filename)+1)
	copy(cstr, filename)
	loopVal := int32(0)
	if loop {
		loopVal = 1
	}
	return WavStreamHnd(wavCreate(uintptr(unsafe.Pointer(&cstr[0])), loopVal))
}

//extern wav_destroy
func WavDestroy(hnd WavStreamHnd)

//extern wav_play
func WavPlay(hnd WavStreamHnd)

//extern wav_pause
func WavPause(hnd WavStreamHnd)

//extern wav_stop
func WavStop(hnd WavStreamHnd)

//extern wav_volume
func WavVolume(hnd WavStreamHnd, vol int32)

//extern wav_is_playing
func WavIsPlaying(hnd WavStreamHnd) int32

//extern sndoggvorbis_init
func OggInit() int32

//extern sndoggvorbis_shutdown
func OggShutdown()

//extern sndoggvorbis_start
func oggStart(filename uintptr, loop int32) int32

func OggStart(filename string, loop bool) int32 {
	cstr := make([]byte, len(filename)+1)
	copy(cstr, filename)
	loopVal := int32(0)
	if loop {
		loopVal = 1
	}
	return oggStart(uintptr(unsafe.Pointer(&cstr[0])), loopVal)
}

//extern sndoggvorbis_stop
func OggStop()

//extern sndoggvorbis_isplaying
func OggIsPlaying() int32

//extern sndoggvorbis_volume
func OggVolume(vol int32)

//extern sndoggvorbis_getbitrate
func OggGetBitrate() int32

//extern sndoggvorbis_getposition
func OggGetPosition() int32

//extern sndoggvorbis_getartist
func oggGetArtist() uintptr

func OggGetArtist() string {
	ptr := oggGetArtist()
	if ptr == 0 {
		return ""
	}
	return gostring(ptr)
}

//extern sndoggvorbis_gettitle
func oggGetTitle() uintptr

func OggGetTitle() string {
	ptr := oggGetTitle()
	if ptr == 0 {
		return ""
	}
	return gostring(ptr)
}

//extern sndoggvorbis_getgenre
func oggGetGenre() uintptr

func OggGetGenre() string {
	ptr := oggGetGenre()
	if ptr == 0 {
		return ""
	}
	return gostring(ptr)
}

//extern sndoggvorbis_wait_start
func OggWaitStart()

const (
	CDDA_TRACKS  = 1
	CDDA_SECTORS = 2
)

//extern cdrom_cdda_play
func CdromCddaPlay(start, end, loops uint32, mode int32) int32

//extern cdrom_cdda_pause
func CdromCddaPause() int32

//extern cdrom_cdda_resume
func CdromCddaResume() int32

//extern cdrom_spin_down
func CdromSpinDown() int32

//extern cdrom_init
func CdromInit()

//extern cdrom_shutdown
func CdromShutdown()

//extern cdrom_reinit
func CdromReinit() int32

//extern cdrom_get_status
func cdromGetStatus(status, discType *int32) int32

func CdromGetStatus() (int32, int32, int32) {
	var status, discType int32
	err := cdromGetStatus(&status, &discType)
	return status, discType, err
}

const (
	CD_STATUS_BUSY    = 0
	CD_STATUS_PAUSED  = 1
	CD_STATUS_STANDBY = 2
	CD_STATUS_PLAYING = 3
	CD_STATUS_SEEKING = 4
	CD_STATUS_OPEN    = 6
	CD_STATUS_NO_DISC = 7
)

const (
	CD_CDDA    = 0x00
	CD_CDROM   = 0x10
	CD_CDROMXA = 0x20
	CD_GDROM   = 0x80
)

func gostring(ptr uintptr) string {
	if ptr == 0 {
		return ""
	}
	var length int
	for {
		b := *(*byte)(unsafe.Pointer(ptr + uintptr(length)))
		if b == 0 {
			break
		}
		length++
		if length > 1024 {
			break
		}
	}
	if length == 0 {
		return ""
	}
	bytes := make([]byte, length)
	for i := 0; i < length; i++ {
		bytes[i] = *(*byte)(unsafe.Pointer(ptr + uintptr(i)))
	}
	return string(bytes)
}
