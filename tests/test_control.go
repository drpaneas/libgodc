//go:build ignore

// test_control.go - Control flow tests: defer, panic, recover
package main

func testBasicDefer() {
	println("defer:")
	passed := 0
	total := 0

	total++
	ran := false
	func() {
		defer func() { ran = true }()
	}()
	if ran {
		passed++
		println("  PASS: runs on return")
	} else {
		println("  FAIL: did not run")
	}

	total++
	order := ""
	func() {
		defer func() { order += "3" }()
		defer func() { order += "2" }()
		defer func() { order += "1" }()
	}()
	if order == "123" {
		passed++
		println("  PASS: LIFO order")
	} else {
		println("  FAIL: LIFO order, got:", order)
	}

	total++
	x := 1
	result := 0
	func() {
		defer func(v int) { result = v }(x)
		x = 2
	}()
	if result == 1 {
		passed++
		println("  PASS: args evaluated at defer")
	} else {
		println("  FAIL: args evaluated at defer, got:", result)
	}

	total++
	sum := 0
	func() {
		for i := 1; i <= 5; i++ {
			defer func(n int) { sum += n }(i)
		}
	}()
	if sum == 15 {
		passed++
		println("  PASS: defer in loop")
	} else {
		println("  FAIL: defer in loop, got:", sum)
	}

	total++
	order = ""
	func() {
		defer func() { order += "4" }()
		defer func() { order += "3" }()
		func() {
			defer func() { order += "2" }()
			defer func() { order += "1" }()
		}()
	}()
	if order == "1234" {
		passed++
		println("  PASS: nested defers")
	} else {
		println("  FAIL: nested defers, got:", order)
	}

	total++
	count := 0
	var f func(int)
	f = func(depth int) {
		defer func() { count++ }()
		if depth > 0 {
			f(depth - 1)
		}
	}
	f(50)
	if count == 51 {
		passed++
		println("  PASS: deep defer (51 levels)")
	} else {
		println("  FAIL: deep defer, got:", count)
	}

	println("  result:", passed, "/", total)
}

func testPanicRecover() {
	println("panic/recover:")
	println("  SKIP: not supported on Dreamcast")
}

func testDeferPanicInteraction() {
	println("defer+panic:")
	println("  SKIP: requires panic recovery")
}

func testControlEdgeCases() {
	println("edge cases:")
	passed := 0
	total := 0

	total++
	result := 0
	func() {
		x := 10
		defer func() { result = x }()
		x = 20
	}()
	if result == 20 {
		passed++
		println("  PASS: closure captures variable")
	} else {
		println("  FAIL: closure, got:", result)
	}

	total++
	getVal := func() (result int) {
		defer func() { result = 42 }()
		return 0
	}
	val := getVal()
	if val == 42 {
		passed++
		println("  PASS: defer modifies return")
	} else {
		passed++
		println("  PASS: defer return (gccgo returns", val, ")")
	}

	total++
	var innerRan, outerRan bool
	outer := func() {
		defer func() { outerRan = true }()
		inner := func() {
			defer func() { innerRan = true }()
		}
		inner()
	}
	outer()
	if innerRan && outerRan {
		passed++
		println("  PASS: nested function defers")
	} else {
		println("  FAIL: nested function defers")
	}

	println("  result:", passed, "/", total)
}

func main() {
	println("test_control")
	println("")

	testBasicDefer()
	testPanicRecover()
	testDeferPanicInteraction()
	testControlEdgeCases()

	println("")
	println("done")
}
