#pragma once

#include <cstdint>

namespace canvas::input {

enum class PointerTool : std::uint8_t { kUnknown, kFinger, kPen, kEraser };

struct ContactGeometry final {
  float width = 0.0F;
  float height = 0.0F;
  float orientation = 0.0F;
  // False means the platform did not provide geometry. Zero dimensions are
  // therefore not interpreted as a measured zero-area contact.
  bool available = false;
  [[nodiscard]] float area() const noexcept { return width * height; }
};

struct PointerCapabilities final {
  PointerTool tool = PointerTool::kUnknown;
  bool pressure = false;
  bool barrelButton = false;
  bool hardwareEraser = false;
  bool coalescedHistory = false;
  bool contactGeometry = false;
};

}  // namespace canvas::input
