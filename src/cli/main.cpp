// cli: parse arguments, assemble concrete adapters, print results.
//
// M1: add `niceshot scan` and call your screenshot scanner.
// M2: add `niceshot watch` and wire a watcher.
//
// Keep this file thin: no xattr, no FSEvents, no SQLite here.
// See docs/ARCHITECTURE.md, section 2 and milestone M1.

#include <adapters/macos/fsevents_source.h>
#include <adapters/macos/polling_source.h>
#include <adapters/macos/screenshots.h>
#include <app/watch.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <print>
#include <string>
#include <thread>
#include <vector>

void usage() {
  std::print("niceshot 0.1.0\n");
  std::print("usage: niceshot <command> [options]\n");
}

void scan() {
  auto screenshots =
      niceshot::macOS::scan_screenshots(niceshot::macOS::screenshot_folder());
  std::print("Scan {} screenshots\n", screenshots.size());
  for (const auto& i : screenshots) {
    const auto captured_at =
        std::chrono::system_clock::to_time_t(i.captured_at);
    std::tm local_tm{};
    localtime_r(&captured_at, &local_tm);

    char local_time[32]{};
    std::strftime(local_time, sizeof(local_time), "%Y-%m-%d %H:%M:%S",
                  &local_tm);
    std::print("{} {}\n", local_time, i.path.filename().string());
  }
}

volatile std::sig_atomic_t g_stop = 0;
void handle_signal(int) { g_stop = 1; }
int watch() {
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  std::filesystem::path folder = niceshot::macOS::screenshot_folder();

  if (folder.empty() || !std::filesystem::is_directory(folder)) {
    std::fprintf(stderr, "screenshot folder not found: %s\n",
                 folder.string().c_str());
    return 3;
  }

  std::print("Watching {}\n", folder.string());
  std::fflush(stdout);
  niceshot::macOS::FSEventsSource source(folder);
  niceshot::app::watch(
      source,
      [](const niceshot::Screenshot& screenshot) {
        std::print("New screenshot! {}\n", screenshot.path.string());
        std::fflush(stdout);
      },
      [] { return g_stop; });
  std::print("Stopped\n");
  return 0;
}

int main(int argc, char* argv[]) {
  if (argc == 1) usage();
  for (int i = 1; i < argc; i++) {
    std::string command = argv[i];
    if (command == "scan") {
      scan();
    } else if (command == "watch") {
      return watch();
    }
  }
  return 0;
}