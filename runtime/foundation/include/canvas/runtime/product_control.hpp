#pragma once

#include "canvas/runtime/canvas_control_types.hpp"

#include <array>
#include <cstdint>
#include <optional>

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

}  // namespace canvas::runtime
