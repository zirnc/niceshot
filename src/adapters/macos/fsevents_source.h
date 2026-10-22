#pragma once

#include <CoreServices/CoreServices.h>
#include <adapters/macos/screenshots.h>
#include <ports/screenshot_source.h>

#include <filesystem>
#include <functional>
#include <utility>
#include <vector>

namespace niceshot::macOS {
class FSEventsSource final : public ScreenshotSource {
 public:
  explicit FSEventsSource(std::filesystem::path folder)
      : screenshotFolder(std::move(folder)) {}

  std::vector<Screenshot> scan() const override {
    return scan_screenshots(screenshotFolder);
  }

  void run(const std::function<void(const Screenshot&)>& on_screenshot,
           const std::function<bool()>& should_stop) override;

 private:
  static void callback(ConstFSEventStreamRef stream, void* callbackInfo,
                       size_t numEvents, void* evPaths,
                       const FSEventStreamEventFlags evFlags[],
                       const FSEventStreamEventId evIds[]);

  std::filesystem::path screenshotFolder;
  std::function<void(const Screenshot&)> on_screenshot_func;
};

}  // namespace niceshot::macOS