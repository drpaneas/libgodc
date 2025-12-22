//go:build ignore

// test_types.go - Type system tests: maps, interfaces, structs, unsafe
package main

import "unsafe"

type Stringer interface {
	String() string
}

type Counter interface {
	Count() int
	Increment()
}

type MyString struct {
	value string
}

func (m MyString) String() string { return m.value }

type MyCounter struct {
	count int
}

func (c *MyCounter) Count() int { return c.count }
func (c *MyCounter) Increment() { c.count++ }

func testMaps() {
	println("maps:")
	passed := 0
	total := 0

	total++
	m := make(map[string]int)
	m["one"] = 1
	m["two"] = 2
	if m["one"] == 1 && m["two"] == 2 && len(m) == 2 {
		passed++
		println("  PASS: basic operations")
	} else {
		println("  FAIL: basic operations")
	}

	total++
	m2 := map[string]int{"a": 1, "b": 2, "c": 3}
	if len(m2) == 3 && m2["b"] == 2 {
		passed++
		println("  PASS: map literal")
	} else {
		println("  FAIL: map literal")
	}

	total++
	v, ok := m["one"]
	_, ok2 := m["nonexistent"]
	if ok && v == 1 && !ok2 {
		passed++
		println("  PASS: lookup with ok")
	} else {
		println("  FAIL: lookup with ok")
	}

	total++
	delete(m2, "b")
	_, ok = m2["b"]
	if !ok && len(m2) == 2 {
		passed++
		println("  PASS: delete")
	} else {
		println("  FAIL: delete")
	}

	total++
	m3 := map[int]int{1: 10, 2: 20, 3: 30}
	sum := 0
	for _, v := range m3 {
		sum += v
	}
	if sum == 60 {
		passed++
		println("  PASS: iteration")
	} else {
		println("  FAIL: iteration, got sum:", sum)
	}

	total++
	large := make(map[int]int)
	for i := 0; i < 500; i++ {
		large[i] = i * 2
	}
	correct := true
	for i := 0; i < 500; i++ {
		if large[i] != i*2 {
			correct = false
			break
		}
	}
	if correct && len(large) == 500 {
		passed++
		println("  PASS: growth (500 entries)")
	} else {
		println("  FAIL: growth")
	}

	total++
	type Point struct{ x, y int }
	pointMap := map[Point]string{{0, 0}: "origin", {1, 0}: "right"}
	if pointMap[Point{0, 0}] == "origin" {
		passed++
		println("  PASS: struct keys")
	} else {
		println("  FAIL: struct keys")
	}

	total++
	var nilMap map[string]int
	_, ok = nilMap["key"]
	if len(nilMap) == 0 && !ok {
		passed++
		println("  PASS: nil map read")
	} else {
		println("  FAIL: nil map read")
	}

	total++
	func() {
		defer func() {
			if r := recover(); r != nil {
				println("  FAIL: delete from nil map panicked")
			}
		}()
		delete(nilMap, "key")
		passed++
		println("  PASS: delete from nil map")
	}()

	println("  result:", passed, "/", total)
}

func testInterfaces() {
	println("interfaces:")
	passed := 0
	total := 0

	total++
	var i interface{} = 42
	if v, ok := i.(int); ok && v == 42 {
		passed++
		println("  PASS: empty interface with int")
	} else {
		println("  FAIL: empty interface with int")
	}

	total++
	i = "hello"
	if v, ok := i.(string); ok && v == "hello" {
		passed++
		println("  PASS: empty interface with string")
	} else {
		println("  FAIL: empty interface with string")
	}

	total++
	var s Stringer = MyString{"test"}
	if s.String() == "test" {
		passed++
		println("  PASS: method interface")
	} else {
		println("  FAIL: method interface")
	}

	total++
	var c Counter = &MyCounter{count: 5}
	c.Increment()
	if c.Count() == 6 {
		passed++
		println("  PASS: pointer receiver")
	} else {
		println("  FAIL: pointer receiver")
	}

	total++
	checkType := func(x interface{}) string {
		switch x.(type) {
		case int:
			return "int"
		case string:
			return "string"
		case bool:
			return "bool"
		default:
			return "unknown"
		}
	}
	if checkType(42) == "int" && checkType("hi") == "string" && checkType(true) == "bool" {
		passed++
		println("  PASS: type switch")
	} else {
		println("  FAIL: type switch")
	}

	total++
	var a, b interface{} = 42, 42
	var c2 interface{} = 43
	if a == b && a != c2 {
		passed++
		println("  PASS: comparison")
	} else {
		println("  FAIL: comparison")
	}

	total++
	items := []interface{}{1, "two", 3.0, true}
	if len(items) == 4 {
		if v, ok := items[0].(int); ok && v == 1 {
			passed++
			println("  PASS: interface slice")
		} else {
			println("  FAIL: interface slice element")
		}
	} else {
		println("  FAIL: interface slice length")
	}

	total++
	getNil := func() interface{} { return nil }
	i = getNil()
	if i == nil {
		passed++
		println("  PASS: nil interface")
	} else {
		println("  FAIL: nil interface")
	}

	println("  result:", passed, "/", total)
}

