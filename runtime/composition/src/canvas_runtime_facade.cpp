#include "canvas/runtime/canvas_runtime_facade.hpp"
#include "canvas/semantic/property_value.hpp"

namespace canvas::runtime {

struct CanvasRuntimeFacade::Binding final : public CanvasControlPort {
 public:
  explicit Binding(interaction::CanvasEditingRuntime& runtime) noexcept : runtime_(runtime) {}

  bool isTargetCurrent(const CanvasTargetKey& target) const noexcept override {
    return !runtime_.closed() && target == target_;
  }

  CanvasControlApplyResult apply(const CanvasControlRequest& request) override {
    if (std::this_thread::get_id() != ownerThread_ || runtime_.closed())
      return {CanvasControlError::kPermissionDenied, "owner or lifecycle admission failed"};
    if (runtime_.closed()) return {CanvasControlError::kPermissionDenied, "runtime closed"};
    if (!isTargetCurrent(request.target)) return {CanvasControlError::kStaleTarget, "stale target"};
    auto& runtime = runtime_;
    const auto& p = request.payload;
    bool applied = false;
    switch (p.kind) {
      case CanvasControlPayloadKind::kSelectTool:
        if (p.tool == CanvasToolKind::kSelect) applied = runtime.setSelectionMode(true);
        else if (p.tool == CanvasToolKind::kPan) applied = runtime.setPanTool(true);
        else if (p.tool == CanvasToolKind::kEraser)
          applied = runtime.setSelectionMode(false) && runtime.setPanTool(false) &&
                    runtime.selectTool(p.eraser.mode.kind == OptionPatchKind::kSet &&
                        p.eraser.mode.value == 1U
                        ? canvas::interaction::CanvasEditingRuntime::ToolMode::kObjectEraser
                        : canvas::interaction::CanvasEditingRuntime::ToolMode::kPartialEraser);
        else
          applied = runtime.setSelectionMode(false) && runtime.setPanTool(false) &&
                    runtime.selectTool(canvas::interaction::CanvasEditingRuntime::ToolMode::kBrush);
        break;
      case CanvasControlPayloadKind::kSelectPreset:
        applied = runtime.setSelectionMode(false) && runtime.setPanTool(false) &&
                  runtime.selectTool(canvas::interaction::CanvasEditingRuntime::ToolMode::kBrush) &&
                  runtime.selectBrushProfile(p.preset.profileId, p.preset.revision);
        break;
      case CanvasControlPayloadKind::kPanBy:
        applied = runtime.applyViewportNavigation({canvas::interaction::ViewportNavigationKind::kWheelPan,
                                                    p.x, p.y, 0.0F, 0.0F, 1.0F});
        break;
      case CanvasControlPayloadKind::kZoomAt:
      case CanvasControlPayloadKind::kSetZoom:
        applied = p.scale > 0.0F && runtime.setViewportZoomAt(
            p.scale, p.x == 0.0F ? runtime.surface().logicalWidth * 0.5F : p.x,
            p.y == 0.0F ? runtime.surface().logicalHeight * 0.5F : p.y);
        break;
      case CanvasControlPayloadKind::kFit:
        applied = p.fitTarget == CanvasFitTarget::kSelection ? runtime.fitViewportToSelection()
            : p.fitTarget == CanvasFitTarget::kObject ? runtime.fitViewportToObject(p.objectId)
            : p.fitTarget == CanvasFitTarget::kWorldRect
                ? runtime.fitViewportToContent(canvas::foundation::WorldRect{
                    p.worldRect[0], p.worldRect[1], p.worldRect[2], p.worldRect[3]})
                : runtime.fitViewportToContent();
        break;
      case CanvasControlPayloadKind::kUndo: applied = runtime.undo(); break;
      case CanvasControlPayloadKind::kRedo: applied = runtime.redo(); break;
      case CanvasControlPayloadKind::kSelectCreationPreset:
        applied = runtime.setSelectionMode(false) && runtime.setPanTool(false) &&
                  runtime.selectCreationPreset(static_cast<canvas::interaction::CreationPreset>(p.creationPreset));
        break;
      case CanvasControlPayloadKind::kFutureAppearance: applied = runtime.setFutureAppearance(p.appearance); break;
      case CanvasControlPayloadKind::kSelectedAppearance: applied = runtime.setSelectedAppearance(p.appearance); break;
      case CanvasControlPayloadKind::kSelectedSize: applied = runtime.setSelectedObjectSize(p.width, p.height); break;
      case CanvasControlPayloadKind::kSelectedTransform:
        applied = runtime.setSelectedTransform({p.transform[0], p.transform[1], p.transform[2],
                                                   p.transform[3], p.transform[4], p.transform[5]});
        break;
      case CanvasControlPayloadKind::kSelectedOrder: applied = runtime.setSelectedOrder(p.flag); break;
      case CanvasControlPayloadKind::kSelectedLock: applied = runtime.patchSelectedProperty(2U, semantic::PropertyValue{p.flag}); break;
      case CanvasControlPayloadKind::kSelectedVisible: applied = runtime.patchSelectedProperty(1U, semantic::PropertyValue{p.flag}); break;
      case CanvasControlPayloadKind::kDeleteSelected: applied = runtime.deleteSelectedObjects(); break;
      case CanvasControlPayloadKind::kUnlockSelected: applied = runtime.unlockSelectedObjects(); break;
      case CanvasControlPayloadKind::kInkOptions: {
        const auto defaults = runtime.brushCatalogDefaults(runtime.selectedBrushProfile(), runtime.selectedBrushRevision());
        if (!defaults) break;
        const auto color = runtime.inkColor();
        const auto size = p.ink.size.kind == OptionPatchKind::kSet ? p.ink.size.value
            : p.ink.size.kind == OptionPatchKind::kClearOverride ? defaults->size : runtime.inkSize();
        const auto opacity = p.ink.opacity.kind == OptionPatchKind::kSet ? p.ink.opacity.value
            : p.ink.opacity.kind == OptionPatchKind::kClearOverride ? defaults->opacity : runtime.inkOpacity();
        const auto nextColor = p.ink.color.kind == OptionPatchKind::kSet
            ? p.ink.color.value
            : p.ink.color.kind == OptionPatchKind::kClearOverride
                ? RgbaColor{defaults->red, defaults->green, defaults->blue, defaults->alpha}
                : RgbaColor{color[0], color[1], color[2], color[3]};
        applied = runtime.setInkOptions(size, nextColor.r, nextColor.g, nextColor.b,
                                        nextColor.a, opacity);
        if (applied) runtime.setInkOverrideState(
            p.ink.size.kind == OptionPatchKind::kSet ? true : p.ink.size.kind == OptionPatchKind::kClearOverride ? false : runtime.inkSizeOverridden(),
            p.ink.color.kind == OptionPatchKind::kSet ? true : p.ink.color.kind == OptionPatchKind::kClearOverride ? false : runtime.inkColorOverridden(),
            p.ink.opacity.kind == OptionPatchKind::kSet ? true : p.ink.opacity.kind == OptionPatchKind::kClearOverride ? false : runtime.inkOpacityOverridden());
        break;
      }
      case CanvasControlPayloadKind::kEraserOptions:
        applied = runtime.setEraserOptions(
            p.eraser.mode.kind == OptionPatchKind::kSet ? p.eraser.mode.value : runtime.eraserMode(),
            p.eraser.diameterLogicalPx.kind == OptionPatchKind::kSet
                ? p.eraser.diameterLogicalPx.value : runtime.eraserDiameterLogicalPx());
        break;
    }
    if (applied) { ++revision_; return {}; }
    return CanvasControlApplyResult{CanvasControlError::kBusy, "runtime rejected request"};
  }

