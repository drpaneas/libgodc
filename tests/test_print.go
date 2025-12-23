//go:build ignore

// test_print.go - Tests for println/print functions
package main

type Point struct {
	x, y int
}

type Person struct {
	name string
	age  int
}

func testPrintln() {
	println("println:")
	println("  empty:")
	println()
	println("  string: hello")
	println("  int:", 42)
	println("  negative:", -123)
	println("  true:", true)
	println("  false:", false)
	println("  multiple:", "a", 1, true, "b")
	println("  large:", 1234567890)
	x := 42
	println("  pointer:", &x)
	var nilPtr *int
	println("  nil pointer:", nilPtr)
	println("  PASS: println")
}

func testPrint() {
	println("print:")
	print("  no newline:")
	print(" ")
	print("hello")
	print("\n")
	print("  multiple: ")
	print("a")
	print(" ")
	print(1)
	print(" ")
	print(true)
	print("\n")
	println("  PASS: print")
}

func testNumericPrint() {
	println("numeric:")
	var i8 int8 = 127
	var i16 int16 = 32767
	var i32 int32 = 2147483647
	var i64 int64 = 9223372036854775807
	println("  int8:", i8)
	println("  int16:", i16)
	println("  int32:", i32)
	println("  int64:", i64)
	var u8 uint8 = 255
	var u16 uint16 = 65535
	var u32 uint32 = 4294967295
	println("  uint8:", u8)
	println("  uint16:", u16)
	println("  uint32:", u32)
	println("  hex:", 0xDEADBEEF)
	println("  PASS: numeric print")
}

func testSpecialValues() {
	println("special:")
	println("  zero:", 0)
	println("  negative:", -1)
	println("  max int32:", 2147483647)
	println("  min int32:", -2147483648)
	println("  true:", true)
	println("  false:", false)
	println("  empty:", "")
	println("  spaces:", "  hello  ")
	println("  escapes:", "tab:\there newline:\nend")
	println("  PASS: special values")
}

func testSlicePrint() {
	println("slice/array:")
	b := []byte("hello")
	println("  bytes:", string(b))
	s := []int{1, 2, 3}
	print("  ints: ")
	for i, v := range s {
		if i > 0 {
			print(", ")
		}
		print(v)
	}
	println()
	arr := [3]int{4, 5, 6}
	print("  array: ")
	for i, v := range arr {
		if i > 0 {
			print(", ")
		}
		print(v)
	}
	println()
	println("  PASS: slice/array print")
}

func testInterfacePrint() {
	println("interface:")
	var i interface{}
	i = 42
	println("  int:", i)
	i = "hello"
	println("  string:", i)
	i = true
	println("  bool:", i)
	i = nil
	println("  nil:", i)
	println("  PASS: interface print")
}

func testStructPrint() {
	println("struct:")
	p := Point{10, 20}
	println("  Point: x=", p.x, "y=", p.y)
	person := Person{"Alice", 30}
	println("  Person:", person.name, person.age)
	println("  &Point:", &p)
	println("  PASS: struct print")
}

func testStressPrint() {
	println("stress:")
	for i := 0; i < 10; i++ {
		println("  iter", i)
	}
	println("  long:", "this is a relatively long string to test that the print buffer handles it correctly")
	println("  PASS: stress print")
}

func main() {
	println("test_print")
	println("")

	testPrintln()
	testPrint()
	testNumericPrint()
	testSpecialValues()
	testSlicePrint()
	testInterfacePrint()
	testStructPrint()
	testStressPrint()

	println("")
	println("done (visual inspection)")
}
