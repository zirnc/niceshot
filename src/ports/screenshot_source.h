#pragma once
#include <domain/screenshot.h>

#include <functional>
#include <vector>

namespace niceshot {
class ScreenshotSource {
 public:
  virtual ~ScreenshotSource() = default;
  virtual std::vector<Screenshot> scan() const = 0;

  virtual bool run(const std::function<void(const Screenshot&)>& on_screenshot,
                   const std::function<bool()>& should_stop) = 0;
};

}  // namespace niceshot