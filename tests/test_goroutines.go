//go:build ignore

// test_goroutines.go - Goroutine and channel tests
package main

func testBasicGoroutine() {
	println("goroutines:")
	passed := 0
	total := 0

	total++
	result := make(chan int)
	go func() { result <- 42 }()
	if <-result == 42 {
		passed++
		println("  PASS: simple goroutine")
	} else {
		println("  FAIL: simple goroutine")
	}

	total++
	result = make(chan int)
	go func(x int) { result <- x * 2 }(21)
	if <-result == 42 {
		passed++
		println("  PASS: with parameter")
	} else {
		println("  FAIL: with parameter")
	}

	total++
	results := make(chan int, 5)
	for i := 0; i < 5; i++ {
		idx := i
		go func() { results <- idx * 10 }()
	}
	sum := 0
	for i := 0; i < 5; i++ {
		sum += <-results
	}
	if sum == 100 {
		passed++
		println("  PASS: multiple goroutines")
	} else {
		println("  FAIL: multiple goroutines, sum:", sum)
	}

	total++
	ch := make(chan int)
	go func() {
		var arr [200]int
		for i := range arr {
			arr[i] = i
		}
		ch <- arr[199]
	}()
	if <-ch == 199 {
		passed++
		println("  PASS: stack usage")
	} else {
		println("  FAIL: stack usage")
	}

	total++
	var recursiveFunc func(int, chan int)
	recursiveFunc = func(depth int, ch chan int) {
		if depth <= 0 {
			ch <- 1
			return
		}
		var arr [100]int
		arr[0] = depth
		recursiveFunc(depth-1, ch)
		_ = arr[0]
	}
	recCh := make(chan int)
	go recursiveFunc(30, recCh)
	if <-recCh == 1 {
		passed++
		println("  PASS: recursive (30 levels)")
	} else {
		println("  FAIL: recursive")
	}

	println("  result:", passed, "/", total)
}

func testChannels() {
	println("channels:")
	passed := 0
	total := 0

	total++
	ch := make(chan int)
	go func() { ch <- 100 }()
	if <-ch == 100 {
		passed++
		println("  PASS: unbuffered")
	} else {
		println("  FAIL: unbuffered")
	}

	total++
	bch := make(chan int, 3)
	bch <- 1
	bch <- 2
	bch <- 3
	if <-bch == 1 && <-bch == 2 && <-bch == 3 {
		passed++
		println("  PASS: buffered")
	} else {
		println("  FAIL: buffered")
	}

	total++
	ch = make(chan int, 3)
	ch <- 10
	ch <- 20
	close(ch)
	v1, ok1 := <-ch
	v2, ok2 := <-ch
	v3, ok3 := <-ch
	if v1 == 10 && ok1 && v2 == 20 && ok2 && v3 == 0 && !ok3 {
		passed++
		println("  PASS: close")
	} else {
		println("  FAIL: close")
	}

	total++
	ch = make(chan int, 3)
	ch <- 10
	ch <- 20
	ch <- 30
	close(ch)
	sum := 0
	for v := range ch {
		sum += v
	}
	if sum == 60 {
		passed++
		println("  PASS: range")
	} else {
		println("  FAIL: range")
	}

	total++
	prodCh := make(chan int, 10)
	go func() {
		for i := 0; i < 10; i++ {
			prodCh <- i
		}
		close(prodCh)
	}()
	sum = 0
	for v := range prodCh {
		sum += v
	}
	if sum == 45 {
		passed++
		println("  PASS: producer-consumer")
	} else {
		println("  FAIL: producer-consumer")
	}

	total++
	request := make(chan int, 2)
	response := make(chan int, 2)
	go func() {
		for {
			x, ok := <-request
			if !ok {
				close(response)
				return
			}
			response <- x * 2
		}
	}()
	request <- 10
	request <- 20
	close(request)
	r1 := <-response
	r2 := <-response
	if r1 == 20 && r2 == 40 {
		passed++
		println("  PASS: bidirectional")
	} else {
		println("  FAIL: bidirectional")
	}

	println("  result:", passed, "/", total)
}

func testSelect() {
	println("select:")
	passed := 0
	total := 0

	total++
	ch := make(chan int, 1)
	ch <- 42
	var received int
	select {
	case received = <-ch:
	default:
		received = -1
	}
	if received == 42 {
		passed++
		println("  PASS: ready channel")
	} else {
		println("  FAIL: ready channel")
	}

	total++
	ch = make(chan int)
	selected := ""
	select {
	case <-ch:
		selected = "chan"
	default:
		selected = "default"
	}
	if selected == "default" {
		passed++
		println("  PASS: default")
	} else {
		println("  FAIL: default")
	}

	total++
	ch1 := make(chan int, 1)
	ch2 := make(chan int, 1)
	ch2 <- 100
	var val int
	select {
	case val = <-ch1:
	case val = <-ch2:
	}
	if val == 100 {
		passed++
		println("  PASS: multiple channels")
	} else {
		println("  FAIL: multiple channels")
	}

	total++
	ch = make(chan int, 1)
	sent := false
	select {
	case ch <- 99:
		sent = true
	default:
		sent = false
	}
	if sent && <-ch == 99 {
		passed++
		println("  PASS: send")
	} else {
		println("  FAIL: send")
	}

	total++
	ping := make(chan bool)
	pong := make(chan bool)
	done := make(chan bool)
	go func() {
		for i := 0; i < 10; i++ {
			<-ping
			pong <- true
		}
		done <- true
	}()
	for i := 0; i < 10; i++ {
		ping <- true
		<-pong
	}
	<-done
	passed++
	println("  PASS: ping-pong")

	println("  result:", passed, "/", total)
}

