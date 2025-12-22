//go:build gccgo

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

func (d *Dirent) GetName() string {
	for i, b := range d.Name {
		if b == 0 {
			return string(d.Name[:i])
		}
	}
	return string(d.Name[:])
}

func (d *Dirent) IsDir() bool {
	return d.Type == DT_DIR
}

type DIR struct {
	_ [64]byte
}

//extern opendir
func opendir(name *byte) uintptr

func Opendir(path string) *DIR {
	cpath := make([]byte, len(path)+1)
	copy(cpath, path)
	ptr := opendir(&cpath[0])
	if ptr == 0 {
		return nil
	}
	return (*DIR)(unsafe.Pointer(ptr))
}

//extern readdir
func readdir(dirp uintptr) uintptr

func Readdir(dir *DIR) *Dirent {
	if dir == nil {
		return nil
	}
	ptr := readdir(uintptr(unsafe.Pointer(dir)))
	if ptr == 0 {
		return nil
	}
	return (*Dirent)(unsafe.Pointer(ptr))
}

//extern closedir
func closedir(dirp uintptr) int32

func Closedir(dir *DIR) int32 {
	if dir == nil {
		return -1
	}
	return closedir(uintptr(unsafe.Pointer(dir)))
}

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

//extern stat
func statfn(path *byte, buf *Stat) int32

func StatFile(path string, st *Stat) int32 {
	cpath := make([]byte, len(path)+1)
	copy(cpath, path)
	return statfn(&cpath[0], st)
}

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

//extern fs_open
func fsOpen(path *byte, mode int32) int32

func Open(path string, mode int32) int32 {
	cpath := make([]byte, len(path)+1)
	copy(cpath, path)
	return fsOpen(&cpath[0], mode)
}

//extern fs_close
func fsClose(fd int32) int32

func Close(fd int32) int32 {
	return fsClose(fd)
}

//extern fs_read
func fsRead(fd int32, buf uintptr, count int32) int32

func Read(fd int32, buf []byte) int32 {
	if len(buf) == 0 {
		return 0
	}
	return fsRead(fd, uintptr(unsafe.Pointer(&buf[0])), int32(len(buf)))
}

//extern fs_write
func fsWrite(fd int32, buf uintptr, count int32) int32

func Write(fd int32, buf []byte) int32 {
	if len(buf) == 0 {
		return 0
	}
	return fsWrite(fd, uintptr(unsafe.Pointer(&buf[0])), int32(len(buf)))
}

//extern fs_seek
func fsSeek(fd int32, offset int64, whence int32) int64

func Seek(fd int32, offset int64, whence int32) int64 {
	return fsSeek(fd, offset, whence)
}

//extern fs_total
func fsTotal(fd int32) int64

func Total(fd int32) int64 {
	return fsTotal(fd)
}

//extern fs_copy
func fsCopy(src *byte, dst *byte) int32

func Copy(src, dst string) int32 {
	csrc := make([]byte, len(src)+1)
	copy(csrc, src)
	cdst := make([]byte, len(dst)+1)
	copy(cdst, dst)
	return fsCopy(&csrc[0], &cdst[0])
}

//extern remove
func removefn(path *byte) int32

func Remove(path string) int32 {
	cpath := make([]byte, len(path)+1)
	copy(cpath, path)
	return removefn(&cpath[0])
}

//extern fs_romdisk_mount
func fsRomdiskMount(mountpoint *byte, img uintptr, own int32) int32

func RomdiskMount(mountpoint string, img unsafe.Pointer, own bool) int32 {
	cmp := make([]byte, len(mountpoint)+1)
	copy(cmp, mountpoint)
	ownVal := int32(0)
	if own {
		ownVal = 1
	}
	return fsRomdiskMount(&cmp[0], uintptr(img), ownVal)
}

//extern fs_romdisk_unmount
func fsRomdiskUnmount(mountpoint *byte) int32

func RomdiskUnmount(mountpoint string) int32 {
	cmp := make([]byte, len(mountpoint)+1)
	copy(cmp, mountpoint)
	return fsRomdiskUnmount(&cmp[0])
}
