#pragma once

#include "canvas/control/canvas_control_port.hpp"
#include "ink_playground_host.hpp"

#include <cstdint>

namespace canvas::ink_playground {

class CanvasControlBinding final : public runtime::CanvasControlPort {
 public:
  explicit CanvasControlBinding(InkPlaygroundHost& host,
                                runtime::CanvasTargetKey target) noexcept
      : host_(host), target_(target) {}

  [[nodiscard]] bool isTargetCurrent(const runtime::CanvasTargetKey& target) const noexcept override {
    return target == target_;
  }
  [[nodiscard]] runtime::CanvasControlApplyResult apply(
      const runtime::CanvasControlRequest& request) override;
  [[nodiscard]] runtime::CanvasControlSnapshot snapshot(
      const runtime::CanvasTargetKey& target) const override;
  void setTarget(runtime::CanvasTargetKey target) noexcept { target_ = target; }
  [[nodiscard]] const runtime::CanvasTargetKey& target() const noexcept { return target_; }

 private:
  InkPlaygroundHost& host_;
  runtime::CanvasTargetKey target_;
  runtime::CanvasControlSnapshot snapshot_{};
  std::uint64_t revision_ = 1;
};

}  // namespace canvas::ink_playground
