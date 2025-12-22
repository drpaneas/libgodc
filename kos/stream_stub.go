//go:build !gccgo

package kos

type WavStreamHnd int32

const WavStreamInvalid WavStreamHnd = -1

func wavCreate(filename uintptr, loop int32) int32      { panic("kos: not on Dreamcast") }
func WavInit() int32                                    { panic("kos: not on Dreamcast") }
func WavShutdown()                                      { panic("kos: not on Dreamcast") }
func WavCreate(filename string, loop bool) WavStreamHnd { panic("kos: not on Dreamcast") }
func WavDestroy(hnd WavStreamHnd)                       { panic("kos: not on Dreamcast") }
func WavPlay(hnd WavStreamHnd)                          { panic("kos: not on Dreamcast") }
func WavPause(hnd WavStreamHnd)                         { panic("kos: not on Dreamcast") }
func WavStop(hnd WavStreamHnd)                          { panic("kos: not on Dreamcast") }
func WavVolume(hnd WavStreamHnd, vol int32)             { panic("kos: not on Dreamcast") }
func WavIsPlaying(hnd WavStreamHnd) int32               { panic("kos: not on Dreamcast") }

func oggStart(filename uintptr, loop int32) int32 { panic("kos: not on Dreamcast") }
func oggGetArtist() uintptr                       { panic("kos: not on Dreamcast") }
func oggGetTitle() uintptr                        { panic("kos: not on Dreamcast") }
func oggGetGenre() uintptr                        { panic("kos: not on Dreamcast") }
func OggInit() int32                              { panic("kos: not on Dreamcast") }
func OggShutdown()                                { panic("kos: not on Dreamcast") }
func OggStart(filename string, loop bool) int32   { panic("kos: not on Dreamcast") }
func OggStop()                                    { panic("kos: not on Dreamcast") }
func OggIsPlaying() int32                         { panic("kos: not on Dreamcast") }
func OggVolume(vol int32)                         { panic("kos: not on Dreamcast") }
func OggGetBitrate() int32                        { panic("kos: not on Dreamcast") }
func OggGetPosition() int32                       { panic("kos: not on Dreamcast") }
func OggGetArtist() string                        { panic("kos: not on Dreamcast") }
func OggGetTitle() string                         { panic("kos: not on Dreamcast") }
func OggGetGenre() string                         { panic("kos: not on Dreamcast") }
func OggWaitStart()                               { panic("kos: not on Dreamcast") }

const (
	CDDA_TRACKS  = 1
	CDDA_SECTORS = 2
)

func cdromGetStatus(status, discType *int32) int32 { panic("kos: not on Dreamcast") }
func CdromCddaPlay(start, end, loops uint32, mode int32) int32 {
	panic("kos: not on Dreamcast")
}
func CdromCddaPause() int32                 { panic("kos: not on Dreamcast") }
func CdromCddaResume() int32                { panic("kos: not on Dreamcast") }
func CdromSpinDown() int32                  { panic("kos: not on Dreamcast") }
func CdromInit()                            { panic("kos: not on Dreamcast") }
func CdromShutdown()                        { panic("kos: not on Dreamcast") }
func CdromReinit() int32                    { panic("kos: not on Dreamcast") }
func CdromGetStatus() (int32, int32, int32) { panic("kos: not on Dreamcast") }

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

func gostring(ptr uintptr) string { return "" }
