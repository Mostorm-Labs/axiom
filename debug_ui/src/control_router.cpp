#include "canvas/debug_ui/control_router.hpp"

#include <utility>

namespace canvas::debug_ui {
namespace {

template <typename T>
T unsupportedReceipt(std::uint64_t requestId) noexcept;

template <>
canvas::runtime::ProductControlReceipt unsupportedReceipt(std::uint64_t requestId) noexcept {
  return {requestId, canvas::runtime::ProductControlState::kUnsupported, 0, std::nullopt};
}

template <>
canvas::runtime::AxiomDebugCommandReceipt unsupportedReceipt(std::uint64_t requestId) noexcept {
  return {requestId, canvas::runtime::AxiomDebugCommandState::kUnsupported};
}

template <>
canvas::runtime::SurfaceModeReceipt unsupportedReceipt(std::uint64_t requestId) noexcept {
  return {requestId, canvas::runtime::SurfaceControlState::kUnsupported};
}

}  // namespace

DebugControlRouter::DebugControlRouter(canvas::runtime::RuntimeFacade* runtime,
                                       canvas::runtime::AxiomDebugControl* axiomDebug,
                                       canvas::runtime::PlatformDebugControl* platformDebug,
                                       DebugActivityLog* activity) noexcept
    : runtime_(runtime), axiomDebug_(axiomDebug), platformDebug_(platformDebug), activity_(activity) {}

void DebugControlRouter::beginFrame(const DebugSnapshot& snapshot) noexcept {
  frame_.stamp = snapshot.stamp;
  if (snapshot.platform.availability != DebugAvailability::kUnsupported &&
      snapshot.platform.availability != DebugAvailability::kUnavailable &&
      snapshot.platform.value.canonicalSurfaceGeneration != 0) {
    frame_.stamp.surfaceGeneration = snapshot.platform.value.canonicalSurfaceGeneration;
  }
  frame_.selectedObjectCount = snapshot.product.value.selection.selectedObjectCount;
  frame_.primaryObject = snapshot.product.value.selection.primaryObject;
}

std::uint64_t DebugControlRouter::nextRequestId() noexcept {
  if (nextRequest_ == 0) nextRequest_ = 1;
  return nextRequest_++;
}

void DebugControlRouter::recordProduct(const std::string& action,
                                       const canvas::runtime::ProductControlReceipt& receipt) noexcept {
  if (activity_ == nullptr) return;
  try {
    DebugActivityEntry entry{};
    entry.requestId = receipt.requestId;
    entry.owner = DebugControlOwner::kProduct;
    entry.action = action;
    entry.state = receipt.control.has_value() ? mapCanvas(receipt.control->state) : mapProduct(receipt.state);
    entry.runtimeGeneration = receipt.runtimeGeneration;
    entry.documentGeneration = frame_.stamp.documentGeneration;
    entry.surfaceGeneration = frame_.stamp.surfaceGeneration;
    entry.frameId = frame_.stamp.frameId;
    entry.productReceipt = receipt;
    activity_->record(std::move(entry));
  } catch (...) {
  }
}

void DebugControlRouter::recordAxiom(const std::string& action,
                                     const canvas::runtime::AxiomDebugCommandReceipt& receipt) noexcept {
  if (activity_ == nullptr) return;
  try {
    DebugActivityEntry entry{};
    entry.requestId = receipt.requestId;
    entry.owner = DebugControlOwner::kAxiomDebug;
    entry.action = action;
    entry.state = mapAxiom(receipt.state);
    entry.runtimeGeneration = receipt.runtimeGeneration;
    entry.documentGeneration = receipt.documentGeneration;
    entry.surfaceGeneration = frame_.stamp.surfaceGeneration;
    entry.frameId = receipt.appliedFrameId != 0 ? receipt.appliedFrameId : frame_.stamp.frameId;
    activity_->record(std::move(entry));
  } catch (...) {
  }
}

void DebugControlRouter::recordSurface(const std::string& action,
                                        const canvas::runtime::SurfaceModeReceipt& receipt) noexcept {
  if (activity_ == nullptr) return;
  try {
    DebugActivityEntry entry{};
    entry.requestId = receipt.requestId;
    entry.owner = DebugControlOwner::kPlatformDebug;
    entry.action = action;
    entry.state = mapSurface(receipt.state);
    entry.runtimeGeneration = frame_.stamp.runtimeGeneration;
    entry.documentGeneration = frame_.stamp.documentGeneration;
    entry.surfaceGeneration = receipt.generation;
    entry.frameId = frame_.stamp.frameId;
    entry.surfaceReceipt = receipt;
    activity_->record(std::move(entry));
  } catch (...) {
  }
}

canvas::runtime::ProductControlReceipt DebugControlRouter::setTool(
    canvas::runtime::CanvasToolKind tool) noexcept {
  const auto requestId = nextRequestId();
  if (runtime_ == nullptr) {
    auto receipt = unsupportedReceipt<canvas::runtime::ProductControlReceipt>(requestId);
    recordProduct("set-tool", receipt);
    return receipt;
  }
  canvas::runtime::CanvasControlRequest request{};
  request.clientId = 1;
  request.requestId = requestId;
  request.deadlineSequence = deadline();
  const auto targets = runtime_->mountedCanvasTargets();
  request.target = targets.empty()
      ? canvas::runtime::CanvasTargetKey{frame_.stamp.runtimeGeneration, 1,
                                         frame_.stamp.viewGeneration, frame_.stamp.surfaceGeneration}
      : targets.front();
  request.payload.kind = canvas::runtime::CanvasControlPayloadKind::kSelectTool;
  request.payload.tool = tool;
  auto receipt = runtime_->submitCanvasControl(request);
  recordProduct("set-tool", receipt);
  return receipt;
}

canvas::runtime::ProductControlReceipt DebugControlRouter::setBrush(std::uint32_t brushId,
                                                                      std::uint32_t revision) noexcept {
  const auto id = nextRequestId();
  auto receipt = runtime_ == nullptr ? unsupportedReceipt<canvas::runtime::ProductControlReceipt>(id)
                                     : runtime_->setBrush(brushId, revision, id, frame_.stamp.runtimeGeneration);
  recordProduct("set-brush", receipt);
  return receipt;
}

canvas::runtime::ProductControlReceipt DebugControlRouter::setEraser(std::uint32_t eraserId) noexcept {
  const auto id = nextRequestId();
  auto receipt = runtime_ == nullptr ? unsupportedReceipt<canvas::runtime::ProductControlReceipt>(id)
                                     : runtime_->setEraser(eraserId, id, frame_.stamp.runtimeGeneration);
  recordProduct("set-eraser", receipt);
  return receipt;
}

canvas::runtime::ProductControlReceipt DebugControlRouter::setSelectionMode(bool enabled) noexcept {
  const auto id = nextRequestId();
  auto receipt = runtime_ == nullptr ? unsupportedReceipt<canvas::runtime::ProductControlReceipt>(id)
                                     : runtime_->setSelectionMode(enabled, id, frame_.stamp.runtimeGeneration);
  recordProduct("set-selection-mode", receipt);
  return receipt;
}

canvas::runtime::ProductControlReceipt DebugControlRouter::undo() noexcept {
  const auto id = nextRequestId();
  auto receipt = runtime_ == nullptr ? unsupportedReceipt<canvas::runtime::ProductControlReceipt>(id)
                                     : runtime_->undo(id, frame_.stamp.runtimeGeneration);
  recordProduct("undo", receipt);
  return receipt;
}

canvas::runtime::ProductControlReceipt DebugControlRouter::redo() noexcept {
  const auto id = nextRequestId();
  auto receipt = runtime_ == nullptr ? unsupportedReceipt<canvas::runtime::ProductControlReceipt>(id)
                                     : runtime_->redo(id, frame_.stamp.runtimeGeneration);
  recordProduct("redo", receipt);
  return receipt;
}

canvas::runtime::ProductControlReceipt DebugControlRouter::panBy(float dx, float dy) noexcept {
  const auto id = nextRequestId();
  auto receipt = runtime_ == nullptr ? unsupportedReceipt<canvas::runtime::ProductControlReceipt>(id)
                                     : runtime_->panBy(dx, dy, id, frame_.stamp.runtimeGeneration);
  recordProduct("pan", receipt);
  return receipt;
}

canvas::runtime::ProductControlReceipt DebugControlRouter::zoomAt(float x, float y,
                                                                   float scaleDelta) noexcept {
  const auto id = nextRequestId();
  auto receipt = runtime_ == nullptr ? unsupportedReceipt<canvas::runtime::ProductControlReceipt>(id)
                                     : runtime_->zoomAt(x, y, scaleDelta, id, frame_.stamp.runtimeGeneration);
  recordProduct("zoom", receipt);
  return receipt;
}

canvas::runtime::ProductControlReceipt DebugControlRouter::fitToContent() noexcept {
  const auto id = nextRequestId();
  auto receipt = runtime_ == nullptr ? unsupportedReceipt<canvas::runtime::ProductControlReceipt>(id)
                                     : runtime_->fitToContent(id, frame_.stamp.runtimeGeneration);
  recordProduct("fit-content", receipt);
  return receipt;
}

canvas::runtime::ProductControlReceipt DebugControlRouter::localRejected(
    std::uint64_t requestId) const noexcept {
  return {requestId, canvas::runtime::ProductControlState::kRejected,
          frame_.stamp.runtimeGeneration, std::nullopt};
}

canvas::runtime::ProductControlReceipt DebugControlRouter::fitToSelection() noexcept {
  const auto id = nextRequestId();
  if (frame_.selectedObjectCount == 0) {
    const auto receipt = localRejected(id);
    recordProduct("fit-selection", receipt);
    return receipt;
  }
  auto receipt = runtime_ == nullptr ? unsupportedReceipt<canvas::runtime::ProductControlReceipt>(id)
                                     : runtime_->fitToSelection(id, frame_.stamp.runtimeGeneration);
  recordProduct("fit-selection", receipt);
  return receipt;
}

canvas::runtime::ProductControlReceipt DebugControlRouter::fitPrimaryObject() noexcept {
  const auto id = nextRequestId();
  if (frame_.primaryObject == 0) {
    const auto receipt = localRejected(id);
    recordProduct("fit-primary", receipt);
    return receipt;
  }
  auto receipt = runtime_ == nullptr ? unsupportedReceipt<canvas::runtime::ProductControlReceipt>(id)
                                     : runtime_->fitToObject(frame_.primaryObject, id, frame_.stamp.runtimeGeneration);
  recordProduct("fit-primary", receipt);
  return receipt;
}

canvas::runtime::AxiomDebugCommandReceipt DebugControlRouter::submitAxiom(
    canvas::runtime::AxiomDebugCommandKind kind, std::uint64_t value) noexcept {
  canvas::runtime::AxiomDebugCommand command{};
  command.requestId = nextRequestId();
  command.kind = kind;
  command.expectedRuntimeGeneration = frame_.stamp.runtimeGeneration;
  command.expectedDocumentGeneration = frame_.stamp.documentGeneration;
  command.deadlineSequence = deadline();
  command.value = value;
  const auto receipt = axiomDebug_ == nullptr
      ? unsupportedReceipt<canvas::runtime::AxiomDebugCommandReceipt>(command.requestId)
      : axiomDebug_->enqueue(command);
  recordAxiom("axiom-debug", receipt);
  if (receipt.state == canvas::runtime::AxiomDebugCommandState::kQueued && axiomDebug_ != nullptr)
    pendingAxiom_.emplace(receipt.requestId, PendingAxiom{"axiom-debug"});
  return receipt;
}

canvas::runtime::SurfaceModeReceipt DebugControlRouter::setCanonicalSurfaceMode(
    canvas::runtime::SurfaceMode mode) noexcept {
  canvas::runtime::SurfaceModeRequest request{};
  request.requestId = nextRequestId();
  request.target = canvas::runtime::SurfaceRole::kCanonicalCanvas;
  request.mode = mode;
  request.expectedGeneration = frame_.stamp.surfaceGeneration;
  request.deadlineSequence = deadline();
  if (frame_.stamp.surfaceGeneration == 0) request.expectedGeneration = 0;
  const auto receipt = platformDebug_ == nullptr
      ? unsupportedReceipt<canvas::runtime::SurfaceModeReceipt>(request.requestId)
      : platformDebug_->enqueueSurfaceMode(request);
  recordSurface("surface-mode", receipt);
  if (receipt.state == canvas::runtime::SurfaceControlState::kQueued && platformDebug_ != nullptr)
    pendingSurface_.emplace(receipt.requestId, PendingSurface{"surface-mode"});
  return receipt;
}

void DebugControlRouter::refreshReceipts() noexcept {
  if (axiomDebug_ != nullptr) {
    for (auto it = pendingAxiom_.begin(); it != pendingAxiom_.end();) {
      const auto receipt = axiomDebug_->receipt(it->first);
      if (receipt.state == canvas::runtime::AxiomDebugCommandState::kQueued) {
        ++it;
        continue;
      }
      recordAxiom(it->second.action, receipt);
      it = pendingAxiom_.erase(it);
    }
  }
  if (platformDebug_ != nullptr) {
    for (auto it = pendingSurface_.begin(); it != pendingSurface_.end();) {
      const auto receipt = platformDebug_->receipt(it->first);
      if (receipt.state == canvas::runtime::SurfaceControlState::kQueued) {
        ++it;
        continue;
      }
      recordSurface(it->second.action, receipt);
      it = pendingSurface_.erase(it);
    }
  }
}

DebugActivityState DebugControlRouter::mapProduct(canvas::runtime::ProductControlState state) noexcept {
  switch (state) {
    case canvas::runtime::ProductControlState::kApplied: return DebugActivityState::kApplied;
    case canvas::runtime::ProductControlState::kQueued: return DebugActivityState::kPending;
    case canvas::runtime::ProductControlState::kUnsupported: return DebugActivityState::kUnsupported;
    case canvas::runtime::ProductControlState::kRejected: return DebugActivityState::kRejected;
    case canvas::runtime::ProductControlState::kFailed: return DebugActivityState::kFailed;
  }
  return DebugActivityState::kFailed;
}

DebugActivityState DebugControlRouter::mapCanvas(canvas::runtime::CanvasControlReceiptState state) noexcept {
  using S = canvas::runtime::CanvasControlReceiptState;
  switch (state) {
    case S::kQueued: return DebugActivityState::kPending;
    case S::kApplied: return DebugActivityState::kApplied;
    case S::kExpired: return DebugActivityState::kExpired;
    case S::kStaleTarget: return DebugActivityState::kStaleGeneration;
    case S::kTargetDestroyed: return DebugActivityState::kUnavailable;
    case S::kQueueFull: return DebugActivityState::kQueueFull;
    case S::kBusy: return DebugActivityState::kUnavailable;
    case S::kUnsupported: return DebugActivityState::kUnsupported;
    case S::kRejected: case S::kDuplicate: return DebugActivityState::kRejected;
    case S::kFailed: return DebugActivityState::kFailed;
  }
  return DebugActivityState::kFailed;
}

DebugActivityState DebugControlRouter::mapAxiom(canvas::runtime::AxiomDebugCommandState state) noexcept {
  using S = canvas::runtime::AxiomDebugCommandState;
  switch (state) {
    case S::kQueued: return DebugActivityState::kPending;
    case S::kApplied: return DebugActivityState::kApplied;
    case S::kRejected: return DebugActivityState::kRejected;
    case S::kFailed: return DebugActivityState::kFailed;
    case S::kStaleGeneration: return DebugActivityState::kStaleGeneration;
    case S::kExpired: return DebugActivityState::kExpired;
    case S::kDestroyed: return DebugActivityState::kUnavailable;
    case S::kQueueFull: return DebugActivityState::kQueueFull;
    case S::kUnsupported: return DebugActivityState::kUnsupported;
  }
  return DebugActivityState::kFailed;
}

DebugActivityState DebugControlRouter::mapSurface(canvas::runtime::SurfaceControlState state) noexcept {
  using S = canvas::runtime::SurfaceControlState;
  switch (state) {
    case S::kQueued: return DebugActivityState::kPending;
    case S::kApplied: return DebugActivityState::kApplied;
    case S::kUnsupported: return DebugActivityState::kUnsupported;
    case S::kStaleGeneration: return DebugActivityState::kStaleGeneration;
    case S::kQueueFull: return DebugActivityState::kQueueFull;
    case S::kExpired: return DebugActivityState::kExpired;
    case S::kUnavailable: return DebugActivityState::kUnavailable;
    case S::kFailed: return DebugActivityState::kFailed;
  }
  return DebugActivityState::kFailed;
}

}  // namespace canvas::debug_ui
