#include "camera.h"
#include "display.h"

auto main() -> int {
  auto display = picam::Display();

  const auto config = picam::Camera::Config{
    .camera_name_hint = "TestCam",
    .image_size = {1280, 720}
  };

  auto camera = picam::Camera(config, [&display](const picam::ImageFrame& frame) {
    display.update(frame);
  });

  while (display.processEvents()) {
    camera.acquire();
  }

  return 0;
}