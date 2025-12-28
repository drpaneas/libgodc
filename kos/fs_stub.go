//go:build !gccgo

package kos

import "unsafe"

const (
	DT_UNKNOWN uint8 = 0
	DT_FIFO    uint8 = 1
	DT_CHR     uint8 = 2
	DT_DIR     uint8 = 4
	DT_BLK     uint8 = 6
	DT_REG     uint8 = 8
	DT_LNK     uint8 = 10
	DT_SOCK    uint8 = 12
)

type Dirent struct {
	Ino    uint32
	Reclen uint16
	Type   uint8
	_      uint8
	Name   [256]byte
}

func (d *Dirent) GetName() string { panic("kos: not on Dreamcast") }
func (d *Dirent) IsDir() bool     { panic("kos: not on Dreamcast") }

type DIR struct {
	_ [64]byte
}

func Opendir(path string) *DIR    { panic("kos: not on Dreamcast") }
func Readdir(dir *DIR) *Dirent    { panic("kos: not on Dreamcast") }
func Closedir(dir *DIR) int32     { panic("kos: not on Dreamcast") }

type Stat struct {
	Dev     uint32
	Ino     uint32
	Mode    uint32
	Nlink   uint32
	Uid     uint32
	Gid     uint32
	Rdev    uint32
	Size    int64
	Atime   int64
	Mtime   int64
	Ctime   int64
	Blksize int32
	Blocks  int32
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

