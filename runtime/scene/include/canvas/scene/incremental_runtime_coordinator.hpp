#pragma once

#include "canvas/foundation/result.hpp"
#include "canvas/scene/scene_binding.hpp"
#include "canvas/semantic/change_set.hpp"
#include "canvas/semantic/semantic_read_view.hpp"

#include <cstdint>
#include <vector>

namespace canvas {

enum class RuntimeUpdateDisposition : std::uint8_t {
    kIncremental,
    kRecovery,
};

struct RuntimeUpdatePlan final {
    semantic::SemanticGeneration beforeGeneration{};
    semantic::SemanticGeneration afterGeneration{};
    RuntimeUpdateDisposition disposition = RuntimeUpdateDisposition::kIncremental;
    bool requiresFullRebuild = false;
    std::vector<semantic::ObjectId> affectedObjects;
};

class IncrementalRuntimeCoordinator final {
  public:
    explicit IncrementalRuntimeCoordinator(SceneBinding& binding) noexcept : binding_(binding) {}

    [[nodiscard]] foundation::Result<RuntimeUpdatePlan> plan(
        const semantic::SemanticReadView& postState,
        const semantic::ChangeSet& changes) const;

    foundation::Result<SceneSyncReceipt> apply(
        const ISemanticSceneCompiler& compiler,
        const SceneCommitInput& input);

    foundation::Result<SceneSyncReceipt> recover(
        const ISemanticSceneCompiler& compiler,
        const SceneCommitInput& input);

  private:
    SceneBinding& binding_;
};

} // namespace canvas
