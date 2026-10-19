# niceshot

A macOS screenshot shelf, built one layer at a time as a software-architecture exercise.

**Status: skeleton only.** There is no feature code yet; `src/cli/main.cpp` is a placeholder.

## Requirements

- macOS 14 or later
- Command Line Tools (`xcode-select --install`)
- CMake 3.25 or later
- GNU Make (ships with macOS) or Ninja

## Build

> CMake 4.4 is already installed via Homebrew on this machine. If `cmake` is not on your `PATH`, use `/opt/homebrew/bin/cmake`.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/niceshot --help
```

With sanitizers:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DNICESHOT_SANITIZE=ON
```

## Layout

```text
src/domain/     pure values and rules: Item, RetentionPolicy
src/ports/      interfaces the core needs (no implementations)
src/app/        use cases: scan, list, thumb, watch, prune
src/adapters/   SQLite, macOS frameworks, configuration
src/cli/        argument parsing, wiring, output
tests/          unit, integration and CLI tests
docs/           architecture notes and decision records
```

## Dependency rule

`core` (domain + ports + app) uses the standard library only. It must not include  
`sqlite3.h`, `ImageIO`, or any AppKit header. Platform code lives in `adapters/`.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the full design and learning path.
