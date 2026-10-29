#pragma once

#include "canvas/runtime/product_control.hpp"
#include "canvas/runtime/multi_selection.hpp"
#include "canvas/runtime/runtime_state.hpp"
#include "canvas/runtime/selection_summary.hpp"
#include "canvas/runtime/text_input.hpp"
#include "canvas/runtime/image_input.hpp"
#include "canvas/runtime/connector_sticky_input.hpp"

#include <utility>
#include <vector>
#include <new>

namespace canvas::runtime {

class RuntimeFacade {
 public:
  virtual ~RuntimeFacade() = default;
  [[nodiscard]] virtual SelectionSummary readSelectionSummary() const { return {}; }
  // Public C++ projection and intents; default implementations fail closed.
  // The concrete composition remains the only selection and History owner.
  [[nodiscard]] virtual bool setMultiSelectionPermission(MultiSelectionPermissionMode) noexcept { return false; }
  [[nodiscard]] virtual MultiSelectionProjection readMultiSelection() const noexcept { return {}; }
  [[nodiscard]] virtual std::optional<std::vector<MultiSelectionRootPropertyValue>> readMultiSelectionPropertyValues(
      std::uint32_t, const MultiSelectionStamp&, const std::vector<foundation::ObjectId>&) const noexcept { return {}; }
  [[nodiscard]] virtual MultiSelectionSubmitReceipt submitMultiSelectionProperty(
      std::uint32_t, const semantic::PropertyValue&, const MultiSelectionStamp&,
      const std::vector<foundation::ObjectId>&, std::uint64_t, std::uint64_t) noexcept { return {}; }
  [[nodiscard]] virtual MultiSelectionSubmitReceipt submitMultiSelectionStrokeWidth(
      double, const MultiSelectionStamp&, const std::vector<foundation::ObjectId>&,
      std::uint64_t, std::uint64_t) noexcept { return {}; }
  [[nodiscard]] virtual std::optional<MultiSelectionOutcome> multiSelectionReceipt(
      std::uint64_t, std::uint64_t) const noexcept { return {}; }
  [[nodiscard]] virtual SectionSnapshot readSection(foundation::ObjectId = {}) const { return {}; }
  [[nodiscard]] ProductControlReceipt submitSectionIntent(const SectionIntent& intent,
      std::uint64_t requestId,std::uint64_t clientId=1) noexcept {
    if(requestId==0||clientId==0||intent.objects.size()>65535||intent.content.title.size()>1048576)
      return {requestId,ProductControlState::kRejected,0,std::nullopt};
    try {
    CanvasControlRequest request{};request.clientId=clientId;request.requestId=requestId;
    const auto targets=mountedCanvasTargets();
    if(targets.empty())return {requestId,ProductControlState::kRejected,0,std::nullopt};
    request.target=targets.front();
    request.payload.kind=CanvasControlPayloadKind::kSection;request.payload.section=intent;
    return submitCanvasControl(request);
    } catch(const std::bad_alloc&) {return {requestId,ProductControlState::kRejected,0,std::nullopt};}
  }
  // Group uses the existing ordered container intent and receipt lane.
  [[nodiscard]] ProductControlReceipt groupObjects(
      std::vector<foundation::ObjectId> objects, foundation::ObjectId groupId,
      std::uint64_t requestId, std::uint64_t clientId = 1,
      std::optional<std::uint64_t> selectionRevision = std::nullopt) noexcept {
    SectionIntent intent; intent.action = SectionAction::kGroup; intent.objectId = groupId;
    intent.objects = std::move(objects); intent.selectionRevision = selectionRevision;
    return submitSectionIntent(intent, requestId, clientId);
  }
  [[nodiscard]] ProductControlReceipt ungroupObject(
      foundation::ObjectId groupId, std::uint64_t requestId,
      std::uint64_t clientId = 1,
      std::optional<std::uint64_t> selectionRevision = std::nullopt) noexcept {
    SectionIntent intent; intent.action = SectionAction::kUngroup; intent.objectId = groupId;
    intent.selectionRevision = selectionRevision;
    return submitSectionIntent(intent, requestId, clientId);
  }
  [[nodiscard]] virtual TextReceipt submitTextIntent(const TextIntent&) { return {}; }
  [[nodiscard]] virtual TextReceipt submitTextInput(const TextInputEvent&) { return {}; }
  [[nodiscard]] virtual TextEditorSnapshot readTextEditor() const { return {}; }
  [[nodiscard]] virtual FontCatalogSnapshot readFontCatalog() const { return {}; }
  [[nodiscard]] virtual std::optional<FontCatalogEntry> readFontResourceStatus(
      foundation::ObjectId) const { return std::nullopt; }
  [[nodiscard]] virtual TextGeometrySnapshot readTextGeometry(const std::optional<TextGeometryToken>& = std::nullopt) const { return {}; }
  [[nodiscard]] virtual std::optional<TextReceipt> textInputReceipt(const TextSessionToken&,std::uint64_t) const { return std::nullopt; }
  [[nodiscard]] virtual std::optional<TextReceipt> textIntentReceipt(std::uint64_t,std::uint64_t) const { return std::nullopt; }
  [[nodiscard]] virtual bool acknowledgeTextInput(const TextSessionToken&,std::uint64_t) { return false; }
  [[nodiscard]] virtual ImageCommandReceipt importImage(const ImageImportRequest&) { return {}; }
  [[nodiscard]] virtual ImageCommandReceipt replaceImage(const ImageReplaceRequest&) { return {}; }
  [[nodiscard]] virtual ImageCommandReceipt resizeImage(const ImageResizeRequest&) { return {}; }
  [[nodiscard]] virtual ImageCommandReceipt beginImageCrop(const ImageCropBeginRequest&) { return {}; }
  [[nodiscard]] virtual ImageCommandReceipt updateImageCrop(const ImageCropUpdateRequest&) { return {}; }
  [[nodiscard]] virtual ImageCommandReceipt commitImageCrop(std::uint64_t) { return {}; }
  [[nodiscard]] virtual ImageCommandReceipt cancelImageCrop(std::uint64_t) { return {}; }
  [[nodiscard]] virtual ImageCropSnapshot readImageCrop() const { return {}; }
  [[nodiscard]] virtual SelectedImageSnapshot readSelectedImage() const { return {}; }
  [[nodiscard]] virtual ConnectorCommandReceipt createConnector(const ConnectorCreateRequest&) { return {}; }
  [[nodiscard]] virtual ConnectorCommandReceipt setConnectorRouting(const ConnectorRoutingRequest&) { return {}; }
  [[nodiscard]] virtual semantic::ConnectorRouting readConnectorRoutingPreset() const { return semantic::ConnectorRouting::kStraight; }
  [[nodiscard]] virtual StickyCommandReceipt createSticky(const StickyCreateRequest&) { return {}; }
  [[nodiscard]] virtual ConnectorSnapshot readSelectedConnector() const { return {}; }
  [[nodiscard]] virtual ConnectorSnapshot readConnectorState(semantic::ObjectId) const { return {}; }
  [[nodiscard]] virtual StickySnapshot readSelectedSticky() const { return {}; }
  [[nodiscard]] virtual ProductControlReceipt submitSelectionPointer(
      const SelectionPointerRequest& request) noexcept {
    ProductControlReceipt receipt{};
    receipt.requestId = request.requestId;
    receipt.state = ProductControlState::kUnsupported;
    receipt.runtimeGeneration = request.runtimeGeneration;
    return receipt;
  }
  [[nodiscard]] virtual RuntimeStateSnapshot readRuntimeState() const noexcept = 0;
  [[nodiscard]] virtual ProductControlReceipt submitProductControl(
      const ProductControlRequest& request) noexcept = 0;
  // Typed canvas controls still use the single RuntimeFacade intent lane.
  // Platform facades may adapt this request to their mounted common owner;
  // they must not interpret the payload in a second UI-owned lane.
  [[nodiscard]] virtual ProductControlReceipt submitCanvasControl(
      const CanvasControlRequest& request) noexcept {
    ProductControlRequest product{};
    product.action = ProductControlAction::kCanvasControl;
    product.requestId = request.requestId;
    product.runtimeGeneration = request.target.runtimeEpoch;
    product.canvasControl = request;
    auto receipt = submitProductControl(product);
    if (!receipt.control.has_value()) {
      CanvasControlReceipt fallback{};
      fallback.key = {request.clientId, request.requestId};
      fallback.state = receipt.state == ProductControlState::kApplied
                           ? CanvasControlReceiptState::kApplied
                           : CanvasControlReceiptState::kFailed;
      fallback.error = receipt.state == ProductControlState::kApplied
                           ? CanvasControlError::kNone
                           : CanvasControlError::kUnsupported;
      receipt.control = std::move(fallback);
    }
    return receipt;
  }
  [[nodiscard]] virtual CanvasControlSnapshot readCanvasControlSnapshot(
      const CanvasTargetKey& target) const noexcept {
    CanvasControlSnapshot snapshot{};
    snapshot.target = target;
    return snapshot;
  }
  [[nodiscard]] virtual std::vector<CanvasTargetKey> mountedCanvasTargets() const { return {}; }
  [[nodiscard]] virtual std::vector<BrushPresetRef> brushPresets() const { return {}; }
  // Owner sequence used by queued product/canvas controls. Implementations
  // that do not expose an owner clock return zero and retain legacy UI timing.
  [[nodiscard]] virtual std::uint64_t controlSequence() const noexcept { return 0U; }
  [[nodiscard]] virtual std::optional<CanvasControlReceipt> canvasControlReceipt(
      CanvasControlReceiptKey) const { return std::nullopt; }

