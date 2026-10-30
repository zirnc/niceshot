#include "watch.h"

#include <thread>
namespace niceshot::app {
void watch(ScreenshotFolderProvider& folders, const SourceFactory& make_source,
           const WatchCallbacks& callbacks,
           const std::function<bool()>& should_stop) {
  auto folder = folders.current();
  while (!should_stop()) {
    if (folder.empty() || !std::filesystem::is_directory(folder)) {
      if (callbacks.on_error) {
        callbacks.on_error("screenshot folder not found: " + folder.string());
      }
      std::this_thread::sleep_for(std::chrono::seconds(1));
      folder = folders.current();
      continue;
    }

    auto source = make_source(folder);
    if (!source) {
      if (callbacks.on_error) callbacks.on_error("failed to create source");
      std::this_thread::sleep_for(std::chrono::seconds(1));
      folder = folders.current();
      continue;
    }
    std::set<std::filesystem::path> known;
    for (const auto& i : source->scan()) {
      known.insert(i.path);
    }
    bool folder_changed = false;
    if (callbacks.on_watching) callbacks.on_watching(folder);
    const bool ok = source->run(
        [&](const Screenshot& screenshot) {
          if (known.insert(screenshot.path).second && callbacks.on_new)
            callbacks.on_new(screenshot);
        },
        [&] {
          if (should_stop()) return true;
          const auto next = folders.current();
          if (next.lexically_normal() == folder.lexically_normal())
            return false;

          if (next.empty() || !std::filesystem::is_directory(next)) {
            if (callbacks.on_error)
              callbacks.on_error("screenshot folder not found: " +
                                 next.string());
            return false;
          }

          folder_changed = true;
          return true;
        });
    if (!ok) {
      if (callbacks.on_error)
        callbacks.on_error("failed to watch " + folder.string());
      std::this_thread::sleep_for(std::chrono::seconds(1));
      folder = folders.current();
      continue;
    }
    if (!folder_changed) break;
    folder = folders.current();
  }
}
}  // namespace niceshot::app