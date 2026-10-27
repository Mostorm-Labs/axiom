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

canvas::runtime::SectionSnapshot DebugControlRouter::sectionSnapshot() const {
  return runtime_?runtime_->readSection():canvas::runtime::SectionSnapshot{};
}
canvas::runtime::ProductControlReceipt DebugControlRouter::sectionIntent(
    const canvas::runtime::SectionIntent& intent) noexcept {
  const auto request=nextRequestId();
  const auto receipt=runtime_?runtime_->submitSectionIntent(intent,request,0xD001U):
      unsupportedReceipt<canvas::runtime::ProductControlReceipt>(request);
  recordProduct("Section",receipt);
  if(receipt.state==canvas::runtime::ProductControlState::kQueued&&receipt.control)
    pendingProduct_.emplace(receipt.control->key,"Section");
  return receipt;
}

std::uint64_t DebugControlRouter::productDeadline() const noexcept {
  // Product/Canvas requests are consumed by the Runtime owner sequence. The
  // UI snapshot sequence is only a UI observation counter and can lag far
  // behind after the panel was hidden while the owner kept pumping.
  const auto ownerSequence = runtime_ == nullptr ? 0U : runtime_->controlSequence();
  return (ownerSequence != 0U ? ownerSequence : frame_.stamp.sequence) + 120U;
}

void DebugControlRouter::beginFrame(const DebugSnapshot& snapshot) noexcept {
  frame_.stamp = snapshot.stamp;
  if (snapshot.platform.availability != DebugAvailability::kUnsupported &&
      snapshot.platform.availability != DebugAvailability::kUnavailable &&
      snapshot.platform.value.canonicalSurfaceGeneration != 0) {
    frame_.stamp.surfaceGeneration = snapshot.platform.value.canonicalSurfaceGeneration;
  }
  const auto& summary=snapshot.selectionSummary;
  frame_.selectionRevision=summary.selectionRevision;frame_.semanticGeneration=summary.semanticGeneration;frame_.sceneRevision=summary.sceneRevision;
  frame_.actions=summary.actions;
}

