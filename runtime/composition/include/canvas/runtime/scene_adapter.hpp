#pragma once

#include "canvas/foundation/result.hpp"
#include "canvas/scene/incremental_runtime_coordinator.hpp"
#include "canvas/scene/scene_binding.hpp"

namespace canvas::runtime {

class SceneAdapter final {
 public:
  explicit SceneAdapter(canvas::IncrementalRuntimeCoordinator& coordinator) noexcept
      : coordinator_(coordinator) {}

  [[nodiscard]] foundation::Result<canvas::SceneSyncReceipt> recover(
      const canvas::ISemanticSceneCompiler& compiler,
      const canvas::SceneCommitInput& input) {
    return coordinator_.recover(compiler, input);
  }
  [[nodiscard]] foundation::Result<canvas::SceneSyncReceipt> apply(
      const canvas::ISemanticSceneCompiler& compiler,
      const canvas::SceneCommitInput& input) {
    return coordinator_.apply(compiler, input);
  }
  [[nodiscard]] const canvas::RuntimeScene& runtimeScene() const noexcept {
    return coordinator_.runtimeScene();
  }
  [[nodiscard]] bool coherent() const noexcept {
    return coordinator_.publicationObservationCoherent();
  }

 private:
  canvas::IncrementalRuntimeCoordinator& coordinator_;
};

}  // namespace canvas::runtime
