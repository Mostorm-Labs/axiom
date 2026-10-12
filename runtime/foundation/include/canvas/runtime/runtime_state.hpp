#pragma once

#include <cstdint>

namespace canvas::runtime {

struct RuntimeIdentityState final {
  std::uint64_t runtimeGeneration = 0;
  std::uint64_t documentGeneration = 0;
  std::uint64_t documentRevision = 0;
  std::uint64_t viewGeneration = 0;
  std::uint64_t surfaceGeneration = 0;
};

struct ToolState final {
  std::uint32_t toolId = 0;
  std::uint32_t brushId = 0;
  std::uint32_t brushRevision = 0;
  std::uint32_t eraserId = 0;
};

struct CameraState final {
  float scale = 1.0F;
  float translationX = 0.0F;
  float translationY = 0.0F;
};

struct HistoryState final {
  bool canUndo = false;
  bool canRedo = false;
};

struct SelectionState final {
  bool enabled = false;
  std::uint32_t selectedObjectCount = 0;
  std::uint64_t primaryObject = 0;
  std::uint64_t snapCandidateCount = 0;
};

struct RuntimeControlState final {
  std::uint32_t selectedKind = 0;
  std::uint32_t selectedVersion = 0;
  std::uint32_t selectedShapeKind = 0;
  bool selectedLocked = false;
  bool selectedVisible = true;
  bool selectedSizeSupported = false;
  double selectedWidth = 0.0;
  double selectedHeight = 0.0;
  double selectedTransform[6] = {1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
  std::uint64_t semanticGeneration = 0;
};

struct RuntimeStateSnapshot final {
  RuntimeIdentityState identity{};
  ToolState tool{};
  CameraState camera{};
  HistoryState history{};
  SelectionState selection{};
  RuntimeControlState control{};
};

}  // namespace canvas::runtime