  CanvasControlSnapshot snapshot(const CanvasTargetKey& target) const override {
    CanvasControlSnapshot out{};
    out.target = target;
    out.controlRevision = revision_;
    const auto& runtime = runtime_;
    out.targetAvailable = runtime.surface().available;
    out.writable = out.targetAvailable && !runtime.closed();
    out.viewportWidth = runtime.surface().logicalWidth;
    out.viewportHeight = runtime.surface().logicalHeight;
    out.cameraScale = runtime.viewportGesture().scale;
    out.cameraTranslationX = runtime.viewportGesture().translationX;
    out.cameraTranslationY = runtime.viewportGesture().translationY;
    out.canUndo = runtime.canUndo(); out.canRedo = runtime.canRedo();
    out.semanticGeneration = runtime.semanticGeneration().value();
    out.selectedObjectCount = static_cast<std::uint32_t>(runtime.selectedObjectCount());
    if (out.selectedObjectCount != 0U) out.selectedObject = runtime.selectedPrimaryObject();
    out.creationPreset = static_cast<CanvasCreationPreset>(runtime.creationPreset());
    out.creationActive = runtime.creationActive();
    out.creationPreviewValid = runtime.creationPreview().valid;
    out.creationSnapped = runtime.creationPreview().snapped;
    out.creationMode = runtime.creationMode();
    out.futureAppearance = runtime.futureAppearance();
    const auto outcome = runtime.operationOutcome();
    if (outcome.applied) {
      out.lastAppliedOperation = outcome.applied->operation_id.value();
      out.appliedCommitEpoch = outcome.applied->commit_stamp.runtime_epoch.value();
      out.appliedCommitOrdinal = outcome.applied->commit_stamp.ordinal.value();
    }
    out.presentedSemanticGeneration = outcome.presentedGeneration.value();
    out.axisLocalDurable = static_cast<std::uint8_t>(outcome.axis.localDurable);
    out.axisCloudSynced = static_cast<std::uint8_t>(outcome.axis.cloudSynced);
    out.preset = {runtime.selectedBrushProfile(), runtime.selectedBrushRevision()};
    out.size = runtime.inkSize(); out.opacity = runtime.inkOpacity();
    const auto color = runtime.inkColor(); out.color = {color[0], color[1], color[2], color[3]};
    out.eraserMode = runtime.eraserMode(); out.eraserDiameterLogicalPx = runtime.eraserDiameterLogicalPx();
    out.tool = runtime.selectionMode() ? CanvasToolKind::kSelect
        : runtime.panTool() ? CanvasToolKind::kPan
        : runtime.toolMode() == canvas::interaction::CanvasEditingRuntime::ToolMode::kObjectEraser ||
          runtime.toolMode() == canvas::interaction::CanvasEditingRuntime::ToolMode::kPartialEraser
            ? CanvasToolKind::kEraser : CanvasToolKind::kInk;
    if (const auto record = runtime.selectedObjectRecord()) {
      out.selectedKind = static_cast<std::uint32_t>(record->kind);
      out.selectedVersion = record->kind_version;
      out.selectedLocked = runtime.selectedObjectLocked();
      const auto& transform = record->transform;
      out.selectedTransform = {transform.a, transform.b, transform.c, transform.d,
                               transform.tx, transform.ty};
      out.selectedVisible = true;
      out.selectedAppearance.fillEnabled = false;
      out.selectedAppearance.strokeEnabled = false;
      out.selectedAppearance.opacity = 1.0F;
      for (const auto& entry : record->properties.entries) {
        if (entry.field_id == 1U) {
          if (const auto* value = std::get_if<bool>(&entry.value)) out.selectedVisible = *value;
        } else if (entry.field_id == 3U) {
          if (const auto* value = std::get_if<float>(&entry.value)) out.selectedAppearance.opacity = *value;
        } else if (entry.field_id == 0x100U) {
          if (const auto* style = std::get_if<semantic::FillStyleValue>(&entry.value)) {
            if (const auto* fill = std::get_if<semantic::SolidFill>(style)) {
              out.selectedAppearance.fillEnabled = true;
              out.selectedAppearance.fill = {fill->color.r, fill->color.g, fill->color.b, fill->color.a};
            }
          }
        } else if (entry.field_id == 0x101U) {
          if (const auto* style = std::get_if<semantic::StrokeStyleValue>(&entry.value)) {
            if (const auto* stroke = std::get_if<semantic::SolidStroke>(style)) {
              out.selectedAppearance.strokeEnabled = true;
              out.selectedAppearance.stroke = {stroke->color.r, stroke->color.g,
                                               stroke->color.b, stroke->color.a};
              out.selectedAppearance.strokeWidth = static_cast<float>(stroke->width);
            }
          }
        }
      }
      if (const auto* shape = std::get_if<semantic::ShapeContent>(&record->content)) {
        out.selectedSizeSupported = true; out.selectedWidth = shape->width; out.selectedHeight = shape->height;
        out.selectedShapeKind = shape->shape_kind;
      }
    }
    return out;
  }

