#pragma once

#include "canvas/control/canvas_control_service.hpp"
#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/runtime/runtime_facade.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <atomic>
#include <vector>

namespace canvas::runtime {

// Shared product-control owner used by Minimal, Playground, Windows and Web.
// It owns one CanvasControlService queue and binds it to an existing
// CanvasEditingRuntime; it never creates a second document or history owner.
class CanvasRuntimeFacade : public RuntimeFacade {
 public:
  CanvasRuntimeFacade(interaction::CanvasEditingRuntime& runtime,
                      CanvasTargetKey target) noexcept;
  ~CanvasRuntimeFacade() override;
  CanvasRuntimeFacade(const CanvasRuntimeFacade&) = delete;
  CanvasRuntimeFacade& operator=(const CanvasRuntimeFacade&) = delete;

  [[nodiscard]] ProductControlReceipt submitCanvasControl(
      const CanvasControlRequest& request) noexcept override;
  [[nodiscard]] CanvasControlApplyResult applyControl(
      const CanvasControlRequest& request);
  [[nodiscard]] CanvasControlSnapshot snapshotControl(
      const CanvasTargetKey& target) const;
  [[nodiscard]] CanvasControlPort& controlPort() noexcept;
  [[nodiscard]] ProductControlReceipt submitProductControl(
      const ProductControlRequest& request) noexcept override;
  [[nodiscard]] RuntimeStateSnapshot readRuntimeState() const noexcept override;
  [[nodiscard]] CanvasControlSnapshot readCanvasControlSnapshot(
      const CanvasTargetKey& target) const noexcept override;
  [[nodiscard]] std::vector<CanvasTargetKey> mountedCanvasTargets() const override;
  [[nodiscard]] std::vector<BrushPresetRef> brushPresets() const override;
  [[nodiscard]] std::optional<CanvasControlReceipt> canvasControlReceipt(
      CanvasControlReceiptKey key) const noexcept override;
  [[nodiscard]] std::uint64_t controlSequence() const noexcept override {
    return controlSequence_.load(std::memory_order_acquire);
  }

  // Only the serialized Runtime owner calls this method.
  [[nodiscard]] std::size_t process(std::uint64_t ownerSequence) noexcept;
  void setTarget(CanvasTargetKey target) noexcept;
  [[nodiscard]] const CanvasTargetKey& target() const noexcept;
  [[nodiscard]] bool isTargetCurrent(const CanvasTargetKey& target) const noexcept;
  [[nodiscard]] interaction::CanvasEditingRuntime& editingRuntime() noexcept {
    return runtime_;
  }
  [[nodiscard]] bool ownerThread() const noexcept;
  [[nodiscard]] const interaction::CanvasEditingRuntime& editingRuntime() const noexcept {
    return runtime_;
  }

 private:
  struct Binding;
  void refreshTarget() const noexcept;

  interaction::CanvasEditingRuntime& runtime_;
  mutable std::unique_ptr<Binding> binding_;
  mutable std::unique_ptr<CanvasControlService> service_;
  const std::thread::id ownerThread_ = std::this_thread::get_id();
  mutable std::atomic_bool processing_{false};
  std::uint64_t revision_ = 1;
  std::atomic_uint64_t controlSequence_{0};
};

}  // namespace canvas::runtime
