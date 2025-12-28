//go:build gccgo

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

// Dirent matches POSIX struct dirent from KOS sys/dirent.h:
//
//	struct dirent {
//	    int      d_ino;    // offset 0, 4 bytes
//	    off_t    d_off;    // offset 4, 4 bytes
//	    uint16_t d_reclen; // offset 8, 2 bytes
//	    uint8_t  d_type;   // offset 10, 1 byte
//	    char     d_name[]; // offset 11, flexible array (NAME_MAX+1 bytes in DIR)
//	};
//
// Note: d_name is a flexible array member in C, but we use fixed 257 bytes
// to match the storage allocated in DIR structure.
type Dirent struct {
	Ino    int32      // File unique identifier (offset 0)
	Off    int32      // File offset (offset 4)
	Reclen uint16     // Record length (offset 8)
	Type   uint8      // File type (offset 10)
	Name   [257]byte  // Filename, null-terminated (offset 11)
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

// Timespec matches C struct timespec (12 bytes on SH4):
//
//	struct timespec {
//	    time_t tv_sec;   // 8 bytes (time_t is 64-bit, but 4-byte aligned!)
//	    long tv_nsec;    // 4 bytes
//	}
//
// Note: We use [2]int32 for Sec because Go's int64 has 8-byte alignment,
// but C's time_t has 4-byte alignment on SH4.
type Timespec struct {
	SecLow  int32 // Lower 32 bits of tv_sec
	SecHigh int32 // Upper 32 bits of tv_sec
	Nsec    int32 // Nanoseconds
}

// GetSec returns the seconds as int64
func (t *Timespec) GetSec() int64 {
	return int64(t.SecHigh)<<32 | int64(uint32(t.SecLow))
}

// Stat matches C struct stat (76 bytes on SH4/Dreamcast):
//
//	struct stat {
//	    dev_t st_dev;        // offset 0, 2 bytes
//	    // 2 bytes padding
//	    ino_t st_ino;        // offset 4, 4 bytes
//	    mode_t st_mode;      // offset 8, 4 bytes
//	    nlink_t st_nlink;    // offset 12, 2 bytes
//	    uid_t st_uid;        // offset 14, 2 bytes
//	    gid_t st_gid;        // offset 16, 2 bytes
//	    dev_t st_rdev;       // offset 18, 2 bytes
//	    off_t st_size;       // offset 20, 4 bytes
//	    struct timespec st_atim;  // offset 24, 12 bytes
//	    struct timespec st_mtim;  // offset 36, 12 bytes
//	    struct timespec st_ctim;  // offset 48, 12 bytes
//	    blksize_t st_blksize;     // offset 60, 4 bytes
//	    blkcnt_t st_blocks;       // offset 64, 4 bytes
//	    long st_spare4[2];        // offset 68, 8 bytes
//	}
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

// fs_seek returns off_t which is 32-bit on SH4/Dreamcast
// We use int32 for the C binding to avoid garbage in upper bits
//
//extern fs_seek
func fsSeek32(fd int32, offset int32, whence int32) int32

// Seek seeks to position in file. Returns new position or -1 on error.
// Note: offset is limited to 32-bit range on Dreamcast.
func Seek(fd int32, offset int64, whence int32) int64 {
	result := fsSeek32(fd, int32(offset), whence)
	return int64(result) // Sign-extend to 64-bit
}

// fs_total returns ssize_t which is 32-bit on SH4/Dreamcast
//
//extern fs_total
func fsTotal32(fd int32) int32

// Total returns the total size of the file. Returns -1 on error.
func Total(fd int32) int64 {
	result := fsTotal32(fd)
	return int64(result) // Sign-extend to 64-bit
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
