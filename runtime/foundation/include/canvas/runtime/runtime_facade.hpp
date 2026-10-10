#pragma once

#include "canvas/runtime/product_control.hpp"
#include "canvas/runtime/runtime_state.hpp"

#include <utility>
#include <vector>

namespace canvas::runtime {

class RuntimeFacade {
 public:
  virtual ~RuntimeFacade() = default;
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
  [[nodiscard]] virtual std::optional<CanvasControlReceipt> canvasControlReceipt(
      CanvasControlReceiptKey) const { return std::nullopt; }

  // These typed entry points are the shared product control plane.  They are
  // implemented in terms of the single owner submission seam so Product Shell
  // and Debug UI cannot grow parallel mutation APIs.
  [[nodiscard]] virtual ProductControlReceipt setTool(
      std::uint32_t toolId, std::uint64_t requestId,
      std::uint64_t runtimeGeneration) noexcept {
    ProductControlRequest request{};
    request.action = ProductControlAction::kSetTool;
    request.requestId = requestId;
    request.runtimeGeneration = runtimeGeneration;
    request.toolId = toolId;
    return submitProductControl(request);
  }
  [[nodiscard]] virtual ProductControlReceipt setBrush(
      std::uint32_t brushId, std::uint32_t revision, std::uint64_t requestId,
      std::uint64_t runtimeGeneration) noexcept {
    ProductControlRequest request{};
    request.action = ProductControlAction::kSetBrush;
    request.requestId = requestId;
    request.runtimeGeneration = runtimeGeneration;
    request.brushId = brushId;
    request.brushRevision = revision;
    return submitProductControl(request);
  }
  [[nodiscard]] virtual ProductControlReceipt setEraser(
      std::uint32_t eraserId, std::uint64_t requestId,
      std::uint64_t runtimeGeneration) noexcept {
    ProductControlRequest request{};
    request.action = ProductControlAction::kSetEraser;
    request.requestId = requestId;
    request.runtimeGeneration = runtimeGeneration;
    request.eraserId = eraserId;
    return submitProductControl(request);
  }
  [[nodiscard]] virtual ProductControlReceipt setSelectionMode(
      bool enabled, std::uint64_t requestId,
      std::uint64_t runtimeGeneration) noexcept {
    ProductControlRequest request{};
    request.action = ProductControlAction::kSetSelectionMode;
    request.requestId = requestId;
    request.runtimeGeneration = runtimeGeneration;
    request.selectionMode = enabled;
    return submitProductControl(request);
  }
  [[nodiscard]] virtual ProductControlReceipt undo(
      std::uint64_t requestId, std::uint64_t runtimeGeneration) noexcept {
    ProductControlRequest request{};
    request.action = ProductControlAction::kUndo;
    request.requestId = requestId;
    request.runtimeGeneration = runtimeGeneration;
    return submitProductControl(request);
  }
  [[nodiscard]] virtual ProductControlReceipt redo(
      std::uint64_t requestId, std::uint64_t runtimeGeneration) noexcept {
    ProductControlRequest request{};
    request.action = ProductControlAction::kRedo;
    request.requestId = requestId;
    request.runtimeGeneration = runtimeGeneration;
    return submitProductControl(request);
  }
  [[nodiscard]] virtual ProductControlReceipt setCamera(
      CameraControlAction cameraAction, float anchorX, float anchorY,
      float deltaX, float deltaY, float scaleDelta, std::uint64_t requestId,
      std::uint64_t runtimeGeneration) noexcept {
    ProductControlRequest request{};
    request.action = ProductControlAction::kSetCamera;
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
      std::uint64_t runtimeGeneration) noexcept {
    return setCamera(CameraControlAction::kPan, 0.0F, 0.0F, deltaX, deltaY,
                     1.0F, requestId, runtimeGeneration);
  }
  [[nodiscard]] ProductControlReceipt zoomAt(
      float anchorX, float anchorY, float scaleDelta, std::uint64_t requestId,
      std::uint64_t runtimeGeneration) noexcept {
    return setCamera(CameraControlAction::kZoomAt, anchorX, anchorY, 0.0F,
                     0.0F, scaleDelta, requestId, runtimeGeneration);
  }
  [[nodiscard]] ProductControlReceipt fitToContent(
      std::uint64_t requestId, std::uint64_t runtimeGeneration) noexcept {
    return setCamera(CameraControlAction::kFitToContent, 0.0F, 0.0F, 0.0F,
                     0.0F, 1.0F, requestId, runtimeGeneration);
  }
  [[nodiscard]] ProductControlReceipt fitToSelection(
      std::uint64_t requestId, std::uint64_t runtimeGeneration) noexcept {
    ProductControlRequest request{};
    request.action = ProductControlAction::kSetCamera;
    request.requestId = requestId;
    request.runtimeGeneration = runtimeGeneration;
    request.cameraAction = static_cast<std::uint32_t>(CameraControlAction::kFitToContent);
    request.cameraFitTarget = CameraFitTarget::kSelection;
    return submitProductControl(request);
  }
  [[nodiscard]] ProductControlReceipt fitToObject(
      std::uint64_t objectId, std::uint64_t requestId,
      std::uint64_t runtimeGeneration) noexcept {
    ProductControlRequest request{};
    request.action = ProductControlAction::kSetCamera;
    request.requestId = requestId;
    request.runtimeGeneration = runtimeGeneration;
    request.cameraAction = static_cast<std::uint32_t>(CameraControlAction::kFitToContent);
    request.cameraFitTarget = CameraFitTarget::kObject;
    request.cameraObjectId = objectId;
    return submitProductControl(request);
  }
};

}  // namespace canvas::runtime
