//go:build ignore

// test_memory.go - Memory allocation and GC tests
package main

import "unsafe"

var globalPtr *int
var globalSlice []*int

func forceGC() {
	for i := 0; i < 100; i++ {
		_ = make([]byte, 20000)
	}
}

func testBasicAllocation() {
	println("allocation:")
	passed := 0
	total := 0

	total++
	p := new(int)
	*p = 42
	if *p == 42 {
		passed++
		println("  PASS: new(int)")
	} else {
		println("  FAIL: new(int)")
	}

	total++
	type Point struct{ X, Y int }
	pt := new(Point)
	pt.X, pt.Y = 10, 20
	if pt.X == 10 && pt.Y == 20 {
		passed++
		println("  PASS: new(struct)")
	} else {
		println("  FAIL: new(struct)")
	}

	total++
	ptrs := make([]*int, 100)
	for i := 0; i < 100; i++ {
		ptrs[i] = new(int)
		*ptrs[i] = i * 2
	}
	correct := true
	for i := 0; i < 100; i++ {
		if *ptrs[i] != i*2 {
			correct = false
			break
		}
	}
	if correct {
		passed++
		println("  PASS: multiple allocations (100)")
	} else {
		println("  FAIL: multiple allocations")
	}

	total++
	type BigStruct struct {
		A, B, C, D, E int
		Name          string
		Data          [64]byte
	}
	big := new(BigStruct)
	big.A = 1
	big.Name = "test"
	big.Data[63] = 99
	if big.A == 1 && big.Name == "test" && big.Data[63] == 99 {
		passed++
		println("  PASS: large struct")
	} else {
		println("  FAIL: large struct")
	}

	total++
	type Empty struct{}
	e1 := new(Empty)
	e2 := new(Empty)
	if unsafe.Pointer(e1) == unsafe.Pointer(e2) {
		passed++
		println("  PASS: zero-size share address")
	} else {
		passed++
		println("  PASS: zero-size different addresses")
	}

	total++
	type Aligned64 struct {
		a, b uint64
	}
	ptrs2 := make([]*Aligned64, 10)
	for i := 0; i < 10; i++ {
		ptrs2[i] = new(Aligned64)
	}
	allAligned := true
	for i := 0; i < 10; i++ {
		addr := uintptr(unsafe.Pointer(ptrs2[i]))
		if addr%8 != 0 {
			allAligned = false
		}
	}
	if allAligned {
		passed++
		println("  PASS: 64-bit alignment")
	} else {
		println("  FAIL: 64-bit alignment")
	}

	println("  result:", passed, "/", total)
}

