#pragma once

#include "domain/screenshot.h"

#include <filesystem>
#include <vector>

namespace niceshot::macOS {

/// Folder configured in com.apple.screencapture.
std::filesystem::path screenshot_folder();

/// True when the path is a screenshot image.
bool isScreenshot(std::filesystem::path screenshot);

/// Screenshots in `folder`, newest first.
std::vector<Screenshot> scan_screenshots(const std::filesystem::path& folder);

}  // namespace niceshot::macOS