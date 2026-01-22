#include "camera.h"

auto main() -> int {
  picam::Camera::Config config;
  config.camera_name_hint = "TestCam";
  config.image_size = {1280, 720};

  picam::Camera camera(config, [](const picam::ImageFrame& frame) {
    // Process the image frame (e.g., display or save)
  });

  while (true) {
    camera.acquire();
  }

  return 0;
}