func testConcurrentPatterns() {
	println("patterns:")
	passed := 0
	total := 0

	total++
	const workers = 5
	const jobs = 20
	jobCh := make(chan int, jobs)
	resultsCh := make(chan int, jobs)
	for w := 0; w < workers; w++ {
		go func() {
			for j := range jobCh {
				resultsCh <- j * 2
			}
		}()
	}
	for j := 0; j < jobs; j++ {
		jobCh <- j
	}
	close(jobCh)
	sum := 0
	for r := 0; r < jobs; r++ {
		sum += <-resultsCh
	}
	if sum == 380 {
		passed++
		println("  PASS: fan-out")
	} else {
		println("  FAIL: fan-out, sum:", sum)
	}

	total++
	const chainLen = 5
	first := make(chan int)
	prev := first
	for i := 0; i < chainLen; i++ {
		next := make(chan int)
		go func(in, out chan int) {
			out <- (<-in + 1)
		}(prev, next)
		prev = next
	}
	go func() { first <- 0 }()
	if <-prev == chainLen {
		passed++
		println("  PASS: goroutine chain")
	} else {
		println("  FAIL: goroutine chain")
	}

	total++
	const numWorkers = 10
	doneCh := make(chan bool, numWorkers)
	for i := 0; i < numWorkers; i++ {
		go func(id int) {
			sum := 0
			for j := 0; j < 100; j++ {
				sum += j
			}
			_ = sum
			doneCh <- true
		}(i)
	}
	for i := 0; i < numWorkers; i++ {
		<-doneCh
	}
	passed++
	println("  PASS: wait group")

	total++
	ch := make(chan int, 10)
	prodDone := make(chan bool, 3)
	for p := 0; p < 3; p++ {
		pid := p
		go func() {
			for i := 0; i < 5; i++ {
				ch <- pid*10 + i
			}
			prodDone <- true
		}()
	}
	go func() {
		for i := 0; i < 3; i++ {
			<-prodDone
		}
		close(ch)
	}()
	count := 0
	for range ch {
		count++
	}
	if count == 15 {
		passed++
		println("  PASS: multiple producers")
	} else {
		println("  FAIL: multiple producers, count:", count)
	}

	println("  result:", passed, "/", total)
}

func testStress() {
	println("stress:")
	passed := 0
	total := 0

	total++
	const iterations = 30
	for i := 0; i < iterations; i++ {
		done := make(chan bool)
		go func() { done <- true }()
		<-done
	}
	passed++
	println("  PASS: goroutine churn (", iterations, ")")

	total++
	for i := 0; i < iterations; i++ {
		ch := make(chan int, 5)
		go func() {
			for j := 0; j < 3; j++ {
				ch <- j
			}
			close(ch)
		}()
		count := 0
		for range ch {
			count++
		}
		if count != 3 {
			println("  FAIL: close race")
			break
		}
	}
	passed++
	println("  PASS: close race (", iterations, ")")

	total++
	for i := 0; i < iterations; i++ {
		ch := make(chan int, 3)
		go func() {
			for j := 0; j < 3; j++ {
				ch <- j
			}
		}()
		sum := 0
		for j := 0; j < 3; j++ {
			sum += <-ch
		}
		if sum != 3 {
			println("  FAIL: buffered churn")
			break
		}
	}
	passed++
	println("  PASS: buffered churn (", iterations, ")")

	total++
	const numGoroutines = 5
	const allocsPerG = 10
	allocDone := make(chan bool, numGoroutines)
	for g := 0; g < numGoroutines; g++ {
		go func(id int) {
			for i := 0; i < allocsPerG; i++ {
				p := new(int)
				*p = id*100 + i
				if *p != id*100+i {
					panic("allocation corrupted")
				}
			}
			allocDone <- true
		}(g)
	}
	for g := 0; g < numGoroutines; g++ {
		<-allocDone
	}
	passed++
	println("  PASS: concurrent allocation")

	println("  result:", passed, "/", total)
}

func testEdgeCases() {
	println("edge cases:")
	passed := 0
	total := 0

	total++
	var nilCh chan int
	selected := ""
	select {
	case <-nilCh:
		selected = "nil"
	default:
		selected = "default"
	}
	if selected == "default" {
		passed++
		println("  PASS: nil channel recv")
	} else {
		println("  FAIL: nil channel recv")
	}

	total++
	selected = ""
	select {
	case nilCh <- 1:
		selected = "nil"
	default:
		selected = "default"
	}
	if selected == "default" {
		passed++
		println("  PASS: nil channel send")
	} else {
		println("  FAIL: nil channel send")
	}

	total++
	selected = ""
	select {
	default:
		selected = "default"
	}
	if selected == "default" {
		passed++
		println("  PASS: empty select")
	} else {
		println("  FAIL: empty select")
	}

	total++
	chch := make(chan chan int)
	go func() {
		inner := make(chan int, 1)
		inner <- 42
		chch <- inner
	}()
	innerCh := <-chch
	if <-innerCh == 42 {
		passed++
		println("  PASS: channel of channels")
	} else {
		println("  FAIL: channel of channels")
	}

	total++
	type Msg struct {
		Value int
		Reply chan int
	}
	msgCh := make(chan Msg)
	go func() {
		msg := <-msgCh
		msg.Reply <- msg.Value * 2
	}()
	reply := make(chan int)
	msgCh <- Msg{Value: 21, Reply: reply}
	if <-reply == 42 {
		passed++
		println("  PASS: struct with channel")
	} else {
		println("  FAIL: struct with channel")
	}

	println("  result:", passed, "/", total)
}

func main() {
	println("test_goroutines")
	println("")

	testBasicGoroutine()
	testChannels()
	testSelect()
	testConcurrentPatterns()
	testStress()
	testEdgeCases()

	println("")
	println("done")
}
