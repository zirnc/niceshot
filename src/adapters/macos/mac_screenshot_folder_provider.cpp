#include "mac_screenshot_folder_provider.h"

#include <CoreFoundation/CoreFoundation.h>
#include <adapters/macos/screenshots.h>

namespace niceshot::macOS {
std::filesystem::path MacScreenshotFolderProvider::current() {
  const auto now = std::chrono::steady_clock::now();

  if (initialized_ && now - last_checked_ < refresh_interval_) return cached_;

  initialized_ = true;
  last_checked_ = now;

  CFPreferencesAppSynchronize(CFSTR("com.apple.screencapture"));

  cached_ = screenshot_folder();
  return cached_;
}
}  // namespace niceshot::macOS