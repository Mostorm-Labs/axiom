#pragma once

#include "canvas/runtime/canvas_control_types.hpp"

namespace canvas::runtime {

struct CanvasControlApplyResult final {
  CanvasControlError error = CanvasControlError::kNone;
  const char* detail = "";
};

class CanvasControlPort {
 public:
  virtual ~CanvasControlPort() = default;
  [[nodiscard]] virtual bool isTargetCurrent(const CanvasTargetKey&) const noexcept { return true; }
  [[nodiscard]] virtual CanvasControlApplyResult apply(const CanvasControlRequest&) = 0;
  [[nodiscard]] virtual CanvasControlSnapshot snapshot(const CanvasTargetKey&) const = 0;
};

}  // namespace canvas::runtime
