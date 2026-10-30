#pragma once
#include <filesystem>

namespace niceshot {
class ScreenshotFolderProvider {
 public:
  virtual ~ScreenshotFolderProvider() = default;
  virtual std::filesystem::path current() = 0;
};
}  // namespace niceshot