#pragma once

#include "canvas/foundation/result.hpp"
#include "canvas/scene/scene.hpp"
#include "canvas/scene/scene_compiler.hpp"
#include "canvas/semantic/semantic_generation.hpp"

#include <cstdint>
#include <optional>

namespace canvas {

enum class SceneSyncDisposition : std::uint8_t {
    kAppliedIncremental,
    kRebuiltFull,
};

struct SceneSyncReceipt final {
    SceneRevision revision;
    semantic::SemanticGeneration semanticGeneration{};
    SceneSyncDisposition disposition = SceneSyncDisposition::kAppliedIncremental;
    SceneApplyReceipt apply;
    std::optional<foundation::Error> incrementalFailure;
};

class SceneBinding final {
  public:
    explicit SceneBinding(Scene& scene) : _scene(scene) {}

    foundation::Result<SceneSyncReceipt> rebuild(const ICompiledSceneSource& source);
    foundation::Result<SceneSyncReceipt> synchronize(const ICompiledSceneSource& source);
    foundation::Result<SceneSyncReceipt> rebuild(const ISemanticSceneCompiler& compiler,
                                                 const SceneCommitInput& input);
    foundation::Result<SceneSyncReceipt> synchronize(const ISemanticSceneCompiler& compiler,
                                                     const SceneCommitInput& input);

  private:
    foundation::Result<SceneSyncReceipt>
    rebuildAfterIncrementalFailure(const ICompiledSceneSource& source,
                                   foundation::Error incrementalFailure);

    Scene& _scene;
};

} // namespace canvas
