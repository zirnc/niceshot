#pragma once

#include <chrono>
#include <filesystem>

namespace niceshot {

struct Screenshot {
  std::filesystem::path path;
  std::chrono::sys_seconds captured_at{};
};

}  // namespace niceshot