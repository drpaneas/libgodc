//go:build !gccgo

package kos

import "unsafe"

// POSIX directory entry types (from sys/dirent.h)
const (
	DT_UNKNOWN uint8 = 0  // Unknown
	DT_FIFO    uint8 = 1  // Named Pipe or FIFO
	DT_CHR     uint8 = 2  // Character Device
	DT_DIR     uint8 = 4  // Directory
	DT_BLK     uint8 = 6  // Block Device
	DT_REG     uint8 = 8  // Regular File
	DT_LNK     uint8 = 10 // Symbolic Link
	DT_SOCK    uint8 = 12 // Local-Domain Socket
)

// Dirent matches POSIX struct dirent from KOS sys/dirent.h
type Dirent struct {
	Ino    int32      // File unique identifier (offset 0)
	Off    int32      // File offset (offset 4)
	Reclen uint16     // Record length (offset 8)
	Type   uint8      // File type (offset 10)
	Name   [257]byte  // Filename, null-terminated (offset 11)
}

func (d *Dirent) GetName() string { panic("kos: not on Dreamcast") }
func (d *Dirent) IsDir() bool     { panic("kos: not on Dreamcast") }

type DIR struct {
	_ [64]byte
}

func Opendir(path string) *DIR    { panic("kos: not on Dreamcast") }
func Readdir(dir *DIR) *Dirent    { panic("kos: not on Dreamcast") }
func Closedir(dir *DIR) int32     { panic("kos: not on Dreamcast") }

// Timespec matches C struct timespec (12 bytes on SH4)
// Note: We use [2]int32 for Sec because Go's int64 has 8-byte alignment,
// but C's time_t has 4-byte alignment on SH4.
type Timespec struct {
	SecLow  int32 // Lower 32 bits of tv_sec
	SecHigh int32 // Upper 32 bits of tv_sec
	Nsec    int32 // Nanoseconds
}

func (t *Timespec) GetSec() int64 { panic("kos: not on Dreamcast") }

// Stat matches C struct stat (76 bytes on SH4/Dreamcast)
type Stat struct {
	Dev     int16    // offset 0
	_pad0   int16    // offset 2 (padding for alignment)
	Ino     uint32   // offset 4
	Mode    uint32   // offset 8
	Nlink   int16    // offset 12
	Uid     uint16   // offset 14
	Gid     uint16   // offset 16
	Rdev    int16    // offset 18
	Size    int32    // offset 20 (off_t is 32-bit on SH4)
	Atim    Timespec // offset 24
	Mtim    Timespec // offset 36
	Ctim    Timespec // offset 48
	Blksize int32    // offset 60
	Blocks  int32    // offset 64
	_spare  [2]int32 // offset 68
}

func StatFile(path string, st *Stat) int32 { panic("kos: not on Dreamcast") }

const (
	O_RDONLY int32 = 0x0000
	O_WRONLY int32 = 0x0001
	O_RDWR   int32 = 0x0002
	O_APPEND int32 = 0x0008
	O_CREAT  int32 = 0x0200
	O_TRUNC  int32 = 0x0400
	O_EXCL   int32 = 0x0800
)

const (
	SEEK_SET int32 = 0
	SEEK_CUR int32 = 1
	SEEK_END int32 = 2
)

func Open(path string, mode int32) int32   { panic("kos: not on Dreamcast") }
func Close(fd int32) int32                 { panic("kos: not on Dreamcast") }
func Read(fd int32, buf []byte) int32      { panic("kos: not on Dreamcast") }
func Write(fd int32, buf []byte) int32     { panic("kos: not on Dreamcast") }
func Seek(fd int32, offset int64, whence int32) int64 { panic("kos: not on Dreamcast") }
func Total(fd int32) int64                 { panic("kos: not on Dreamcast") }
func Copy(src, dst string) int32           { panic("kos: not on Dreamcast") }
func Remove(path string) int32             { panic("kos: not on Dreamcast") }

func RomdiskMount(mountpoint string, img unsafe.Pointer, own bool) int32 { panic("kos: not on Dreamcast") }
func RomdiskUnmount(mountpoint string) int32                             { panic("kos: not on Dreamcast") }