func testStructs() {
	println("structs:")
	passed := 0
	total := 0

	total++
	type Point struct{ X, Y int }
	p := Point{10, 20}
	if p.X == 10 && p.Y == 20 {
		passed++
		println("  PASS: basic struct")
	} else {
		println("  FAIL: basic struct")
	}

	total++
	p2 := Point{Y: 30, X: 40}
	if p2.X == 40 && p2.Y == 30 {
		passed++
		println("  PASS: named field init")
	} else {
		println("  FAIL: named field init")
	}

	total++
	pp := &Point{5, 6}
	pp.X = 50
	if pp.X == 50 && pp.Y == 6 {
		passed++
		println("  PASS: struct pointer")
	} else {
		println("  FAIL: struct pointer")
	}

	total++
	type Named struct {
		Point
		Name string
	}
	n := Named{Point{1, 2}, "test"}
	if n.X == 1 && n.Y == 2 && n.Name == "test" {
		passed++
		println("  PASS: embedded struct")
	} else {
		println("  FAIL: embedded struct")
	}

	total++
	anon := struct {
		A int
		B string
	}{10, "hello"}
	if anon.A == 10 && anon.B == "hello" {
		passed++
		println("  PASS: anonymous struct")
	} else {
		println("  FAIL: anonymous struct")
	}

	total++
	s1 := Point{1, 2}
	s2 := Point{1, 2}
	s3 := Point{1, 3}
	if s1 == s2 && s1 != s3 {
		passed++
		println("  PASS: struct comparison")
	} else {
		println("  FAIL: struct comparison")
	}

	println("  result:", passed, "/", total)
}

func testUnsafe() {
	println("unsafe:")
	passed := 0
	total := 0

	total++
	if unsafe.Sizeof(int32(0)) == 4 && unsafe.Sizeof(int64(0)) == 8 {
		passed++
		println("  PASS: Sizeof")
	} else {
		println("  FAIL: Sizeof")
	}

	total++
	var ptr *int
	if unsafe.Sizeof(ptr) == 4 {
		passed++
		println("  PASS: pointer size (32-bit)")
	} else {
		println("  FAIL: pointer size:", unsafe.Sizeof(ptr))
	}

	total++
	if unsafe.Alignof(int32(0)) == 4 && unsafe.Alignof(byte(0)) == 1 {
		passed++
		println("  PASS: Alignof")
	} else {
		println("  FAIL: Alignof")
	}

	total++
	type S struct {
		a byte
		b int32
	}
	var s S
	if unsafe.Offsetof(s.a) == 0 {
		passed++
		println("  PASS: Offsetof first field")
	} else {
		println("  FAIL: Offsetof")
	}

	total++
	x := 42
	ptr2 := unsafe.Pointer(&x)
	xPtr := (*int)(ptr2)
	if *xPtr == 42 {
		passed++
		println("  PASS: pointer conversion")
	} else {
		println("  FAIL: pointer conversion")
	}

	total++
	arr := [5]int{10, 20, 30, 40, 50}
	ptr3 := unsafe.Pointer(&arr[0])
	ptr4 := unsafe.Pointer(uintptr(ptr3) + unsafe.Sizeof(arr[0]))
	if *(*int)(ptr4) == 20 {
		passed++
		println("  PASS: pointer arithmetic")
	} else {
		println("  FAIL: pointer arithmetic")
	}

	total++
	bytes := [4]byte{0x78, 0x56, 0x34, 0x12}
	val := *(*uint32)(unsafe.Pointer(&bytes[0]))
	if val == 0x12345678 {
		passed++
		println("  PASS: type punning (little-endian)")
	} else {
		println("  FAIL: type punning, got:", val)
	}

	println("  result:", passed, "/", total)
}

func testFunctions() {
	println("functions:")
	passed := 0
	total := 0

	total++
	add := func(a, b int) int { return a + b }
	if add(2, 3) == 5 {
		passed++
		println("  PASS: function variable")
	} else {
		println("  FAIL: function variable")
	}

	total++
	counter := func() func() int {
		n := 0
		return func() int {
			n++
			return n
		}
	}()
	if counter() == 1 && counter() == 2 && counter() == 3 {
		passed++
		println("  PASS: closure")
	} else {
		println("  FAIL: closure")
	}

	total++
	divmod := func(a, b int) (int, int) { return a / b, a % b }
	q, r := divmod(17, 5)
	if q == 3 && r == 2 {
		passed++
		println("  PASS: multiple return values")
	} else {
		println("  FAIL: multiple return values")
	}

	total++
	namedRet := func() (x, y int) {
		x, y = 10, 20
		return
	}
	a, b := namedRet()
	if a == 10 && b == 20 {
		passed++
		println("  PASS: named return values")
	} else {
		println("  FAIL: named return values")
	}

	total++
	sum := func(nums ...int) int {
		total := 0
		for _, n := range nums {
			total += n
		}
		return total
	}
	if sum(1, 2, 3, 4, 5) == 15 {
		passed++
		println("  PASS: variadic function")
	} else {
		println("  FAIL: variadic function")
	}

	total++
	ms := MyString{"method"}
	fn := ms.String
	if fn() == "method" {
		passed++
		println("  PASS: method value")
	} else {
		println("  FAIL: method value")
	}

	println("  result:", passed, "/", total)
}

func main() {
	println("test_types")
	println("")

	testMaps()
	testInterfaces()
	testStructs()
	testUnsafe()
	testFunctions()

	println("")
	println("done")
}
