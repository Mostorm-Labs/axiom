#include "canvas_runtime_facade_adapter.hpp"

namespace canvas::ink_playground {

runtime::ProductControlReceipt CanvasRuntimeFacadeAdapter::submitCanvasControl(
    const runtime::CanvasControlRequest& request) noexcept {
  auto receipt = service_.enqueue(request);
  if (receipt.state == runtime::CanvasControlReceiptState::kQueued) {
    (void)service_.processPending(request.deadlineSequence == 0U ? request.requestId
                                                                  : request.deadlineSequence);
    if (const auto applied = service_.receipt(receipt.key); applied.has_value()) {
      receipt = *applied;
    }
  }
  const auto applied = receipt.state == runtime::CanvasControlReceiptState::kApplied;
  const auto state = applied ? runtime::ProductControlState::kApplied
      : receipt.state == runtime::CanvasControlReceiptState::kQueued
          ? runtime::ProductControlState::kQueued
          : runtime::ProductControlState::kRejected;
  return {request.requestId, state, request.target.runtimeEpoch, receipt};
}

runtime::CanvasControlSnapshot CanvasRuntimeFacadeAdapter::readCanvasControlSnapshot(
    const runtime::CanvasTargetKey& target) const noexcept {
  return service_.snapshot(target);
}

std::vector<runtime::CanvasTargetKey> CanvasRuntimeFacadeAdapter::mountedCanvasTargets() const {
  return {canvasTarget()};
}

std::vector<runtime::BrushPresetRef> CanvasRuntimeFacadeAdapter::brushPresets() const {
  return {{"vector-solid-v1", 1U}, {"marker-flat-v1", 1U},
          {"chalk-grain-v1", 4U}, {"membrane-v1", 1U}};
}

}  // namespace canvas::ink_playground