bool DebugControlRouter::canSubmitSelection(canvas::runtime::SelectionAction action) const noexcept {
  const auto i=static_cast<std::size_t>(action);
  if(!runtime_||i>=frame_.actions.size()||!frame_.actions[i].available())return false;
  try {
    const auto current=runtime_->readSelectionSummary();
    return current.status==canvas::runtime::SelectionSummaryStatus::kAvailable&&current.capability(action).available()&&
      current.selectionRevision==frame_.selectionRevision&&current.semanticGeneration==frame_.semanticGeneration&&current.sceneRevision==frame_.sceneRevision;
  } catch(const std::bad_alloc&) { return false; }
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

namespace {
canvas::runtime::ProductControlState productStateFromCanvas(
    canvas::runtime::CanvasControlReceiptState state) noexcept {
  return state == canvas::runtime::CanvasControlReceiptState::kApplied
      ? canvas::runtime::ProductControlState::kApplied
      : state == canvas::runtime::CanvasControlReceiptState::kQueued
          ? canvas::runtime::ProductControlState::kQueued
          : canvas::runtime::ProductControlState::kRejected;
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
  request.clientId = 0xD001U;
  request.requestId = requestId;
  request.deadlineSequence = productDeadline();
  const auto targets = runtime_->mountedCanvasTargets();
  request.target = targets.empty()
      ? canvas::runtime::CanvasTargetKey{frame_.stamp.runtimeGeneration, 1,
                                         frame_.stamp.viewGeneration, frame_.stamp.surfaceGeneration}
      : targets.front();
  request.payload.kind = canvas::runtime::CanvasControlPayloadKind::kSelectTool;
  request.payload.tool = tool;
  auto receipt = runtime_->submitCanvasControl(request);
  recordProduct("set-tool", receipt);
  if (receipt.state == canvas::runtime::ProductControlState::kQueued &&
      receipt.control.has_value()) pendingProduct_.emplace(receipt.control->key, "set-tool");
  return receipt;
}

canvas::runtime::ProductControlReceipt DebugControlRouter::setBrush(std::uint32_t brushId,
                                                                      std::uint32_t revision) noexcept {
  const auto id = nextRequestId();
  auto receipt = runtime_ == nullptr ? unsupportedReceipt<canvas::runtime::ProductControlReceipt>(id)
                                     : runtime_->setBrush(brushId, revision, id, frame_.stamp.runtimeGeneration, 0xD001U);
  recordProduct("set-brush", receipt);
  if (receipt.state == canvas::runtime::ProductControlState::kQueued && receipt.control)
    pendingProduct_.emplace(receipt.control->key, "set-brush");
  return receipt;
}

canvas::runtime::ProductControlReceipt DebugControlRouter::setEraser(std::uint32_t eraserId) noexcept {
  const auto id = nextRequestId();
  auto receipt = runtime_ == nullptr ? unsupportedReceipt<canvas::runtime::ProductControlReceipt>(id)
                                     : runtime_->setEraser(eraserId, id, frame_.stamp.runtimeGeneration, 0xD001U);
  recordProduct("set-eraser", receipt);
  if (receipt.state == canvas::runtime::ProductControlState::kQueued && receipt.control)
    pendingProduct_.emplace(receipt.control->key, "set-eraser");
  return receipt;
}

canvas::runtime::ProductControlReceipt DebugControlRouter::setSelectionMode(bool enabled) noexcept {
  const auto id = nextRequestId();
  auto receipt = runtime_ == nullptr ? unsupportedReceipt<canvas::runtime::ProductControlReceipt>(id)
                                     : runtime_->setSelectionMode(enabled, id, frame_.stamp.runtimeGeneration, 0xD001U);
  recordProduct("set-selection-mode", receipt);
  if (receipt.state == canvas::runtime::ProductControlState::kQueued && receipt.control)
    pendingProduct_.emplace(receipt.control->key, "set-selection-mode");
  return receipt;
}

canvas::runtime::ProductControlReceipt DebugControlRouter::setSelectionAcquisitionMode(canvas::runtime::CanvasSelectionMode mode) noexcept {
  const auto id=nextRequestId();
  auto receipt=runtime_==nullptr?unsupportedReceipt<canvas::runtime::ProductControlReceipt>(id):
      runtime_->setSelectionAcquisitionMode(mode,id,0xD001U);
  recordProduct("selection-acquisition",receipt);
  if(receipt.state==canvas::runtime::ProductControlState::kQueued&&receipt.control)
      pendingProduct_.emplace(receipt.control->key,"selection-acquisition");
  return receipt;
}

canvas::runtime::ProductControlReceipt DebugControlRouter::undo() noexcept {
  const auto id = nextRequestId();
  auto receipt = runtime_ == nullptr ? unsupportedReceipt<canvas::runtime::ProductControlReceipt>(id)
                                     : runtime_->undo(id, frame_.stamp.runtimeGeneration, 0xD001U);
  recordProduct("undo", receipt);
  if (receipt.state == canvas::runtime::ProductControlState::kQueued && receipt.control)
    pendingProduct_.emplace(receipt.control->key, "undo");
  return receipt;
}

canvas::runtime::ProductControlReceipt DebugControlRouter::redo() noexcept {
  const auto id = nextRequestId();
  auto receipt = runtime_ == nullptr ? unsupportedReceipt<canvas::runtime::ProductControlReceipt>(id)
                                     : runtime_->redo(id, frame_.stamp.runtimeGeneration, 0xD001U);
  recordProduct("redo", receipt);
  if (receipt.state == canvas::runtime::ProductControlState::kQueued && receipt.control)
    pendingProduct_.emplace(receipt.control->key, "redo");
  return receipt;
}

canvas::runtime::ProductControlReceipt DebugControlRouter::setCreationPreset(
    canvas::runtime::CanvasCreationPreset preset) noexcept {
  const auto requestId = nextRequestId();
  if (runtime_ == nullptr) {
    auto receipt = unsupportedReceipt<canvas::runtime::ProductControlReceipt>(requestId);
    recordProduct("set-creation-preset", receipt);
    return receipt;
  }
  const auto targets = runtime_->mountedCanvasTargets();
  canvas::runtime::CanvasControlRequest request{};
  request.clientId = 0xD001U;
  request.requestId = requestId;
  request.deadlineSequence = productDeadline();
  request.target = targets.empty()
      ? canvas::runtime::CanvasTargetKey{frame_.stamp.runtimeGeneration, 1U,
                                         frame_.stamp.viewGeneration,
                                         frame_.stamp.surfaceGeneration}
      : targets.front();
  request.payload.kind = canvas::runtime::CanvasControlPayloadKind::kSelectCreationPreset;
  request.payload.creationPreset = preset;
  auto receipt = runtime_->submitCanvasControl(request);
  recordProduct("set-creation-preset", receipt);
  if (receipt.state == canvas::runtime::ProductControlState::kQueued && receipt.control)
    pendingProduct_.emplace(receipt.control->key, "set-creation-preset");
  return receipt;
}

canvas::runtime::StickyCommandReceipt DebugControlRouter::createSticky() noexcept {
  canvas::runtime::StickyCommandReceipt receipt{};
  if (runtime_ == nullptr) {
    connectorStickyStatus_ = "Sticky: Runtime owner unavailable";
    return receipt;
  }
  const auto state = runtime_->readRuntimeState();
  const auto id = canvas::foundation::ObjectId::fromUint64(
      (nextRequestId() << 8U) ^ 0x535449434B59ULL);
  canvas::runtime::StickyCreateRequest request{};
  request.objectId = id;
  request.transform.tx = (450.0 - state.camera.translationX) /
                         (state.camera.scale > 0.0 ? state.camera.scale : 1.0);
  request.transform.ty = (140.0 - state.camera.translationY) /
                         (state.camera.scale > 0.0 ? state.camera.scale : 1.0);
  receipt = runtime_->createSticky(request);
  connectorStickyStatus_ = receipt.state == canvas::runtime::ConnectorCommandState::kApplied
      ? "Sticky: created" : "Sticky: rejected";
  return receipt;
}

canvas::runtime::ConnectorCommandReceipt DebugControlRouter::createConnector() noexcept {
  canvas::runtime::ConnectorCommandReceipt receipt{};
  if (runtime_ == nullptr) {
    connectorStickyStatus_ = "Connector: Runtime owner unavailable";
    return receipt;
  }
  const auto state = runtime_->readRuntimeState();
  const auto id = canvas::foundation::ObjectId::fromUint64(
      (nextRequestId() << 8U) ^ 0x434F4E4EULL);
  canvas::runtime::ConnectorCreateRequest request{};
  request.objectId = id;
  request.content.routing=runtime_->readConnectorRoutingPreset();
  request.content.start.value = canvas::semantic::FreePointEndpoint{{80.0, 140.0}};
  std::optional<canvas::foundation::ObjectId> selectedObject;
  const auto targets = runtime_->mountedCanvasTargets();
  if (!targets.empty()) {
    selectedObject = runtime_->readCanvasControlSnapshot(targets.front()).selectedObject;
  }
  if (selectedObject.has_value() && !selectedObject->isZero()) {
    request.content.end.value = canvas::semantic::AttachedEndpoint{
        *selectedObject,
        canvas::semantic::AutoPerimeterAnchor{}};
  } else {
    request.content.end.value = canvas::semantic::FreePointEndpoint{{240.0, 140.0}};
  }
  request.transform.tx = -state.camera.translationX /
                         (state.camera.scale > 0.0 ? state.camera.scale : 1.0);
  request.transform.ty = -state.camera.translationY /
                         (state.camera.scale > 0.0 ? state.camera.scale : 1.0);
  receipt = runtime_->createConnector(request);
  connectorStickyStatus_ = receipt.state == canvas::runtime::ConnectorCommandState::kApplied
      ? "Connector: created" : "Connector: rejected";
  return receipt;
}

canvas::runtime::StickySnapshot DebugControlRouter::selectedSticky() const {
  return runtime_ ? runtime_->readSelectedSticky() : canvas::runtime::StickySnapshot{};
}

void DebugControlRouter::prepareConnectorDefaults(std::uint64_t epoch, bool retry) {
  if (!runtime_ || epoch == 0 || (!retry && connectorDefaultsEpoch_ == epoch)) return;
  connectorDefaultsEpoch_ = epoch;
  connectorDefaultsReady_ = false;
  if (runtime_->readConnectorRoutingPreset() == canvas::semantic::ConnectorRouting::kOrthogonal) {
    connectorDefaultsReady_ = true;
    return;
  }
  canvas::runtime::ConnectorRoutingRequest request{};
  request.routing = canvas::semantic::ConnectorRouting::kOrthogonal;
  const auto receipt = runtime_->setConnectorRouting(request);
  connectorDefaultsReady_ = receipt.state == canvas::runtime::ConnectorCommandState::kApplied &&
      runtime_->readConnectorRoutingPreset() == request.routing;
  if (!connectorDefaultsReady_) connectorStickyStatus_ = "Connector setup failed; retry when Runtime is ready";
}
canvas::runtime::CanvasToolKind DebugControlRouter::canvasTool() const {
  if (runtime_) {
    const auto targets = runtime_->mountedCanvasTargets();
    if (!targets.empty()) return runtime_->readCanvasControlSnapshot(targets.front()).tool;
  }
  return canvas::runtime::CanvasToolKind::kInk;
}

canvas::runtime::ConnectorCommandReceipt DebugControlRouter::setConnectorRouting(
    canvas::semantic::ConnectorRouting routing, bool selected) noexcept {
  if(!runtime_)return {};
  canvas::runtime::ConnectorRoutingRequest request{};request.routing=routing;
  if(selected) {
    const auto connector=runtime_->readSelectedConnector();
    if(!connector.available)return {};
    request.objectId=connector.objectId;
  }
  const auto receipt=runtime_->setConnectorRouting(request);
  connectorStickyStatus_=receipt.state==canvas::runtime::ConnectorCommandState::kApplied
      ? "Connector routing: applied" : "Connector routing: rejected (no route / invalid target / budget)";
  if (activity_) {
    DebugActivityEntry entry{};
    entry.action = selected ? "Convert connector to orthogonal" : "New connector routing";
    entry.state = receipt.state == canvas::runtime::ConnectorCommandState::kApplied
        ? DebugActivityState::kApplied : DebugActivityState::kRejected;
    activity_->record(std::move(entry));
  }
  return receipt;
}
canvas::semantic::ConnectorRouting DebugControlRouter::connectorRoutingPreset() const {
  return runtime_?runtime_->readConnectorRoutingPreset():canvas::semantic::ConnectorRouting::kStraight;
}

canvas::runtime::ConnectorSnapshot DebugControlRouter::selectedConnector() const {
  return runtime_ ? runtime_->readSelectedConnector() : canvas::runtime::ConnectorSnapshot{};
}

canvas::runtime::ProductControlReceipt DebugControlRouter::panBy(float dx, float dy) noexcept {
  const auto id = nextRequestId();
  auto receipt = runtime_ == nullptr ? unsupportedReceipt<canvas::runtime::ProductControlReceipt>(id)
                                     : runtime_->panBy(dx, dy, id, frame_.stamp.runtimeGeneration, 0xD001U);
  recordProduct("pan", receipt);
  return receipt;
}

canvas::runtime::ProductControlReceipt DebugControlRouter::zoomAt(float x, float y,
                                                                   float scaleDelta) noexcept {
  const auto id = nextRequestId();
  auto receipt = runtime_ == nullptr ? unsupportedReceipt<canvas::runtime::ProductControlReceipt>(id)
                                     : runtime_->zoomAt(x, y, scaleDelta, id, frame_.stamp.runtimeGeneration, 0xD001U);
  recordProduct("zoom", receipt);
  return receipt;
}

canvas::runtime::ProductControlReceipt DebugControlRouter::fitToContent() noexcept {
  const auto id = nextRequestId();
  auto receipt = runtime_ == nullptr ? unsupportedReceipt<canvas::runtime::ProductControlReceipt>(id)
                                     : runtime_->fitToContent(id, frame_.stamp.runtimeGeneration, 0xD001U);
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
  if (!canSubmitSelection(canvas::runtime::SelectionAction::kFit)) {
    const auto receipt = localRejected(id);
    recordProduct("fit-selection", receipt);
    return receipt;
  }
  auto receipt = runtime_ == nullptr ? unsupportedReceipt<canvas::runtime::ProductControlReceipt>(id)
                                     : runtime_->fitToSelection(id, frame_.stamp.runtimeGeneration, 0xD001U);
  recordProduct("fit-selection", receipt);
  return receipt;
}

canvas::runtime::ProductControlReceipt DebugControlRouter::fitPrimaryObject() noexcept {
  const auto id = nextRequestId();
  if (!canSubmitSelection(canvas::runtime::SelectionAction::kFitPrimary)) {
    const auto receipt = localRejected(id);
    recordProduct("fit-primary", receipt);
    return receipt;
  }
  auto receipt = runtime_ == nullptr ? unsupportedReceipt<canvas::runtime::ProductControlReceipt>(id)
                                     : runtime_->fitToSelection(id, frame_.stamp.runtimeGeneration, 0xD001U);
  recordProduct("fit-primary", receipt);
  return receipt;
}

canvas::runtime::ProductControlReceipt DebugControlRouter::selectedAction(
    canvas::runtime::CanvasControlPayloadKind kind, bool flag) noexcept {
  const auto requestId = nextRequestId();
  using A=canvas::runtime::SelectionAction;using K=canvas::runtime::CanvasControlPayloadKind;
  const auto action=kind==K::kDeleteSelected?A::kDelete:kind==K::kSelectedOrder?A::kOrder:
      kind==K::kUnlockSelected?A::kUnlock:kind==K::kSelectedLock?(flag?A::kLock:A::kUnlock):A::kCount;
  if (!canSubmitSelection(action)) {
    const auto receipt = localRejected(requestId);
    recordProduct("selected-action", receipt);
    return receipt;
  }
  const auto targets = runtime_->mountedCanvasTargets();
  if(targets.empty()||targets.front().runtimeEpoch!=frame_.stamp.runtimeGeneration) {
    const auto receipt=localRejected(requestId);recordProduct("selected-action",receipt);return receipt;
  }
  canvas::runtime::CanvasControlRequest request{};
  request.clientId = 0xD001U;
  request.requestId = requestId;
  request.deadlineSequence = productDeadline();
  request.target = targets.empty()
      ? canvas::runtime::CanvasTargetKey{frame_.stamp.runtimeGeneration, 1U,
                                         frame_.stamp.viewGeneration,
                                         frame_.stamp.surfaceGeneration}
      : targets.front();
  request.payload.kind = kind;
  request.payload.flag = flag;
  auto receipt = runtime_->submitCanvasControl(request);
  recordProduct("selected-action", receipt);
  return receipt;
}

canvas::runtime::AxiomDebugCommandReceipt DebugControlRouter::submitAxiom(
    canvas::runtime::AxiomDebugCommandKind kind, std::uint64_t value) noexcept {
  canvas::runtime::AxiomDebugCommand command{};
  command.requestId = nextRequestId();
  command.kind = kind;
  command.expectedRuntimeGeneration = frame_.stamp.runtimeGeneration;
  command.expectedDocumentGeneration = frame_.stamp.documentGeneration;
  command.deadlineSequence = uiDeadline();
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
  request.deadlineSequence = uiDeadline();
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
  if (runtime_ != nullptr) {
    for (auto it = pendingText_.begin(); it != pendingText_.end();) {
      const auto receipt = runtime_->textIntentReceipt(it->second.client, it->first);
      if (!receipt || receipt->status == canvas::runtime::TextStatus::kQueued ||
          receipt->status == canvas::runtime::TextStatus::kObserved) { ++it; continue; }
      recordText(it->second.action.c_str(), *receipt);
      it = pendingText_.erase(it);
    }
    for (auto it = pendingProduct_.begin(); it != pendingProduct_.end();) {
      const auto terminal = runtime_->canvasControlReceipt(it->first);
      if (!terminal.has_value() || terminal->state == canvas::runtime::CanvasControlReceiptState::kQueued) {
        ++it;
        continue;
      }
      canvas::runtime::ProductControlReceipt receipt{};
      receipt.requestId = it->first.requestId;
      receipt.runtimeGeneration = frame_.stamp.runtimeGeneration;
      receipt.control = *terminal;
      receipt.state = productStateFromCanvas(terminal->state);
      recordProduct(it->second, receipt);
      it = pendingProduct_.erase(it);
    }
  }
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

namespace canvas::debug_ui {
canvas::runtime::SelectedImageSnapshot DebugControlRouter::selectedImage() const {
 return runtime_ ? runtime_->readSelectedImage() : canvas::runtime::SelectedImageSnapshot{};
}
canvas::runtime::ImageCropSnapshot DebugControlRouter::imageCrop() const {
 return runtime_ ? runtime_->readImageCrop() : canvas::runtime::ImageCropSnapshot{};
}
void DebugControlRouter::requestImageFile(bool replace) {
 if (!runtime_ || imageFileRequest_ || imageCrop().active) return;
 const auto targets = runtime_->mountedCanvasTargets(); if (targets.empty()) return;
 const auto selected = selectedImage();
 if (replace && (!selected.available || !selected.editable)) return;
 imageFileRequest_ = ImageFileRequest{replace, selected, targets.front(), nextRequestId(), false};
 imageStatus_ = "Choose a PNG, JPEG or WebP file";
}
void DebugControlRouter::processImageFileRequest(const std::function<ImageFileResult()>& chooseFile) {
 const auto token = beginImageFileRequest();
 if (!token) return;
 ImageFileResult file;
 if (chooseFile) file = chooseFile();
 else file.error = "File picker unavailable in this host";
 (void)completeImageFileRequest(token, file);
}
std::uint64_t DebugControlRouter::beginImageFileRequest() {
 if (!imageFileRequest_ || imageFileRequest_->dispatched) return 0;
 imageFileRequest_->dispatched = true;
 return imageFileRequest_->token;
}
bool DebugControlRouter::completeImageFileRequest(std::uint64_t token, const ImageFileResult& file) {
 if (!imageFileRequest_ || !imageFileRequest_->dispatched || imageFileRequest_->token != token) return false;
 const auto request = *imageFileRequest_; imageFileRequest_.reset();
 if (file.cancelled) { imageStatus_ = "File selection cancelled"; return true; }
 if (!file.error.empty()) { imageStatus_ = file.error; return true; }
 const auto targets = runtime_->mountedCanvasTargets();
 if (targets.empty() || targets.front() != request.target) { imageStatus_ = "Image request rejected: binding changed"; return true; }
 if (request.replace) {
   const auto current = selectedImage();
   if (!current.editable || current.objectId != request.selected.objectId || current.content != request.selected.content) {
     imageStatus_ = "Image request rejected: selection changed"; return true;
   }
   canvas::runtime::ImageReplaceRequest r; r.objectId = request.selected.objectId;
   r.bytes = file.bytes; r.mimeType = file.mimeType;
   r.content = request.selected.content; r.content.source_rect.reset();
   recordImage("Replace image", runtime_->replaceImage(r));
 } else {
   canvas::runtime::ImageImportRequest r; r.bytes = file.bytes; r.mimeType = file.mimeType;
   const auto camera = runtime_->readRuntimeState().camera;
   const double scale = camera.scale > 0 ? camera.scale : 1;
   r.transform.tx = (450.0 - camera.translationX) / scale;
   r.transform.ty = (100.0 - camera.translationY) / scale;
   recordImage("Import image", runtime_->importImage(r));
 }
 return true;
}
void DebugControlRouter::recordImage(const char* action, const canvas::runtime::ImageCommandReceipt& r) {
 using R = canvas::runtime::ImageFailureReason;
 const char* reason = "Rejected";
 switch (r.reason) {
 case R::kNone: reason = "Applied"; break;
 case R::kWrongThread: reason = "Wrong owner thread"; break;
 case R::kNotReady: reason = "Runtime not ready"; break;
 case R::kInvalidRequest: reason = "Invalid crop or image values"; break;
 case R::kUnknownResource: reason = "Resource missing"; break;
 case R::kResourceRejected: reason = "Resource validation rejected"; break;
 case R::kObjectRejected: reason = "Object is unavailable, locked or conflicting"; break;
 case R::kInvalidCropSession: reason = "No valid crop session"; break;
 case R::kStaleCropSession: reason = "Crop session expired"; break;
 case R::kUnsupportedFormat: reason = "Unsupported image format"; break;
 case R::kMimeMismatch: reason = "File type does not match encoded bytes"; break;
 case R::kDecodeFailed: reason = "Image decode failed"; break;
 }
 imageStatus_ = std::string(action) + ": " + reason;
 if (activity_) activity_->record({0, nextRequestId(), DebugControlOwner::kProduct, imageStatus_,
   r.state == canvas::runtime::ImageCommandState::kApplied ? DebugActivityState::kApplied : DebugActivityState::kRejected,
   frame_.stamp.runtimeGeneration, r.semanticGeneration, frame_.stamp.surfaceGeneration, 0, {}, {}});
}
void DebugControlRouter::beginImageCrop() {
 if (runtime_) recordImage("Crop", runtime_->beginImageCrop({selectedImage().objectId}));
}
void DebugControlRouter::updateImageCrop(const canvas::runtime::ImageCropUpdateRequest& r) {
 if (runtime_) recordImage("Crop preview", runtime_->updateImageCrop(r));
}
void DebugControlRouter::finishImageCrop(bool commit) {
 if (!runtime_) return;
 const auto crop = imageCrop();
 recordImage(commit ? "Crop Done" : "Crop Cancel", commit ? runtime_->commitImageCrop(crop.cropToken) : runtime_->cancelImageCrop(crop.cropToken));
}
void DebugControlRouter::recordText(const char* action, const canvas::runtime::TextReceipt& receipt) {
 if (!activity_) return;
 DebugActivityEntry entry{}; entry.action = action; entry.requestId = receipt.request;
 entry.runtimeGeneration = frame_.stamp.runtimeGeneration;
 entry.documentGeneration = frame_.stamp.documentGeneration;
 using S = canvas::runtime::TextStatus;
 switch (receipt.status) {
 case S::kApplied: entry.state = DebugActivityState::kApplied; break;
 case S::kQueued: case S::kObserved: entry.state = DebugActivityState::kPending; break;
 case S::kUnsupported: entry.state = DebugActivityState::kUnsupported; break;
 case S::kStale: entry.state = DebugActivityState::kStaleGeneration; break;
 case S::kExpired: entry.state = DebugActivityState::kExpired; break;
 case S::kBackpressure: entry.state = DebugActivityState::kQueueFull; break;
 case S::kBusy: entry.state = DebugActivityState::kUnavailable; break;
 default: entry.state = DebugActivityState::kRejected; break;
 }
 activity_->record(std::move(entry));
}
canvas::runtime::TextReceipt DebugControlRouter::setTextFormat(const canvas::runtime::TextFormatPatch& patch){
 if(!runtime_ || runtime_->mountedCanvasTargets().empty())return {};
 canvas::runtime::TextIntent request;request.kind=canvas::runtime::TextIntentKind::kFormat;request.clientId=0xD003;request.requestId=nextRequestId();request.target=runtime_->mountedCanvasTargets().front();const auto editor=runtime_->readTextEditor();request.token=editor.token;request.object=editor.formatObject;request.format=patch;
 const auto receipt=runtime_->submitTextIntent(request); recordText("text-format", receipt);
 if (receipt.status==canvas::runtime::TextStatus::kQueued || receipt.status==canvas::runtime::TextStatus::kObserved)
   pendingText_.emplace(request.requestId, PendingText{request.clientId,"text-format"});
 return receipt;
}
canvas::runtime::TextEditorSnapshot DebugControlRouter::textEditor() const {return runtime_?runtime_->readTextEditor():canvas::runtime::TextEditorSnapshot{};}
canvas::runtime::TextReceipt DebugControlRouter::setTextWidth(double width){
 if(!runtime_ || runtime_->mountedCanvasTargets().empty())return {};
 canvas::runtime::TextIntent request;request.kind=canvas::runtime::TextIntentKind::kSetWidth;request.clientId=0xD002;request.requestId=nextRequestId();request.target=runtime_->mountedCanvasTargets().front();request.token=runtime_->readTextEditor().token;request.width=width;
 const auto receipt=runtime_->submitTextIntent(request); recordText("text-width", receipt);
 if (receipt.status==canvas::runtime::TextStatus::kQueued || receipt.status==canvas::runtime::TextStatus::kObserved)
   pendingText_.emplace(request.requestId, PendingText{request.clientId,"text-width"});
 return receipt;
}
}
