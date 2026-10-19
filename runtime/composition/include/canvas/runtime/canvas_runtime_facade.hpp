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
#include <deque>
#include <mutex>
#include <variant>
#include <unordered_map>

namespace canvas::runtime {

// Shared product-control owner used by Minimal, Playground, Windows and Web.
// It owns one CanvasControlService queue and binds it to an existing
// CanvasEditingRuntime; it never creates a second document or history owner.
class CanvasRuntimeFacade : public RuntimeFacade {
 public:
#if defined(AXIOM_RUNTIME_QUALIFICATION)
  CanvasRuntimeFacade(interaction::CanvasEditingRuntime& runtime,
                      CanvasTargetKey target,std::uint64_t runtimeInstance = 1) noexcept;
#else
 private:
  friend class RuntimeComposition;
  CanvasRuntimeFacade(interaction::CanvasEditingRuntime& runtime,
                      CanvasTargetKey target,std::uint64_t runtimeInstance = 1) noexcept;
 public:
#endif
  ~CanvasRuntimeFacade() override;
  [[nodiscard]] TextReceipt submitTextIntent(const TextIntent&) override;
  [[nodiscard]] TextReceipt submitTextInput(const TextInputEvent&) override;
  [[nodiscard]] TextEditorSnapshot readTextEditor() const override;
  [[nodiscard]] std::optional<TextReceipt> textInputReceipt(const TextSessionToken&,std::uint64_t) const override;
  [[nodiscard]] std::optional<TextReceipt> textIntentReceipt(std::uint64_t,std::uint64_t) const override;
  [[nodiscard]] bool acknowledgeTextInput(const TextSessionToken&,std::uint64_t) override;
  CanvasRuntimeFacade(const CanvasRuntimeFacade&) = delete;
  CanvasRuntimeFacade& operator=(const CanvasRuntimeFacade&) = delete;

  [[nodiscard]] ProductControlReceipt submitCanvasControl(
      const CanvasControlRequest& request) noexcept override;
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

#if defined(AXIOM_RUNTIME_QUALIFICATION)
 public:
  // White-box qualification only. RuntimeComposition is the only production
  // owner that invokes these execution seams; ordinary consumers receive the
  // RuntimeFacade base interface from RuntimeComposition::controlFacade().
#else
 private:
  // Internal RuntimeComposition execution seam; ordinary consumers cannot
  // reach the concrete binding or editor.
#endif
  friend class RuntimeComposition;
  [[nodiscard]] CanvasControlApplyResult applyControl(
      const CanvasControlRequest& request);
  [[nodiscard]] CanvasControlSnapshot snapshotControl(
      const CanvasTargetKey& target) const;
  [[nodiscard]] CanvasControlPort& controlPort() noexcept;
  [[nodiscard]] std::size_t process(std::uint64_t ownerSequence) noexcept;
  void setTarget(CanvasTargetKey target) noexcept;
  [[nodiscard]] const CanvasTargetKey& target() const noexcept;
  [[nodiscard]] bool isTargetCurrent(const CanvasTargetKey& target) const noexcept;
  [[nodiscard]] bool ownerThread() const noexcept;
  [[nodiscard]] interaction::CanvasEditingRuntime& editingRuntime() noexcept {
    return runtime_;
  }
  [[nodiscard]] const interaction::CanvasEditingRuntime& editingRuntime() const noexcept {
    return runtime_;
  }
 private:
  struct Binding;
  [[nodiscard]] std::size_t processIngress(std::uint64_t);
  void finishTextReceipts();
  using Work = std::variant<CanvasControlRequest,TextIntent,TextInputEvent>;
  mutable std::mutex ingressMutex_;
  std::deque<Work> ingress_;
  std::deque<CanvasControlRequest> deferredControls_;
  std::deque<std::pair<TextIntent,TextReceipt>> textIntents_;
  std::deque<std::pair<TextInputEvent,TextReceipt>> textInputs_;
  std::unordered_map<std::uint64_t,std::uint64_t> textClientHighWater_;
  std::atomic_size_t pendingWork_{0};
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
