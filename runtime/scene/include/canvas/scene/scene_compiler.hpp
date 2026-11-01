#pragma once

#include "canvas/foundation/result.hpp"
#include "canvas/scene/scene_types.hpp"

#include "canvas/scene/scene_commit_input.hpp"

#include "canvas/semantic/change_set.hpp"
#include "canvas/semantic/semantic_read_view.hpp"

namespace canvas {

class SceneReadView;

class ISemanticSceneCompiler {
  public:
    virtual ~ISemanticSceneCompiler() = default;
    virtual foundation::Result<CompiledSceneSnapshot> compileFull(
        const semantic::SemanticReadView& view) const = 0;
    virtual foundation::Result<CompiledSceneDelta> compileDelta(
        const semantic::SemanticReadView& view,
        const semantic::ChangeSet& changes) const = 0;
    // Runtime-internal preparation seam. The coordinator supplies a closed
    // affected projection and the exact published before image; legacy
    // compilers retain their explicit full-rebuild/failure behavior.
    virtual foundation::Result<CompiledSceneDelta> compilePreparedDelta(
        const semantic::SemanticReadView& view, const semantic::ChangeSet& changes,
        std::span<const RuntimeSceneRecord>, const SceneReadView&) const {
        return compileDelta(view, changes);
    }
};

class ICompiledSceneSource {
  public:
    virtual ~ICompiledSceneSource() = default;

    virtual foundation::Result<CompiledSceneSnapshot> compileFull() const = 0;
    virtual foundation::Result<CompiledSceneDelta> compileDelta() const = 0;
};

} // namespace canvas
