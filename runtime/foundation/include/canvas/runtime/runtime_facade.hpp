#pragma once

#include "canvas/runtime/diagnostics.hpp"

#include <cstdint>

namespace canvas::runtime {

enum class ProductControlAction : std::uint8_t {
  kSetTool,
  kSetBrush,
  kSetEraser,
  kSetCamera,
  kUndo,
  kRedo,
};

enum class ProductControlState : std::uint8_t {
  kApplied,
  kQueued,
  kUnsupported,
  kRejected,
  kFailed,
};

enum class CameraControlAction : std::uint8_t { kPan, kZoomAt, kFitToContent };

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
};

struct ProductControlReceipt final {
  std::uint64_t requestId = 0;
  ProductControlState state = ProductControlState::kRejected;
  std::uint64_t runtimeGeneration = 0;
};

class RuntimeFacade : public RuntimeDiagnostics {
 public:
  virtual ~RuntimeFacade() = default;
  [[nodiscard]] virtual RuntimeStateSnapshot readRuntimeState() const noexcept {
    const auto diagnostics = readDiagnostics();
    return {diagnostics.runtimeGeneration, diagnostics.documentGeneration,
            diagnostics.documentRevision, diagnostics.viewGeneration,
            diagnostics.surfaceGeneration, diagnostics.toolId, 0U, 0U, 0U,
            diagnostics.cameraScale, diagnostics.cameraTranslationX,
            diagnostics.cameraTranslationY};
  }
  [[nodiscard]] virtual ProductControlReceipt submitProductControl(
      const ProductControlRequest& request) noexcept = 0;

  // These typed entry points are the shared product control plane.  They are
  // implemented in terms of the single owner submission seam so Product Shell
  // and Debug UI cannot grow parallel mutation APIs.
  [[nodiscard]] virtual ProductControlReceipt setTool(
      std::uint32_t toolId, std::uint64_t requestId,
      std::uint64_t runtimeGeneration) noexcept {
    return submitProductControl({ProductControlAction::kSetTool, requestId,
                                 runtimeGeneration, 0U, toolId});
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
  [[nodiscard]] virtual ProductControlReceipt undo(
      std::uint64_t requestId, std::uint64_t runtimeGeneration) noexcept {
    return submitProductControl({ProductControlAction::kUndo, requestId,
                                 runtimeGeneration});
  }
  [[nodiscard]] virtual ProductControlReceipt redo(
      std::uint64_t requestId, std::uint64_t runtimeGeneration) noexcept {
    return submitProductControl({ProductControlAction::kRedo, requestId,
                                 runtimeGeneration});
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
};

}  // namespace canvas::runtime
