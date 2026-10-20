// adapters: every macOS API lives here.
//
// M1: screenshot_folder(), isScreenshot(), scan_screenshots().
// M2: polling watcher. M3: FSEvents watcher.
// See docs/ARCHITECTURE.md, section 3 and milestones M1-M3.

#include "adapters/macos/screenshots.h"

#include <CoreFoundation/CoreFoundation.h>
#include <sys/stat.h>
#include <sys/xattr.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace niceshot::macOS {

namespace {

constexpr const char* kScreenCaptureAttribute =
    "com.apple.metadata:kMDItemIsScreenCapture";

std::optional<std::string> copyStringPreference(const char* key) {
  CFStringRef cf_key =
      CFStringCreateWithCString(nullptr, key, kCFStringEncodingUTF8);
  if (cf_key == nullptr) {
    return std::nullopt;
  }

  CFPropertyListRef value = CFPreferencesCopyAppValue(
      cf_key, CFSTR("com.apple.screencapture"));
  CFRelease(cf_key);

  if (value == nullptr) {
    return std::nullopt;
  }

  std::optional<std::string> result;
  if (CFGetTypeID(value) == CFStringGetTypeID()) {
    const auto string = static_cast<CFStringRef>(value);
    const CFIndex capacity =
        CFStringGetMaximumSizeForEncoding(CFStringGetLength(string),
                                          kCFStringEncodingUTF8) +
        1;
    std::string buffer(static_cast<std::size_t>(capacity), '\0');

    if (CFStringGetCString(string, buffer.data(), capacity,
                           kCFStringEncodingUTF8)) {
      buffer.resize(std::strlen(buffer.c_str()));
      result = std::move(buffer);
    }
  }

  CFRelease(value);
  return result;
}

std::filesystem::path expandTilde(const std::string& value) {
  if (value == "~" || value.rfind("~/", 0) == 0) {
    if (const char* home = std::getenv("HOME")) {
      if (value == "~") {
        return std::filesystem::path(home);
      }
      return std::filesystem::path(home) / value.substr(2);
    }
  }
  return std::filesystem::path(value);
}

bool isImageExtension(const std::filesystem::path& path) {
  std::string extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });

  return extension == ".png" || extension == ".jpg" ||
         extension == ".jpeg" || extension == ".heic" ||
         extension == ".tif" || extension == ".tiff";
}

bool hasScreenshotAttribute(const std::filesystem::path& path) {
  // The value is a binary plist; for detection we only need its presence.
  return ::getxattr(path.c_str(), kScreenCaptureAttribute, nullptr, 0, 0, 0) >=
         0;
}

}  // namespace
std::filesystem::path screenshot_folder() {
  for (const char* key : {"location-screenshot", "location"}) {
    if (auto value = copyStringPreference(key); value && !value->empty()) {
      return expandTilde(*value);
    }
  }

  if (const char* home = std::getenv("HOME")) {
    return std::filesystem::path(home) / "Desktop";
  }

  return {};
}

bool isScreenshot(std::filesystem::path screenshot) {
  return isImageExtension(screenshot) && hasScreenshotAttribute(screenshot);
}

std::vector<Screenshot> scan_screenshots(const std::filesystem::path& folder) {
  std::vector<Screenshot> screenshots;

  std::error_code error;
  std::filesystem::directory_iterator iterator(
      folder, std::filesystem::directory_options::skip_permission_denied,
      error);
  if (error) {
    return screenshots;
  }

  for (const auto& entry : iterator) {
    std::error_code entry_error;
    if (!entry.is_regular_file(entry_error) || entry_error) {
      continue;
    }

    const std::filesystem::path& path = entry.path();
    if (!isScreenshot(path)) {
      continue;
    }

    struct stat info {};
    if (::stat(path.c_str(), &info) != 0) {
      continue;
    }

    auto seconds = info.st_birthtimespec.tv_sec;
    if (seconds <= 0) {
      seconds = info.st_mtimespec.tv_sec;
    }

    screenshots.push_back(Screenshot{
        path,
        std::chrono::sys_seconds{std::chrono::seconds{seconds}},
    });
  }

  std::sort(screenshots.begin(), screenshots.end(),
            [](const Screenshot& left, const Screenshot& right) {
              if (left.captured_at != right.captured_at) {
                return left.captured_at > right.captured_at;
              }
              return left.path.filename().string() >
                     right.path.filename().string();
            });

  return screenshots;
}

};  // namespace niceshot::macOS