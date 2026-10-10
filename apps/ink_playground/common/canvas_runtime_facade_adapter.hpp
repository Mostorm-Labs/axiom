#pragma once

#include "canvas_control_binding.hpp"
#include "canvas/control/canvas_control_service.hpp"
#include "canvas/runtime/runtime_facade.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace canvas::ink_playground {

// Common RuntimeFacade adapter for platform shells.  Platforms retain their
// diagnostics and legacy input hooks, while all typed canvas controls use one
// owner binding and one bounded service queue.
class CanvasRuntimeFacadeAdapter : public runtime::RuntimeFacade {
 public:
  CanvasRuntimeFacadeAdapter(InkPlaygroundHost& host,
                             runtime::CanvasTargetKey target) noexcept
      : binding_(host, target), service_(binding_) {}

  [[nodiscard]] runtime::ProductControlReceipt submitCanvasControl(
      const runtime::CanvasControlRequest& request) noexcept override;
  [[nodiscard]] runtime::CanvasControlSnapshot readCanvasControlSnapshot(
      const runtime::CanvasTargetKey& target) const noexcept override;
  [[nodiscard]] std::vector<runtime::CanvasTargetKey> mountedCanvasTargets() const override;
  [[nodiscard]] std::vector<runtime::BrushPresetRef> brushPresets() const override;
  [[nodiscard]] std::optional<runtime::CanvasControlReceipt> canvasControlReceipt(
      runtime::CanvasControlReceiptKey key) const noexcept override {
    return service_.receipt(key);
  }

 protected:
  [[nodiscard]] runtime::CanvasControlService& canvasControlService() noexcept {
    return service_;
  }
  [[nodiscard]] const runtime::CanvasTargetKey& canvasTarget() const noexcept {
    return binding_.target();
  }
  [[nodiscard]] std::size_t processCanvasControls(std::uint64_t ownerSequence) noexcept {
    return service_.processPending(ownerSequence);
  }

 private:
  CanvasControlBinding binding_;
  runtime::CanvasControlService service_;
};

}  // namespace canvas::ink_playground
