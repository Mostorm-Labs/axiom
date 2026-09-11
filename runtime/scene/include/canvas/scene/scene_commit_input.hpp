#pragma once

#include "canvas/semantic/change_set.hpp"
#include "canvas/semantic/semantic_read_view.hpp"

#include <utility>

namespace canvas {

// Explicit bridge value for a generation-bound Scene transition. The
// post_state view is captured at after_generation; changes is immutable and
// describes only the before -> after semantic transition.
struct SceneCommitInput final {
    const canvas::semantic::SemanticGeneration before_generation{};
    const canvas::semantic::SemanticGeneration after_generation{};
    const canvas::semantic::SemanticReadView post_state;
    const canvas::semantic::ChangeSet* const changes = nullptr;

    SceneCommitInput(
        canvas::semantic::SemanticGeneration generation,
        canvas::semantic::SemanticReadView post)
        : before_generation(generation),
          after_generation(generation),
          post_state(post),
          changes(nullptr) {}

    SceneCommitInput(
        canvas::semantic::SemanticGeneration before,
        canvas::semantic::SemanticGeneration after,
        canvas::semantic::SemanticReadView post,
        const canvas::semantic::ChangeSet* change_set)
        : before_generation(before),
          after_generation(after),
          post_state(post),
          changes(change_set) {}

    SceneCommitInput(const SceneCommitInput&) = delete;
    SceneCommitInput& operator=(const SceneCommitInput&) = delete;
    SceneCommitInput(SceneCommitInput&&) = delete;
    SceneCommitInput& operator=(SceneCommitInput&&) = delete;
};

} // namespace canvas
