#pragma once

#include "canvas/semantic/document_runtime_state.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/semantic_generation.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "canvas/semantic/stateful_validation.hpp"

namespace canvas::semantic {

struct SnapshotBootstrapResult final {
    bool restored = false;
    SemanticError decode_error = SemanticError::kNone;
    StatefulResult semantic_error{};
};

class SnapshotBootstrapper final {
  public:
    [[nodiscard]] static SnapshotBootstrapResult restore(
        const SemanticSnapshot& snapshot,
        DocumentRuntimeState& state,
        ReferenceObjectStore& objects,
        SemanticGenerationState& generation);

    [[nodiscard]] static SnapshotBootstrapResult restore(
        const SemanticSnapshot& snapshot,
        DocumentRuntimeState& state,
        IndexedObjectStore& objects,
        SemanticGenerationState& generation);
};

} // namespace canvas::semantic
