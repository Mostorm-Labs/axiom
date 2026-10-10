#pragma once

#include "canvas/runtime/diagnostics.hpp"
#include "canvas/runtime/canvas_control_types.hpp"

#include <cstdint>
#include <array>
#include <optional>
#include <vector>

namespace canvas::runtime {

enum class ProductControlAction : std::uint8_t {
  kSetTool,
  kSetBrush,
  kSetEraser,
  kSetSelectionMode,
  kSetCamera,
  kUndo,
  kRedo,
  kCanvasControl,
};

enum class ProductControlState : std::uint8_t {
  kApplied,
  kQueued,
  kUnsupported,
  kRejected,
  kFailed,
};

enum class CameraControlAction : std::uint8_t { kPan, kZoomAt, kFitToContent };
enum class CameraFitTarget : std::uint8_t { kDocument, kSelection, kObject, kWorldRect };

// Product-safe state exposed to Product Shell and the Debug Controller.  The
// values are an aggregate projection; they never expose RuntimeScene or
// renderer-owned pointers.
struct RuntimeStateSnapshot final {
  std::uint64_t runtimeGeneration = 0;
  std::uint64_t documentGeneration = 0;
  std::uint64_t documentRevision = 0;
  std::uint64_t viewGeneration = 0;
  std::uint64_t surfaceGeneration = 0;
  std::uint32_t toolId = 0;
  std::uint32_t brushId = 0;
  std::uint32_t brushRevision = 0;
  std::uint32_t eraserId = 0;
  float cameraScale = 1.0F;
  float cameraTranslationX = 0.0F;
  float cameraTranslationY = 0.0F;
  bool canUndo = false;
  bool canRedo = false;
  bool selectionMode = false;
  std::uint32_t selectedObjectCount = 0;
  std::uint64_t selectedPrimaryObject = 0;
  std::uint64_t snapCandidateCount = 0;
};

struct RuntimeDiagnosticsSnapshot final {
  std::uint64_t runtimeGeneration = 0;
  std::uint64_t documentGeneration = 0;
  std::uint64_t documentRevision = 0;
  std::uint64_t viewGeneration = 0;
  std::uint32_t toolId = 0;
  std::uint64_t surfaceGeneration = 0;
  float cameraScale = 1.0F;
  float cameraTranslationX = 0.0F;
  float cameraTranslationY = 0.0F;
  bool canUndo = false;
  bool canRedo = false;
  std::uint64_t overlayUpdateCount = 0;
  std::uint64_t transientTransformCount = 0;
  std::uint64_t canonicalOperationCount = 0;
  std::uint64_t cameraGeneration = 0;
  bool selectionMode = false;
  std::uint32_t selectedObjectCount = 0;
  std::uint64_t selectedPrimaryObject = 0;
  std::uint64_t snapCandidateCount = 0;
};

class RuntimeDiagnostics : public DiagnosticsProvider {
 public:
  ~RuntimeDiagnostics() override = default;
  [[nodiscard]] virtual RuntimeDiagnosticsSnapshot readDiagnostics() const noexcept = 0;
};

using AxiomDiagnostics = RuntimeDiagnostics;
using IAxiomDiagnostics = RuntimeDiagnostics;

struct ProductControlRequest final {
  ProductControlAction action = ProductControlAction::kSetTool;
  std::uint64_t requestId = 0;
  std::uint64_t runtimeGeneration = 0;
  std::uint64_t deadlineSequence = 0;
  std::uint32_t toolId = 0;
  std::uint32_t brushId = 0;
  std::uint32_t brushRevision = 0;
  std::uint32_t eraserId = 0;
  std::uint32_t cameraAction = 0;
  float deltaX = 0.0F;
  float deltaY = 0.0F;
  float anchorX = 0.0F;
  float anchorY = 0.0F;
  float scaleDelta = 1.0F;
  bool selectionMode = false;
  CameraFitTarget cameraFitTarget = CameraFitTarget::kDocument;
  std::uint64_t cameraObjectId = 0;
  // All new controls use this value payload through the existing intent lane.
  std::optional<CanvasControlRequest> canvasControl;
  std::optional<foundation::ObjectId> fullCameraObjectId;
  std::array<float, 4> cameraWorldRect{};
};

struct ProductControlReceipt final {
  std::uint64_t requestId = 0;
  ProductControlState state = ProductControlState::kRejected;
  std::uint64_t runtimeGeneration = 0;
  std::optional<CanvasControlReceipt> control;
};

enum class SelectionPointerPhase : std::uint8_t { kDown, kMove, kUp, kCancel };
struct SelectionPointerRequest final {
  std::uint64_t pointerId = 0;
  SelectionPointerPhase phase = SelectionPointerPhase::kMove;
  float viewX = 0.0F;
  float viewY = 0.0F;
  std::uint64_t requestId = 0;
  std::uint64_t runtimeGeneration = 0;
};

class RuntimeFacade : public RuntimeDiagnostics {
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
  [[nodiscard]] virtual RuntimeStateSnapshot readRuntimeState() const noexcept {
    const auto diagnostics = readDiagnostics();
    return {diagnostics.runtimeGeneration, diagnostics.documentGeneration,
            diagnostics.documentRevision, diagnostics.viewGeneration,
            diagnostics.surfaceGeneration, diagnostics.toolId, 0U, 0U, 0U,
            diagnostics.cameraScale, diagnostics.cameraTranslationX,
            diagnostics.cameraTranslationY, diagnostics.canUndo, diagnostics.canRedo};
  }
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
    request.toolId = brushId;
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
    request.toolId = eraserId;
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
