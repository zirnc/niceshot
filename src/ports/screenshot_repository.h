#pragma once

#include <domain/screenshot.h>

#include <vector>

namespace niceshot {
class ScreenshotRepository {
 public:
  virtual ~ScreenshotRepository() = default;

  virtual void upsert(const Screenshot& screenshot) = 0;
  virtual std::vector<Screenshot> recent(int limit) = 0;
};

}  // namespace niceshot