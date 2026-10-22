#include "polling_source.h"

namespace niceshot::macOS {

void PollingSource::run(
    const std::function<void(const Screenshot&)>& on_screenshot,
    const std::function<bool()>& should_stop) {
  using namespace std::chrono_literals;

  while (!should_stop()) {
    for (const auto& screenshot : scan()) {
      on_screenshot(screenshot);
    }
    std::this_thread::sleep_for(500ms);
  }
}

}  // namespace niceshot::macOS