# libgodc - Go runtime for Sega Dreamcast

<p align="center">
  <img src="logo.png" alt="libgodc" width="400">
</p>

<p align="center">
  <img src="examples/pong/pong.gif" alt="Pong" width="240">
  <img src="examples/brkout/brkout.gif" alt="Breakout" width="240">
  <img src="examples/platformer/platformer.gif" alt="Platformer" width="240">
</p>

Replaces the standard Go runtime with one designed for the Dreamcast's
constraints: memory 16MB RAM, CPU single-core SH-4, no operating system. Provides garbage
collection, goroutines, channels, and the core runtime functions.

## Quick Start

```sh
go install github.com/drpaneas/godc@latest
godc setup
godc doctor # to check (optional)
```

> **Note:** The [`godc`](https://github.com/drpaneas/godc) CLI tool is a separate project that handles toolchain setup and builds.

Create and run a project:

```sh
mkdir myproject && cd myproject
godc init
# write you main.go and other *.go files
godc build
godc run
```

See the [Quick Start Guide](https://drpaneas.github.io/libgodc/getting-started/quick-start.html) for your first program.

## Documentation

📚 **[Full Documentation](https://drpaneas.github.io/libgodc/)**

- [Installation](https://drpaneas.github.io/libgodc/getting-started/installation.html) — Setup and configuration
- [Quick Start](https://drpaneas.github.io/libgodc/getting-started/quick-start.html) — First program walkthrough
- [Design](https://drpaneas.github.io/libgodc/reference/design.html) — Runtime architecture
- [Effective Dreamcast Go](https://drpaneas.github.io/libgodc/reference/effective-dreamcast-go.html) — Best practices
- [KOS Wrappers](https://drpaneas.github.io/libgodc/reference/kos-wrappers.html) — Calling C from Go
- [Limitations](https://drpaneas.github.io/libgodc/reference/limitations.html) — What doesn't work

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

- `hello` — Minimal program (debug output)
- `hello_screen` — Hello World on screen using BIOS font
- `blue_screen` — Minimal graphics
- `input` — Controller input
- `goroutines` — Concurrent bouncing balls
- `channels` — Producer/consumer pattern
- `timer` — Frame-rate independent animation
- `bfont` — BIOS font rendering
- `filesystem` — Directory browser
- `vmu` — VMU LCD and buzzer
- `brkout` — Breakout clone (GPL v2, port of Jim Ursetto's original)
- `pong` — Pong clone with 1P/2P mode, particle effects, and AI

## License

BSD 3-Clause License. See [LICENSE](LICENSE) for details.
