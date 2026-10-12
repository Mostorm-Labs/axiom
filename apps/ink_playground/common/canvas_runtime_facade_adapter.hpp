#pragma once

#include "canvas_control_binding.hpp"
#include "canvas/control/canvas_control_service.hpp"
#include "canvas/runtime/runtime_facade.hpp"

#include <cstddef>
#include <optional>
#include <vector>
#include <thread>
#include <atomic>

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

 public:
  // Called only by the platform/runtime owner at its serialized safe point.
  // Submission never executes the queue inline or re-enters the owner.
  [[nodiscard]] std::size_t processCanvasControls(std::uint64_t ownerSequence) noexcept {
    if (std::this_thread::get_id() != ownerThread_ || processing_.exchange(true)) return 0;
    const auto result = service_.processPending(ownerSequence);
    processing_ = false;
    return result;
  }

 protected:
  [[nodiscard]] runtime::CanvasControlService& canvasControlService() noexcept {
    return service_;
  }
  [[nodiscard]] const runtime::CanvasTargetKey& canvasTarget() const noexcept {
    return binding_.target();
  }
 private:
  CanvasControlBinding binding_;
  runtime::CanvasControlService service_;
  const std::thread::id ownerThread_ = std::this_thread::get_id();
  std::atomic_bool processing_{false};
};

}  // namespace canvas::ink_playground
