#pragma once

#include "canvas/foundation/result.hpp"
#include "canvas/scene/scene_types.hpp"

#include "canvas/scene/scene_commit_input.hpp"

#include "canvas/semantic/change_set.hpp"
#include "canvas/semantic/semantic_read_view.hpp"

namespace canvas {

class ISemanticSceneCompiler {
  public:
    virtual ~ISemanticSceneCompiler() = default;
    virtual foundation::Result<CompiledSceneSnapshot> compileFull(
        const semantic::SemanticReadView& view) const = 0;
    virtual foundation::Result<CompiledSceneDelta> compileDelta(
        const semantic::SemanticReadView& view,
        const semantic::ChangeSet& changes) const = 0;
};

class ICompiledSceneSource {
  public:
    virtual ~ICompiledSceneSource() = default;

    virtual foundation::Result<CompiledSceneSnapshot> compileFull() const = 0;
    virtual foundation::Result<CompiledSceneDelta> compileDelta() const = 0;
};

} // namespace canvas
