#pragma once
#include <ports/screenshot_source.h>

#include <set>

namespace niceshot::app {
void watch(ScreenshotSource& source,
           const std::function<void(const Screenshot&)>& on_new,
           const std::function<bool()>& should_stop);
}