//=================================================================================================
// Copyright (C) 2025 GRAPE Contributors
//=================================================================================================

#pragma once

#include <chrono>
#include <cstdint>
#include <span>
#include <tuple>

namespace picam {

//=================================================================================================
/// Image dimensions in pixels
struct ImageSize {
  std::uint16_t width{};   //!< Width of the image in pixels
  std::uint16_t height{};  //!< Height of the image in pixels (number of rows)
  constexpr auto operator<=>(const ImageSize&) const = default;
};

//=================================================================================================
/// Single image frame data
struct ImageFrame {
  struct Header {
    std::chrono::system_clock::time_point timestamp;  //!< Acquistion timestamp
    std::uint32_t pitch{};                            //!< Bytes per row of pixels
    ImageSize size;                                   //!< Image dimensions
    std::uint32_t format{};                           //!< driver backend-specific pixel format
  };
  Header header;
  std::span<std::byte> pixels;  //!< pixel data
};

/// @return true if image dimensions and format matches
constexpr auto matchesFormat(const ImageFrame::Header& hd1, const ImageFrame::Header& hd2) -> bool {
  return std::tie(hd1.size, hd1.format) == std::tie(hd2.size, hd2.format);
}

}  // namespace picam
