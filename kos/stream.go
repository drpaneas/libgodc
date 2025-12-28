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
	CD_STATUS_READ_FAIL int32 = -1 // Read failed
	CD_STATUS_BUSY      int32 = 0  // Drive busy
	CD_STATUS_PAUSED    int32 = 1  // Paused
	CD_STATUS_STANDBY   int32 = 2  // Standby
	CD_STATUS_PLAYING   int32 = 3  // Playing
	CD_STATUS_SEEKING   int32 = 4  // Seeking
	CD_STATUS_SCANNING  int32 = 5  // Scanning
	CD_STATUS_OPEN      int32 = 6  // Tray open
	CD_STATUS_NO_DISC   int32 = 7  // No disc
	CD_STATUS_RETRY     int32 = 8  // Retry
	CD_STATUS_ERROR     int32 = 9  // Error
	CD_STATUS_FATAL     int32 = 12 // Fatal error
)

const (
	CD_CDDA    int32 = 0x00 // Audio CD
	CD_CDROM   int32 = 0x10 // CD-ROM
	CD_CDROMXA int32 = 0x20 // CD-ROM XA
	CD_CDI     int32 = 0x30 // CD-i
	CD_GDROM   int32 = 0x80 // GD-ROM
	CD_FAIL    int32 = 0xf0 // Detection failed
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
