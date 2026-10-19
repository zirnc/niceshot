# niceshot

A macOS screenshot shelf, built one vertical slice at a time as a software-architecture exercise.

**Status: learning scaffold.** There is intentionally no feature code yet. The implementation is your job; the repo gives you the build, the boundaries, and the acceptance criteria.

**Start here: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).** It contains the milestone plan, contracts, API map, and the first assignment (M1: `niceshot scan`).

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
src/domain/     pure values and rules (no macOS headers)
src/ports/      interfaces the core owns
src/app/        use cases and policy
src/adapters/   macOS, SQLite, configuration
src/cli/        argument parsing, wiring, output
tests/          unit, integration and CLI tests
docs/           architecture notes and the learning path
```

## Dependency rule

`core` (domain + ports + app) uses the standard library only. It must not include `sqlite3.h`, `ImageIO`, `CoreFoundation`, `FSEvents`, or any AppKit header. Platform code lives in `adapters/`.

## How to work on this

1. Pick one milestone from the guide.
2. Write the smallest implementation that satisfies its acceptance criteria.
3. Run it against real files, not just your imagination.
4. Commit when it works; write down the design decision in `NOTES.md`.
5. Ask for a review with the goal, actual output, and expected output.

Do not build the whole architecture up front. M1 is deliberately small: list screenshots, filter by the screenshot xattr, and print them.