func testSlices() {
	println("slices:")
	passed := 0
	total := 0

	total++
	s := make([]int, 5, 10)
	if len(s) == 5 && cap(s) == 10 {
		passed++
		println("  PASS: make with len/cap")
	} else {
		println("  FAIL: make")
	}

	total++
	s2 := []int{1, 2, 3, 4, 5}
	if len(s2) == 5 && s2[0] == 1 && s2[4] == 5 {
		passed++
		println("  PASS: slice literal")
	} else {
		println("  FAIL: slice literal")
	}

	total++
	s3 := []int{1, 2}
	s3 = append(s3, 3, 4, 5)
	if len(s3) == 5 && s3[4] == 5 {
		passed++
		println("  PASS: append")
	} else {
		println("  FAIL: append")
	}

	total++
	s4 := make([]int, 0, 2)
	for i := 0; i < 100; i++ {
		s4 = append(s4, i)
	}
	if len(s4) == 100 && s4[99] == 99 {
		passed++
		println("  PASS: append with growth")
	} else {
		println("  FAIL: append with growth")
	}

	total++
	full := []int{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}
	part := full[2:7]
	if len(part) == 5 && part[0] == 2 && part[4] == 6 {
		passed++
		println("  PASS: slice of slice")
	} else {
		println("  FAIL: slice of slice")
	}

	total++
	src := []int{10, 20, 30, 40, 50}
	dst := make([]int, 5)
	n := copy(dst, src)
	if n == 5 && dst[0] == 10 && dst[4] == 50 {
		passed++
		println("  PASS: copy")
	} else {
		println("  FAIL: copy")
	}

	total++
	data := []int{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
	copy(data[2:], data[0:5])
	if data[2] == 1 && data[4] == 3 && data[6] == 5 {
		passed++
		println("  PASS: overlapping copy")
	} else {
		println("  FAIL: overlapping copy")
	}

	total++
	var nilSlice []int
	if nilSlice == nil && len(nilSlice) == 0 {
		passed++
		println("  PASS: nil slice")
	} else {
		println("  FAIL: nil slice")
	}

	total++
	var ns []int
	ns = append(ns, 100)
	if len(ns) == 1 && ns[0] == 100 {
		passed++
		println("  PASS: append to nil")
	} else {
		println("  FAIL: append to nil")
	}

	total++
	sum := 0
	for _, v := range s2 {
		sum += v
	}
	if sum == 15 {
		passed++
		println("  PASS: range")
	} else {
		println("  FAIL: range")
	}

	println("  result:", passed, "/", total)
}

func testStrings() {
	println("strings:")
	passed := 0
	total := 0

	total++
	s := "Hello, World!"
	if len(s) == 13 {
		passed++
		println("  PASS: string literal")
	} else {
		println("  FAIL: string literal")
	}

	total++
	a := "Hello"
	b := "World"
	c := a + ", " + b + "!"
	if c == "Hello, World!" {
		passed++
		println("  PASS: concatenation")
	} else {
		println("  FAIL: concatenation")
	}

	total++
	s = "ABCDE"
	if s[0] == 'A' && s[4] == 'E' {
		passed++
		println("  PASS: indexing")
	} else {
		println("  FAIL: indexing")
	}

	total++
	s = "Hello, World!"
	sub := s[7:12]
	if sub == "World" {
		passed++
		println("  PASS: slicing")
	} else {
		println("  FAIL: slicing")
	}

	total++
	if "apple" < "banana" && "zebra" > "apple" {
		passed++
		println("  PASS: comparison")
	} else {
		println("  FAIL: comparison")
	}

	total++
	s = "Hello"
	bs := []byte(s)
	if len(bs) == 5 && bs[0] == 'H' && bs[4] == 'o' {
		passed++
		println("  PASS: to []byte")
	} else {
		println("  FAIL: to []byte")
	}

	total++
	bs = []byte{'W', 'o', 'r', 'l', 'd'}
	s = string(bs)
	if s == "World" {
		passed++
		println("  PASS: from []byte")
	} else {
		println("  FAIL: from []byte")
	}

	total++
	s = "Hello"
	bs = []byte(s)
	bs[0] = 'J'
	if s == "Hello" && string(bs) == "Jello" {
		passed++
		println("  PASS: immutability")
	} else {
		println("  FAIL: immutability")
	}

	total++
	s = "ABC"
	runes := []rune(s)
	if len(runes) == 3 && runes[0] == 'A' {
		passed++
		println("  PASS: to []rune")
	} else {
		println("  FAIL: to []rune")
	}

	total++
	r := 'X'
	s = string(r)
	if s == "X" {
		passed++
		println("  PASS: rune to string")
	} else {
		println("  FAIL: rune to string")
	}

	println("  result:", passed, "/", total)
}

func testGC() {
	println("gc:")
	passed := 0
	total := 0

	total++
	p := new(int)
	*p = 12345
	forceGC()
	if *p == 12345 {
		passed++
		println("  PASS: pointer survives")
	} else {
		println("  FAIL: pointer survival")
	}

	total++
	type Node struct {
		value int
		next  *Node
	}
	head := &Node{value: 0}
	current := head
	for i := 1; i <= 50; i++ {
		current.next = &Node{value: i}
		current = current.next
	}
	forceGC()
	forceGC()
	current = head
	correct := true
	for i := 0; i <= 50; i++ {
		if current == nil || current.value != i {
			correct = false
			break
		}
		current = current.next
	}
	if correct {
		passed++
		println("  PASS: linked list survives")
	} else {
		println("  FAIL: linked list")
	}

	total++
	type CNode struct {
		value int
		next  *CNode
		prev  *CNode
	}
	cn1 := &CNode{value: 1}
	cn2 := &CNode{value: 2}
	cn3 := &CNode{value: 3}
	cn1.next, cn1.prev = cn2, cn3
	cn2.next, cn2.prev = cn3, cn1
	cn3.next, cn3.prev = cn1, cn2
	forceGC()
	if cn1.next == cn2 && cn2.next == cn3 && cn3.next == cn1 {
		passed++
		println("  PASS: cyclic references")
	} else {
		println("  FAIL: cyclic references")
	}

	total++
	live := make([]*int, 0, 250)
	for i := 0; i < 500; i++ {
		p := new(int)
		*p = i
		if i%2 == 0 {
			live = append(live, p)
		}
	}
	forceGC()
	correct = true
	for i := 0; i < len(live); i++ {
		if *live[i] != i*2 {
			correct = false
			break
		}
	}
	if correct {
		passed++
		println("  PASS: mixed live/dead")
	} else {
		println("  FAIL: mixed live/dead")
	}

	total++
	v := 999
	globalPtr = &v
	forceGC()
	if *globalPtr == 999 {
		passed++
		println("  PASS: global pointer root")
	} else {
		println("  FAIL: global pointer root")
	}

	total++
	globalSlice = make([]*int, 0, 50)
	for i := 0; i < 50; i++ {
		v := i * 3
		globalSlice = append(globalSlice, &v)
	}
	forceGC()
	correct = true
	for i := 0; i < 50; i++ {
		if *globalSlice[i] != i*3 {
			correct = false
			break
		}
	}
	if correct {
		passed++
		println("  PASS: global slice root")
	} else {
		println("  FAIL: global slice root")
	}

	total++
	sp := make([]*int, 50)
	for i := 0; i < 50; i++ {
		v := i * 10
		sp[i] = &v
	}
	forceGC()
	correct = true
	for i := 0; i < 50; i++ {
		if *sp[i] != i*10 {
			correct = false
			break
		}
	}
	if correct {
		passed++
		println("  PASS: slice of pointers")
	} else {
		println("  FAIL: slice of pointers")
	}

	total++
	type DeepNode struct {
		child *DeepNode
		value int
	}
	root := &DeepNode{value: 0}
	curr := root
	for i := 1; i < 100; i++ {
		curr.child = &DeepNode{value: i}
		curr = curr.child
	}
	forceGC()
	curr = root
	correct = true
	for i := 0; i < 100; i++ {
		if curr.value != i {
			correct = false
			break
		}
		curr = curr.child
	}
	if correct {
		passed++
		println("  PASS: deep nesting (100)")
	} else {
		println("  FAIL: deep nesting")
	}

	println("  result:", passed, "/", total)
}

func testLargeAllocation() {
	println("large alloc:")
	passed := 0
	total := 0

	total++
	s1 := make([]byte, 1024)
	s1[0] = 0xAA
	s1[1023] = 0xBB
	if s1[0] == 0xAA && s1[1023] == 0xBB {
		passed++
		println("  PASS: 1KB")
	} else {
		println("  FAIL: 1KB")
	}

	total++
	s2 := make([]byte, 10*1024)
	s2[0] = 0xCC
	s2[len(s2)-1] = 0xDD
	if s2[0] == 0xCC && s2[len(s2)-1] == 0xDD {
		passed++
		println("  PASS: 10KB")
	} else {
		println("  FAIL: 10KB")
	}

	total++
	type BigData struct {
		ID   int
		Data [64]byte
	}
	bd := make([]BigData, 50)
	for i := range bd {
		bd[i].ID = i
		bd[i].Data[0] = byte(i)
	}
	correct := true
	for i := range bd {
		if bd[i].ID != i || bd[i].Data[0] != byte(i) {
			correct = false
			break
		}
	}
	if correct {
		passed++
		println("  PASS: struct slice (50)")
	} else {
		println("  FAIL: struct slice")
	}

	total++
	large := make([]byte, 32*1024)
	large[0] = 0xEE
	large[len(large)-1] = 0xFF
	if large[0] == 0xEE && large[len(large)-1] == 0xFF {
		passed++
		println("  PASS: 32KB")
	} else {
		println("  FAIL: 32KB")
	}

	println("  result:", passed, "/", total)
}

func testMemoryOps() {
	println("memops:")
	passed := 0
	total := 0

	total++
	sizes := []int{1, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128, 256, 512}
	allOK := true
	for _, size := range sizes {
		src := make([]byte, size)
		dst := make([]byte, size)
		for i := 0; i < size; i++ {
			src[i] = byte((i * 7) & 0xFF)
		}
		n := copy(dst, src)
		if n != size {
			allOK = false
			break
		}
		for i := 0; i < size; i++ {
			if dst[i] != src[i] {
				allOK = false
				break
			}
		}
	}
	if allOK {
		passed++
		println("  PASS: copy various sizes")
	} else {
		println("  FAIL: copy various sizes")
	}

	total++
	allZero := true
	for _, size := range []int{16, 64, 256, 1024} {
		buf := make([]byte, size)
		for i := 0; i < size; i++ {
			if buf[i] != 0 {
				allZero = false
				break
			}
		}
	}
	if allZero {
		passed++
		println("  PASS: zero initialization")
	} else {
		println("  FAIL: zero initialization")
	}

	total++
	buf := []byte("0123456789")
	copy(buf[2:7], buf[0:5])
	if string(buf) == "0101234789" {
		passed++
		println("  PASS: forward overlap")
	} else {
		println("  FAIL: forward overlap")
	}

	total++
	buf = []byte("0123456789")
	copy(buf[0:5], buf[3:8])
	if string(buf) == "3456756789" {
		passed++
		println("  PASS: backward overlap")
	} else {
		println("  FAIL: backward overlap")
	}

	println("  result:", passed, "/", total)
}

func main() {
	println("test_memory")
	println("")

	testBasicAllocation()
	testSlices()
	testStrings()
	testGC()
	testLargeAllocation()
	testMemoryOps()

	println("")
	println("done")
}