  void setTarget(CanvasTargetKey target) noexcept { target_ = target; }
  const CanvasTargetKey& target() const noexcept { return target_; }

 private:
  interaction::CanvasEditingRuntime& runtime_;
  CanvasTargetKey target_{1U, 1U, 1U, 1U};
  std::uint64_t revision_ = 1;
  std::thread::id ownerThread_ = std::this_thread::get_id();
};



CanvasRuntimeFacade::CanvasRuntimeFacade(interaction::CanvasEditingRuntime& runtime,
                                         CanvasTargetKey target) noexcept
    : runtime_(runtime), binding_(std::make_unique<Binding>(runtime)),
      service_(std::make_unique<CanvasControlService>(*binding_)) {
  binding_->setTarget(target);
}
CanvasRuntimeFacade::~CanvasRuntimeFacade() = default;
bool CanvasRuntimeFacade::ownerThread() const noexcept {
  return std::this_thread::get_id() == ownerThread_;
}
void CanvasRuntimeFacade::refreshTarget() const noexcept {
  if (binding_) binding_->setTarget(binding_->target());
}
void CanvasRuntimeFacade::setTarget(CanvasTargetKey target) noexcept {
  if (binding_ == nullptr) return;
  const auto previous = binding_->target();
  if (previous != target && service_ != nullptr) service_->destroyTarget(previous);
  binding_->setTarget(target);
}
const CanvasTargetKey& CanvasRuntimeFacade::target() const noexcept { return binding_->target(); }
bool CanvasRuntimeFacade::isTargetCurrent(const CanvasTargetKey& target) const noexcept {
  return binding_->isTargetCurrent(target);
}
ProductControlReceipt CanvasRuntimeFacade::submitCanvasControl(const CanvasControlRequest& request) noexcept {
  if (request.target != target()) {
    CanvasControlReceipt stale{{request.clientId, request.requestId},
                               CanvasControlReceiptState::kStaleTarget,
                               CanvasControlError::kStaleTarget, 0U, "stale target"};
    return {request.requestId, ProductControlState::kRejected,
            request.target.runtimeEpoch, stale};
  }
  const auto receipt = service_->enqueue(request);
  ProductControlReceipt out{request.requestId,
      receipt.state == CanvasControlReceiptState::kQueued ? ProductControlState::kQueued
        : receipt.state == CanvasControlReceiptState::kApplied ? ProductControlState::kApplied
        : ProductControlState::kRejected, request.target.runtimeEpoch, receipt};
  return out;
}
CanvasControlApplyResult CanvasRuntimeFacade::applyControl(
    const CanvasControlRequest& request) {
  if (!ownerThread() || runtime_.closed())
    return {CanvasControlError::kPermissionDenied, "owner or lifecycle admission failed"};
  return binding_->apply(request);
}
CanvasControlSnapshot CanvasRuntimeFacade::snapshotControl(
    const CanvasTargetKey& target) const {
  return service_->snapshot(target);
}
CanvasControlPort& CanvasRuntimeFacade::controlPort() noexcept {
  return *binding_;
}
ProductControlReceipt CanvasRuntimeFacade::submitProductControl(const ProductControlRequest& request) noexcept {
  if (request.runtimeGeneration != 0U && request.runtimeGeneration != target().runtimeEpoch) {
    return {request.requestId, ProductControlState::kRejected,
            request.runtimeGeneration, std::nullopt};
  }
  if (request.canvasControl) return submitCanvasControl(*request.canvasControl);
  CanvasControlRequest canvas{};
  canvas.clientId = request.clientId;
  canvas.requestId = request.requestId;
  canvas.deadlineSequence = request.deadlineSequence;
  canvas.target = target();
  switch (request.action) {
    case ProductControlAction::kSetTool:
      canvas.payload.kind = CanvasControlPayloadKind::kSelectTool;
      if (request.toolId == 4102U) canvas.payload.tool = CanvasToolKind::kSelect;
      else if (request.toolId == 4103U) canvas.payload.tool = CanvasToolKind::kPan;
      else if (request.toolId == 4104U) { canvas.payload.tool = CanvasToolKind::kEraser; canvas.payload.eraser.mode = OptionPatch<std::uint32_t>::set(2U); }
      else if (request.toolId == 4105U) { canvas.payload.tool = CanvasToolKind::kEraser; canvas.payload.eraser.mode = OptionPatch<std::uint32_t>::set(1U); }
      else if (request.toolId == 4101U || request.toolId == 0U) canvas.payload.tool = CanvasToolKind::kInk;
      else if (request.toolId == 1U) { canvas.payload.tool = CanvasToolKind::kEraser; canvas.payload.eraser.mode = OptionPatch<std::uint32_t>::set(1U); }
      else if (request.toolId == 2U) { canvas.payload.tool = CanvasToolKind::kEraser; canvas.payload.eraser.mode = OptionPatch<std::uint32_t>::set(2U); }
      else return {request.requestId, ProductControlState::kUnsupported, request.runtimeGeneration, std::nullopt};
      break;
    case ProductControlAction::kSetBrush:
      canvas.payload.kind = CanvasControlPayloadKind::kSelectPreset;
      canvas.payload.preset = request.brushId == 1U ? BrushPresetRef{"vector-solid-v1", request.brushRevision}
          : request.brushId == 2U ? BrushPresetRef{"marker-flat-v1", request.brushRevision}
          : request.brushId == 3U ? BrushPresetRef{"chalk-grain-v1", request.brushRevision}
          : BrushPresetRef{"membrane-v1", request.brushRevision};
      break;
    case ProductControlAction::kSetEraser:
      canvas.payload.kind = CanvasControlPayloadKind::kSelectTool;
      canvas.payload.tool = CanvasToolKind::kEraser;
      if (request.eraserId != 1U && request.eraserId != 2U)
        return {request.requestId, ProductControlState::kUnsupported, request.runtimeGeneration, std::nullopt};
      canvas.payload.eraser.mode = OptionPatch<std::uint32_t>::set(request.eraserId);
      break;
    case ProductControlAction::kSetSelectionMode:
      canvas.payload.kind = CanvasControlPayloadKind::kSelectTool;
      canvas.payload.tool = request.selectionMode ? CanvasToolKind::kSelect : CanvasToolKind::kInk;
      break;
    case ProductControlAction::kUndo: canvas.payload.kind = CanvasControlPayloadKind::kUndo; break;
    case ProductControlAction::kRedo: canvas.payload.kind = CanvasControlPayloadKind::kRedo; break;
    case ProductControlAction::kSetCamera:
      if (request.cameraAction == static_cast<std::uint32_t>(CameraControlAction::kPan)) {
        canvas.payload.kind = CanvasControlPayloadKind::kPanBy;
        canvas.payload.x = request.deltaX; canvas.payload.y = request.deltaY;
      } else if (request.cameraAction == static_cast<std::uint32_t>(CameraControlAction::kZoomAt)) {
        canvas.payload.kind = CanvasControlPayloadKind::kZoomAt;
        canvas.payload.x = request.anchorX; canvas.payload.y = request.anchorY;
        canvas.payload.scale = request.scaleDelta;
      } else {
        canvas.payload.kind = CanvasControlPayloadKind::kFit;
        canvas.payload.fitTarget = request.cameraFitTarget == CameraFitTarget::kSelection
            ? CanvasFitTarget::kSelection : request.cameraFitTarget == CameraFitTarget::kObject
                ? CanvasFitTarget::kObject : CanvasFitTarget::kContent;
        canvas.payload.objectId = foundation::ObjectId::fromUint64(request.cameraObjectId);
      }
      break;
    default: return {request.requestId, ProductControlState::kUnsupported, request.runtimeGeneration, std::nullopt};
  }
  return submitCanvasControl(canvas);
}
RuntimeStateSnapshot CanvasRuntimeFacade::readRuntimeState() const noexcept {
  RuntimeStateSnapshot out{};
  out.identity = {target().runtimeEpoch, runtime_.semanticGeneration().value(),
                  static_cast<std::uint64_t>(runtime_.submittedOperationCount()), target().viewEpoch,
                  runtime_.surface().generation};
  out.tool = {runtime_.selectionMode() ? 2U : runtime_.panTool() ? 3U :
                (runtime_.toolMode() == interaction::CanvasEditingRuntime::ToolMode::kObjectEraser ||
                 runtime_.toolMode() == interaction::CanvasEditingRuntime::ToolMode::kPartialEraser) ? 0U : 1U,
              runtime_.selectedBrushProfile() == "vector-solid-v1" ? 1U :
                runtime_.selectedBrushProfile() == "marker-flat-v1" ? 2U :
                runtime_.selectedBrushProfile() == "chalk-grain-v1" ? 3U : 4U,
              runtime_.selectedBrushRevision(),
              runtime_.toolMode() == interaction::CanvasEditingRuntime::ToolMode::kObjectEraser ? 1U
              : runtime_.toolMode() == interaction::CanvasEditingRuntime::ToolMode::kPartialEraser ? 2U : 0U};
  out.camera = {runtime_.viewportGesture().scale, runtime_.viewportGesture().translationX,
                runtime_.viewportGesture().translationY};
  out.history = {runtime_.canUndo(), runtime_.canRedo()};
  out.selection = {runtime_.selectionMode(), static_cast<std::uint32_t>(runtime_.selectedObjectCount()),
                   runtime_.selectedPrimaryObjectValue(), runtime_.snapCandidateCount()};
  const auto snapshot = readCanvasControlSnapshot(target());
  out.control.creationPreset = static_cast<std::uint8_t>(snapshot.creationPreset);
  out.control.creationMode = snapshot.creationMode;
  out.control.semanticGeneration = snapshot.semanticGeneration;
  out.control.selectedKind = snapshot.selectedKind; out.control.selectedVersion = snapshot.selectedVersion;
  out.control.selectedShapeKind = snapshot.selectedShapeKind; out.control.selectedLocked = snapshot.selectedLocked;
  out.control.selectedVisible = snapshot.selectedVisible; out.control.selectedSizeSupported = snapshot.selectedSizeSupported;
  out.control.selectedWidth = snapshot.selectedWidth; out.control.selectedHeight = snapshot.selectedHeight;
  for (int i=0;i<6;++i) out.control.selectedTransform[i] = snapshot.selectedTransform[i];
  out.control.appliedCommitEpoch = snapshot.appliedCommitEpoch;
  out.control.appliedCommitOrdinal = snapshot.appliedCommitOrdinal;
  out.control.presentedSemanticGeneration = snapshot.presentedSemanticGeneration;
  out.control.axisLocalDurable = snapshot.axisLocalDurable;
  out.control.axisCloudSynced = snapshot.axisCloudSynced;
  return out;
}
CanvasControlSnapshot CanvasRuntimeFacade::readCanvasControlSnapshot(const CanvasTargetKey& target) const noexcept {
  return service_->snapshot(target);
}
std::vector<CanvasTargetKey> CanvasRuntimeFacade::mountedCanvasTargets() const {
  return runtime_.closed() ? std::vector<CanvasTargetKey>{} : std::vector<CanvasTargetKey>{target()};
}
std::vector<BrushPresetRef> CanvasRuntimeFacade::brushPresets() const {
  return {{"vector-solid-v1",1U},{"marker-flat-v1",1U},{"chalk-grain-v1",4U},{"membrane-v1",1U}};
}
std::optional<CanvasControlReceipt> CanvasRuntimeFacade::canvasControlReceipt(CanvasControlReceiptKey key) const noexcept {
  return service_->receipt(key);
}
std::size_t CanvasRuntimeFacade::process(std::uint64_t ownerSequence) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || processing_.exchange(true)) return 0;
  controlSequence_.store(ownerSequence, std::memory_order_release);
  const auto result = service_->processPending(ownerSequence);
  processing_ = false;
  return result;
}

}  // namespace canvas::runtime
