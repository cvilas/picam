//=================================================================================================
// Copyright (C) 2025 GRAPE Contributors
//=================================================================================================

#pragma once

#include <memory>

#include "image_frame.h"

namespace picam {

//=================================================================================================
/// OpenGL display window using GLFW for rendering camera frames
class Display {
public:
  Display();

  /// Update the display with a new camera frame
  /// @param frame Camera image frame to display
  void update(const ImageFrame& frame);

  /// Process window events (must be called periodically)
  /// @return false if window should close, true otherwise
  auto processEvents() -> bool;

  ~Display();
  Display(const Display&) = delete;
  Display(Display&&) = delete;
  auto operator=(const Display&) = delete;
  auto operator=(Display&&) = delete;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace picam