  // These typed entry points are the shared product control plane.  They are
  // implemented in terms of the single owner submission seam so Product Shell
  // and Debug UI cannot grow parallel mutation APIs.
  [[nodiscard]] virtual ProductControlReceipt setTool(
      std::uint32_t toolId, std::uint64_t requestId,
      std::uint64_t runtimeGeneration, std::uint64_t clientId = 1) noexcept {
    ProductControlRequest request{};
    request.action = ProductControlAction::kSetTool;
    request.clientId = clientId;
    request.requestId = requestId;
    request.runtimeGeneration = runtimeGeneration;
    request.toolId = toolId;
    return submitProductControl(request);
  }
  [[nodiscard]] virtual ProductControlReceipt setBrush(
      std::uint32_t brushId, std::uint32_t revision, std::uint64_t requestId,
      std::uint64_t runtimeGeneration, std::uint64_t clientId = 1) noexcept {
    ProductControlRequest request{};
    request.action = ProductControlAction::kSetBrush;
    request.clientId = clientId;
    request.requestId = requestId;
    request.runtimeGeneration = runtimeGeneration;
    request.brushId = brushId;
    request.brushRevision = revision;
    return submitProductControl(request);
  }
  [[nodiscard]] virtual ProductControlReceipt setEraser(
      std::uint32_t eraserId, std::uint64_t requestId,
      std::uint64_t runtimeGeneration, std::uint64_t clientId = 1) noexcept {
    ProductControlRequest request{};
    request.action = ProductControlAction::kSetEraser;
    request.clientId = clientId;
    request.requestId = requestId;
    request.runtimeGeneration = runtimeGeneration;
    request.eraserId = eraserId;
    return submitProductControl(request);
  }
  [[nodiscard]] ProductControlReceipt setSelectionAcquisitionMode(CanvasSelectionMode mode,
      std::uint64_t requestId,std::uint64_t clientId=1) noexcept {
    const auto targets=mountedCanvasTargets();
    if(targets.empty()) return {requestId,ProductControlState::kRejected,0,std::nullopt};
    CanvasControlRequest request{};request.clientId=clientId;request.requestId=requestId;request.target=targets.front();
    request.payload.kind=CanvasControlPayloadKind::kSelectionAcquisitionMode;
    request.payload.selectionAcquisitionMode=mode;return submitCanvasControl(request);
  }
  [[nodiscard]] virtual ProductControlReceipt setSelectionMode(
      bool enabled, std::uint64_t requestId,
      std::uint64_t runtimeGeneration, std::uint64_t clientId = 1) noexcept {
    ProductControlRequest request{};
    request.action = ProductControlAction::kSetSelectionMode;
    request.clientId = clientId;
    request.requestId = requestId;
    request.runtimeGeneration = runtimeGeneration;
    request.selectionMode = enabled;
    return submitProductControl(request);
  }
  [[nodiscard]] virtual ProductControlReceipt undo(
      std::uint64_t requestId, std::uint64_t runtimeGeneration,
      std::uint64_t clientId = 1) noexcept {
    ProductControlRequest request{};
    request.action = ProductControlAction::kUndo;
    request.clientId = clientId;
    request.requestId = requestId;
    request.runtimeGeneration = runtimeGeneration;
    return submitProductControl(request);
  }
  [[nodiscard]] virtual ProductControlReceipt redo(
      std::uint64_t requestId, std::uint64_t runtimeGeneration,
      std::uint64_t clientId = 1) noexcept {
    ProductControlRequest request{};
    request.action = ProductControlAction::kRedo;
    request.clientId = clientId;
    request.requestId = requestId;
    request.runtimeGeneration = runtimeGeneration;
    return submitProductControl(request);
  }
  [[nodiscard]] virtual ProductControlReceipt setCamera(
      CameraControlAction cameraAction, float anchorX, float anchorY,
      float deltaX, float deltaY, float scaleDelta, std::uint64_t requestId,
      std::uint64_t runtimeGeneration, std::uint64_t clientId = 1) noexcept {
    ProductControlRequest request{};
    request.action = ProductControlAction::kSetCamera;
    request.clientId = clientId;
    request.requestId = requestId;
    request.runtimeGeneration = runtimeGeneration;
    request.cameraAction = static_cast<std::uint32_t>(cameraAction);
    request.anchorX = anchorX;
    request.anchorY = anchorY;
    request.deltaX = deltaX;
    request.deltaY = deltaY;
    request.scaleDelta = scaleDelta;
    return submitProductControl(request);
  }
  [[nodiscard]] ProductControlReceipt panBy(
      float deltaX, float deltaY, std::uint64_t requestId,
      std::uint64_t runtimeGeneration, std::uint64_t clientId = 1) noexcept {
    return setCamera(CameraControlAction::kPan, 0.0F, 0.0F, deltaX, deltaY,
                     1.0F, requestId, runtimeGeneration, clientId);
  }
  [[nodiscard]] ProductControlReceipt zoomAt(
      float anchorX, float anchorY, float scaleDelta, std::uint64_t requestId,
      std::uint64_t runtimeGeneration, std::uint64_t clientId = 1) noexcept {
    return setCamera(CameraControlAction::kZoomAt, anchorX, anchorY, 0.0F,
                     0.0F, scaleDelta, requestId, runtimeGeneration, clientId);
  }
  [[nodiscard]] ProductControlReceipt fitToContent(
      std::uint64_t requestId, std::uint64_t runtimeGeneration,
      std::uint64_t clientId = 1) noexcept {
    return setCamera(CameraControlAction::kFitToContent, 0.0F, 0.0F, 0.0F,
                     0.0F, 1.0F, requestId, runtimeGeneration, clientId);
  }
  [[nodiscard]] ProductControlReceipt fitToSelection(
      std::uint64_t requestId, std::uint64_t runtimeGeneration,
      std::uint64_t clientId = 1) noexcept {
    ProductControlRequest request{};
    request.action = ProductControlAction::kSetCamera;
    request.clientId = clientId;
    request.requestId = requestId;
    request.runtimeGeneration = runtimeGeneration;
    request.cameraAction = static_cast<std::uint32_t>(CameraControlAction::kFitToContent);
    request.cameraFitTarget = CameraFitTarget::kSelection;
    return submitProductControl(request);
  }
  [[nodiscard]] ProductControlReceipt fitToObject(
      std::uint64_t objectId, std::uint64_t requestId,
      std::uint64_t runtimeGeneration, std::uint64_t clientId = 1) noexcept {
    ProductControlRequest request{};
    request.action = ProductControlAction::kSetCamera;
    request.clientId = clientId;
    request.requestId = requestId;
    request.runtimeGeneration = runtimeGeneration;
    request.cameraAction = static_cast<std::uint32_t>(CameraControlAction::kFitToContent);
    request.cameraFitTarget = CameraFitTarget::kObject;
    request.cameraObjectId = objectId;
    return submitProductControl(request);
  }
};

}  // namespace canvas::runtime
