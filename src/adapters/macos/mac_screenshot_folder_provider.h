#pragma once
#include <ports/screenshot_folder_provider.h>

#include <chrono>
#include <filesystem>

namespace niceshot::macOS {
class MacScreenshotFolderProvider final : public ScreenshotFolderProvider {
 public:
  explicit MacScreenshotFolderProvider(
      std::chrono::milliseconds refresh_interval = std::chrono::seconds(1))
      : refresh_interval_(refresh_interval) {}

  std::filesystem::path current() override;

 private:
  std::chrono::milliseconds refresh_interval_;
  std::chrono::steady_clock::time_point last_checked_{};
  std::filesystem::path cached_;
  bool initialized_ = false;
};

}  // namespace niceshot::macOS