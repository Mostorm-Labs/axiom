#pragma once

#include <cstdint>

namespace canvas::input {

enum class PointerTool : std::uint8_t { kUnknown, kFinger, kPen, kEraser };

struct ContactGeometry final {
  float width = 0.0F;
  float height = 0.0F;
  float orientation = 0.0F;
  [[nodiscard]] float area() const noexcept { return width * height; }
};

struct PointerCapabilities final {
  PointerTool tool = PointerTool::kUnknown;
  bool pressure = false;
  bool barrelButton = false;
  bool hardwareEraser = false;
  bool coalescedHistory = false;
};

}  // namespace canvas::input
