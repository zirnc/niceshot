#include <adapters/macos/fsevents_source.h>
#include <adapters/macos/mac_screenshot_folder_provider.h>
#include <adapters/macos/polling_source.h>
#include <adapters/macos/screenshots.h>
#include <adapters/sqlite/sqlite_screenshot_repository.h>
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

  niceshot::sqlite::SqliteScreenshotRepository repository("./test.db");

  auto make_source = [](const std::filesystem::path& folder)
      -> std::unique_ptr<niceshot::ScreenshotSource> {
    return std::make_unique<niceshot::macOS::FSEventsSource>(folder);
  };
  niceshot::macOS::MacScreenshotFolderProvider folders;

  niceshot::app::WatchCallbacks callbacks{
      .on_watching =
          [](const std::filesystem::path& folder) {
            std::print("Watching {}\n", folder.string());
            std::fflush(stdout);
          },
      .on_new =
          [&repository](const niceshot::Screenshot& screenshot) {
            repository.upsert(screenshot);
            std::print("New screenshot! {}\n", screenshot.path.string());
            std::fflush(stdout);
          },
      .on_error =
          [](const std::string& message) {
            std::fprintf(stderr, "%s\n", message.c_str());
          },
  };

  niceshot::app::watch(folders, make_source, callbacks,
                       [] { return g_stop != 0; });

  auto v = repository.recent(10);
  for (auto i : v) {
    std::cout << i.path.c_str() << std::endl;
  }
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