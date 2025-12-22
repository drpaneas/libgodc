# libgodc

Go runtime for Sega Dreamcast.

Replaces the standard Go runtime with one designed for the Dreamcast's
constraints: 16MB RAM, single-core SH-4, no operating system. Provides garbage
collection, goroutines, channels, and the core runtime functions.

## Quick Start

```sh
go install github.com/drpaneas/godc@latest
godc setup
godc doctor # to check (optional)
```

Create and run a project:

```sh
mkdir myproject && cd myproject
godc init
# write you main.go and other *.go files
godc build
godc run
```

See the [Quick Start Guide](docs/getting-started/quick-start.md) for your first program.

## Documentation

- [Installation](docs/getting-started/installation.md) — Setup and configuration
- [Quick Start](docs/getting-started/quick-start.md) — First program walkthrough
- [Design](docs/reference/design.md) — Runtime architecture
- [Effective Dreamcast Go](docs/reference/effective-dreamcast-go.md) — Best practices
- [KOS Wrappers](docs/reference/kos-wrappers.md) — Calling C from Go
- [Limitations](docs/reference/limitations.md) — What doesn't work

## Performance

Measured on real hardware (SH-4 @ 200MHz):

| Operation           | Time     |
|---------------------|----------|
| Gosched yield       | ~120 ns  |
| Allocation          | ~186 ns  |
| Buffered channel    | ~1.8 μs  |
| Context switch      | ~6.4 μs  |
| Unbuffered channel  | ~13 μs   |
| Goroutine spawn     | ~31 μs   |
| GC pause            | 72 μs - 6 ms |

## Examples

The `examples/` directory contains working programs:

- `hello` — Minimal program
- `blue_screen` — Minimal graphics
- `input` — Controller input
- `goroutines` — Concurrent bouncing balls
- `channels` — Producer/consumer pattern
- `timer` — Frame-rate independent animation
- `bfont` — BIOS font rendering
- `filesystem` — Directory browser
- `vmu` — VMU LCD and buzzer
- `brkout` — Complete Breakout game

## License

BSD 3-Clause License. See [LICENSE](LICENSE) for details.
