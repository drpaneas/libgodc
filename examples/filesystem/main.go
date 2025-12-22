// Filesystem browser - navigate directories with D-Pad
package main

import (
	"kos"
	"unsafe"
)

type FileEntry struct {
	Name  string
	IsDir bool
}

var (
	currentPath     string
	entries         []FileEntry
	selectorIndex   int
	scrollOffset    int
	previousButtons uint32
)

const MaxVisible = 18

func browseDirectory(path string) []FileEntry {
	result := make([]FileEntry, 0, 100)
	dir := kos.Opendir(path)
	if dir == nil {
		return result
	}
	defer kos.Closedir(dir)

	for len(result) < 100 {
		entry := kos.Readdir(dir)
		if entry == nil {
			break
		}
		name := entry.GetName()
		if name == "." || name == ".." {
			continue
		}
		result = append(result, FileEntry{Name: name, IsDir: entry.IsDir()})
	}
	return result
}

func getParentPath(path string) string {
	if path == "/" {
		return "/"
	}
	if len(path) > 1 && path[len(path)-1] == '/' {
		path = path[:len(path)-1]
	}
	for i := len(path) - 1; i >= 0; i-- {
		if path[i] == '/' {
			if i == 0 {
				return "/"
			}
			return path[:i]
		}
	}
	return "/"
}

func joinPath(base, name string) string {
	if base == "/" {
		return "/" + name
	}
	return base + "/" + name
}

func buttonPressed(current, changed, button uint32) bool {
	return (changed & current & button) != 0
}

func handleInput() bool {
	ctrl := kos.MapleEnumType(0, kos.MAPLE_FUNC_CONTROLLER)
	if ctrl == nil {
		return true
	}
	state := ctrl.ContState()
	if state == nil {
		return true
	}

	current := state.Buttons
	changed := current ^ previousButtons
	previousButtons = current

	if buttonPressed(current, changed, kos.CONT_START) {
		return false
	}
	if buttonPressed(current, changed, kos.CONT_DPAD_UP) && selectorIndex > 0 {
		selectorIndex--
		if selectorIndex < scrollOffset {
			scrollOffset = selectorIndex
		}
	}
	if buttonPressed(current, changed, kos.CONT_DPAD_DOWN) && selectorIndex < len(entries)-1 {
		selectorIndex++
		if selectorIndex >= scrollOffset+MaxVisible {
			scrollOffset = selectorIndex - MaxVisible + 1
		}
	}
	if buttonPressed(current, changed, kos.CONT_A) && len(entries) > 0 && entries[selectorIndex].IsDir {
		newPath := joinPath(currentPath, entries[selectorIndex].Name)
		newEntries := browseDirectory(newPath)
		if len(newEntries) > 0 || newPath == "/" {
			currentPath = newPath
			entries = newEntries
			selectorIndex, scrollOffset = 0, 0
		}
	}
	if buttonPressed(current, changed, kos.CONT_B) && currentPath != "/" {
		currentPath = getParentPath(currentPath)
		entries = browseDirectory(currentPath)
		selectorIndex, scrollOffset = 0, 0
	}
	return true
}

func drawText(x, y int, text string, opaque bool) {
	vram := kos.VramSOffset(y*640 + x)
	kos.BfontDrawStr(vram, 640, opaque, text)
}

func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	neg := n < 0
	if neg {
		n = -n
	}
	digits := make([]byte, 0, 10)
	for n > 0 {
		digits = append(digits, byte('0'+n%10))
		n /= 10
	}
	result := make([]byte, len(digits))
	for i := range digits {
		result[i] = digits[len(digits)-1-i]
	}
	if neg {
		return "-" + string(result)
	}
	return string(result)
}

func drawScreen() {
	vram := kos.VramS()
	for i := 0; i < 640*480; i++ {
		*(*uint16)(unsafe.Pointer(uintptr(vram) + uintptr(i*2))) = 0x0008
	}

	kos.BfontSetEncoding(kos.BFONT_CODE_ISO8859_1)
	kos.BfontSetForegroundColor(0xFFFFFFFF)

	drawText(kos.BFONT_THIN_WIDTH*2, kos.BFONT_HEIGHT/2, "Filesystem Browser - "+currentPath, true)
	drawText(0, kos.BFONT_HEIGHT+kos.BFONT_HEIGHT/2, "----------------------------------------", true)

	startY := kos.BFONT_HEIGHT * 3
	x := kos.BFONT_THIN_WIDTH * 3
	visibleEnd := scrollOffset + MaxVisible
	if visibleEnd > len(entries) {
		visibleEnd = len(entries)
	}

	for i := scrollOffset; i < visibleEnd; i++ {
		entry := entries[i]
		y := startY + (i-scrollOffset)*kos.BFONT_HEIGHT

		if i == selectorIndex {
			kos.BfontSetForegroundColor(0xFF00FF00)
			drawText(kos.BFONT_THIN_WIDTH, y, ">", true)
		}

		if entry.IsDir {
			kos.BfontSetForegroundColor(0xFFFFFF00)
			name := entry.Name
			if len(name) > 30 {
				name = name[:30]
			}
			for len(name) < 32 {
				name += " "
			}
			drawText(x, y, name+"<DIR>", true)
		} else {
			kos.BfontSetForegroundColor(0xFFFFFFFF)
			name := entry.Name
			if len(name) > 40 {
				name = name[:40]
			}
			drawText(x, y, name, true)
		}
	}

	kos.BfontSetForegroundColor(0xFF808080)
	if scrollOffset > 0 {
		drawText(640-kos.BFONT_THIN_WIDTH*4, startY, "^^^", true)
	}
	if visibleEnd < len(entries) {
		drawText(640-kos.BFONT_THIN_WIDTH*4, startY+(MaxVisible-1)*kos.BFONT_HEIGHT, "vvv", true)
	}

	kos.BfontSetForegroundColor(0xFF00FFFF)
	drawText(kos.BFONT_THIN_WIDTH*2, 480-kos.BFONT_HEIGHT*2,
		"UP/DOWN: Navigate  A: Enter  B: Back  START: Exit", true)

	kos.BfontSetForegroundColor(0xFFFFFFFF)
	if len(entries) > 0 {
		drawText(kos.BFONT_THIN_WIDTH*2, 480-kos.BFONT_HEIGHT,
			"Entry "+itoa(selectorIndex+1)+" of "+itoa(len(entries)), true)
	} else {
		drawText(kos.BFONT_THIN_WIDTH*2, 480-kos.BFONT_HEIGHT, "Empty directory", true)
	}
}

func main() {
	kos.VidSetMode(kos.DM_640x480, kos.PM_RGB555)

	currentPath = "/"
	entries = browseDirectory(currentPath)

	for {
		if !handleInput() {
			break
		}
		drawScreen()
		kos.TimerSpinSleep(16)
	}
}
