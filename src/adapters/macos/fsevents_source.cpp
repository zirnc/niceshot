#include "fsevents_source.h"

#include <dispatch/dispatch.h>

#include <chrono>
#include <cstdio>
#include <thread>

namespace niceshot::macOS {

void FSEventsSource::callback(ConstFSEventStreamRef, void* callbackInfo, size_t,
                              void*, const FSEventStreamEventFlags[],
                              const FSEventStreamEventId[]) {
  auto* self = static_cast<FSEventsSource*>(callbackInfo);
  if (self == nullptr) {
    return;
  }

  for (const auto& screenshot : self->scan()) {
    self->on_screenshot_func(screenshot);
  }
}

bool FSEventsSource::run(
    const std::function<void(const Screenshot&)>& on_screenshot,
    const std::function<bool()>& should_stop) {
  on_screenshot_func = on_screenshot;

  CFStringRef path = CFStringCreateWithCString(
      kCFAllocatorDefault, screenshotFolder.c_str(), kCFStringEncodingUTF8);
  if (path == nullptr) {
    std::fprintf(stderr, "CFStringCreateWithCString failed\n");
    return 0;
  }

  const void* values[] = {path};
  CFArrayRef paths = CFArrayCreate(nullptr, values, 1, &kCFTypeArrayCallBacks);
  CFRelease(path);

  if (paths == nullptr) {
    std::fprintf(stderr, "CFArrayCreate failed\n");
    return 0;
  }

  FSEventStreamContext context{};
  context.version = 0;
  context.info = this;

  FSEventStreamRef stream = FSEventStreamCreate(
      nullptr, &FSEventsSource::callback, &context, paths,
      kFSEventStreamEventIdSinceNow, 0.5, kFSEventStreamCreateFlagFileEvents);

  if (stream == nullptr) {
    std::fprintf(stderr, "FSEventStreamCreate failed\n");
    CFRelease(paths);
    return 0;
  }

  dispatch_queue_t queue =
      dispatch_queue_create("niceshot.fsevents", DISPATCH_QUEUE_SERIAL);
  if (queue == nullptr) {
    std::fprintf(stderr, "dispatch_queue_create failed\n");
    FSEventStreamInvalidate(stream);
    FSEventStreamRelease(stream);
    CFRelease(paths);
    return 0;
  }

  FSEventStreamSetDispatchQueue(stream, queue);

  if (!FSEventStreamStart(stream)) {
    std::fprintf(stderr, "FSEventStreamStart failed\n");
    FSEventStreamInvalidate(stream);
    FSEventStreamRelease(stream);
    CFRelease(paths);
    dispatch_release(queue);
    return 0;
  }

  while (!should_stop()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  FSEventStreamStop(stream);
  FSEventStreamInvalidate(stream);

  // Wait for callbacks already queued before releasing the source and queue.
  dispatch_sync_f(queue, nullptr, [](void*) {});

  FSEventStreamRelease(stream);
  CFRelease(paths);
  dispatch_release(queue);
  return 1;
}
}  // namespace niceshot::macOS