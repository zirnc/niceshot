#pragma once
#include <adapters/macos/screenshots.h>
#include <ports/screenshot_source.h>

#include <chrono>
#include <filesystem>
#include <thread>
#include <utility>

namespace niceshot::macOS {
class PollingSource final : public ScreenshotSource {
 public:
  PollingSource(std::filesystem::path folder)
      : screenshotFolder(std::move(folder)) {};
  std::vector<Screenshot> scan() const override {
    return scan_screenshots(screenshotFolder);
  }

  void run(const std::function<void(const Screenshot&)>& on_screenshot,
           const std::function<bool()>& should_stop) override;

 private:
  std::filesystem::path screenshotFolder;
};
}  // namespace niceshot::macOS