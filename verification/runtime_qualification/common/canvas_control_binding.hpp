#pragma once

#include "canvas/runtime/canvas_runtime_facade.hpp"
#include "ink_playground_host.hpp"

namespace canvas::ink_playground {

class CanvasControlBinding final : public runtime::CanvasControlPort {
 public:
  explicit CanvasControlBinding(InkPlaygroundHost& host,
                                runtime::CanvasTargetKey target) noexcept
      : facade_(host, target) {}

  [[nodiscard]] runtime::CanvasControlApplyResult apply(
      const runtime::CanvasControlRequest& request) override {
    return facade_.controlPort().apply(request);
  }
  [[nodiscard]] runtime::CanvasControlSnapshot snapshot(
      const runtime::CanvasTargetKey& target) const override {
    return facade_.snapshotControl(target);
  }
  [[nodiscard]] bool isTargetCurrent(const runtime::CanvasTargetKey& target) const noexcept override {
    return target == facade_.target();
  }
  void setTarget(runtime::CanvasTargetKey target) noexcept { facade_.setTarget(target); }
  [[nodiscard]] const runtime::CanvasTargetKey& target() const noexcept { return facade_.target(); }

 private:
  runtime::CanvasRuntimeFacade facade_;
};

}  // namespace canvas::ink_playground
