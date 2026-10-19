// cli: parse arguments, assemble concrete adapters, print results.
//
// M1: add `niceshot scan` and call your screenshot scanner.
// M2: add `niceshot watch` and wire a watcher.
//
// Keep this file thin: no xattr, no FSEvents, no SQLite here.
// See docs/ARCHITECTURE.md, section 2 and milestone M1.

#include <adapters/macos/screenshots.h>

#include <cstdio>
#include <iostream>
#include <print>
#include <string>
#include <vector>

void scan() {
  auto screenshots =
      niceshot::macOS::scan_screenshots(niceshot::macOS::screenshot_folder());
  std::print("Scan {} screenshots\n", screenshots.size());
  for (auto i : screenshots) {
    std::print("{} {}\n", i.captured_at, i.path.filename().string());
  }
}

int main(int argc, char* argv[]) {
  std::printf("niceshot 0.1.0\n");
  std::printf("usage: niceshot <command> [options]\n");

  for (int i = 1; i < argc; i++) {
    std::string command = argv[i];
    if (command == "scan") {
      scan();
    }
  }
  return 0;
}