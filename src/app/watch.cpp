#include "watch.h"

namespace niceshot::app {
void watch(ScreenshotSource& source,
           const std::function<void(const Screenshot&)>& on_new,
           const std::function<bool()>& should_stop) {
  std::set<std::filesystem::path> known;
  for (const auto& i : source.scan()) {
    known.insert(i.path);
  }

  // TODO: what if delete ?
  source.run(
      [&](const Screenshot& screenshot) {
        if (known.insert(screenshot.path).second) {
          on_new(screenshot);
        }
      },
      should_stop);
}
}  // namespace niceshot::app