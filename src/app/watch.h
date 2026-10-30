#pragma once
#include <ports/screenshot_folder_provider.h>
#include <ports/screenshot_source.h>

#include <set>

namespace niceshot::app {
struct WatchCallbacks {
  std::function<void(const std::filesystem::path&)> on_watching;
  std::function<void(const Screenshot&)> on_new;
  std::function<void(const std::string&)> on_error;
};
using SourceFactory = std::function<std::unique_ptr<ScreenshotSource>(
    const std::filesystem::path&)>;

void watch(ScreenshotFolderProvider& folders, const SourceFactory& make_source,
           const WatchCallbacks& callbacks,
           const std::function<bool()>& should_stop);
}  // namespace niceshot::app