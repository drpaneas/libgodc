# libgodc Test Suite

Hardware tests for verifying libgodc runtime functionality on real Dreamcast hardware.

## Test Files

| Test | Description |
|------|-------------|
| `test_print` | Basic I/O: println, print, numeric types |
| `test_memory` | Memory: allocation, slices, strings, GC |
| `test_goroutines` | Concurrency: goroutines, channels, select |
| `test_types` | Types: maps, interfaces, structs, unsafe |
| `test_control` | Control flow: defer, panic, recover |
| `test_time` | Timer operations |
| `test_timers` | Timer scheduling |
| `test_alloc_inspect` | Memory allocation inspection (for docs) |

## Benchmarks

| Benchmark | Description |
|-----------|-------------|
| `bench_architecture` | **Run first** - validates all architecture parameters |
| `bench_detailed` | Detailed performance analysis |
| `bench_gc_pause` | GC pause time measurements |
| `bench_gc_techniques` | GC optimization techniques |
| `bench_goroutine_usecase` | Goroutine use case comparison |

## C Tests

| Test | Description |
|------|-------------|
| `test_gc_internals` | C-level GC verification |
| `test_gc_edge` | GC edge cases |
| `test_platform` | Platform-specific tests |

## Quick Start

```bash
# Set up environment
source /opt/toolchains/dc/kos/environ.sh

# Build all tests
cd tests
make

# Run a test on Dreamcast
dc-tool-ip -t 192.168.2.203 -x test_memory.elf
```

## Build Commands

| Command | Description |
|---------|-------------|
| `make` | Build all tests |
| `make test_NAME` | Build single test |
| `make clean` | Remove all artifacts |
| `make tidy` | Remove only .o files |
| `make V=1` | Verbose mode |

## Recommended Test Order

1. `test_print.elf` - Basic I/O works
2. `test_memory.elf` - Allocation and GC work
3. `test_goroutines.elf` - Concurrency works
4. `test_types.elf` - Type system works
5. `test_control.elf` - Control flow works
6. `bench_architecture.elf` - Get performance numbers

## Adding Tests

When contributing runtime changes, add tests to the appropriate consolidated file:

- Memory/GC changes → `test_memory.go`
- Goroutine/channel changes → `test_goroutines.go`
- Type system changes → `test_types.go`
- Defer/panic changes → `test_control.go`
