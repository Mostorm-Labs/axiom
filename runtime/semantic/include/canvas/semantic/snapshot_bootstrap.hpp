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
    struct CheckpointBinding final {
        DocumentId document{};
        std::uint64_t source_revision = 0;
        std::vector<std::uint8_t> frontier;
        std::vector<std::uint8_t> checkpoint_digest;
    };
    [[nodiscard]] static SnapshotBootstrapResult restore(
        const SemanticSnapshot&, SnapshotCapability, DocumentRuntimeState&,
        ReferenceObjectStore&, ResourceManifest&, SemanticGenerationState&, const CheckpointBinding&);
    [[nodiscard]] static SnapshotBootstrapResult restore(
        const SemanticSnapshot&, SnapshotCapability, DocumentRuntimeState&,
        IndexedObjectStore&, ResourceManifest&, SemanticGenerationState&, const CheckpointBinding&);
    [[nodiscard]] static SnapshotBootstrapResult restore(
        const SemanticSnapshot& snapshot,
        DocumentRuntimeState& state,
        ReferenceObjectStore& objects,
        SemanticGenerationState& generation);

    [[nodiscard]] static SnapshotBootstrapResult restore(
        const SemanticSnapshot& snapshot,
        SnapshotCapability capability,
        DocumentRuntimeState& state,
        ReferenceObjectStore& objects,
        SemanticGenerationState& generation);

    [[nodiscard]] static SnapshotBootstrapResult restore(
        const SemanticSnapshot& snapshot,
        DocumentRuntimeState& state,
        IndexedObjectStore& objects,
        SemanticGenerationState& generation);

    [[nodiscard]] static SnapshotBootstrapResult restore(
        const SemanticSnapshot& snapshot,
        SnapshotCapability capability,
        DocumentRuntimeState& state,
        IndexedObjectStore& objects,
        SemanticGenerationState& generation);

    [[nodiscard]] static SnapshotBootstrapResult restore(
        const SemanticSnapshot& snapshot,
        SnapshotCapability capability,
        DocumentRuntimeState& state,
        ReferenceObjectStore& objects,
        ResourceManifest& manifest,
        SemanticGenerationState& generation);

    [[nodiscard]] static SnapshotBootstrapResult restore(
        const SemanticSnapshot& snapshot,
        SnapshotCapability capability,
        DocumentRuntimeState& state,
        IndexedObjectStore& objects,
        ResourceManifest& manifest,
        SemanticGenerationState& generation);
};

} // namespace canvas::semantic
