#include "canvas/semantic/snapshot_bootstrap.hpp"

#include "canvas/semantic/connector_validation.hpp"
#include "canvas/semantic/hierarchy_capability_validation.hpp"
#include "canvas/semantic/hierarchy_validation.hpp"
#include "object_record_semantics_internal.hpp"
#include "object_store_mutator.hpp"

#include <algorithm>
#include <type_traits>
#include <utility>
#include <vector>
#if defined(CANVAS_SEMANTIC_PROTOBUF)
#include "auditoryworks/axiom/v1/snapshot.pb.h"
#endif

namespace canvas::semantic {
namespace {

bool supportedSnapshotVersion(std::uint32_t version) noexcept {
    return version == 1U || version == 2U || version == 3U;
}

bool snapshotObjectSupported(std::uint32_t schema_version, SnapshotCapability capability, const ObjectRecord& object) noexcept {
    if (object.kind == ObjectKind::kTable) return schema_version==3U && (static_cast<unsigned>(capability)&64U)!=0;
    if (object.kind == ObjectKind::kSection)
        return schema_version == 3U && (static_cast<unsigned>(capability) & 16U) != 0U;
    if (object.kind == ObjectKind::kRichText && object.kind_version == 3U)
        return schema_version==3U && (static_cast<unsigned>(capability)&6U)==6U;
    if (object.kind == ObjectKind::kRichText && object.kind_version == 2U)
        return schema_version==3U && (static_cast<unsigned>(capability)&2U)!=0U;
    if (object.kind != ObjectKind::kShape || (object.kind_version != 2U && object.kind_version != 3U)) return true;
    const auto* shape = std::get_if<ShapeContent>(&object.content);
    return shape == nullptr || shape->shape_kind != 3U ||
           ((schema_version == 2U || schema_version == 3U) && (static_cast<unsigned>(capability)&1U)!=0U);
}

bool connectorShapeCapabilitySatisfied(const SemanticSnapshot& snapshot) {
    bool required = false;
    for (const auto& object : snapshot.objects) {
        if (object.kind != ObjectKind::kConnector) continue;
        const auto* connector = std::get_if<ConnectorContent>(&object.content);
        if (!connector) continue;
        for (const auto* endpoint : {&connector->start, &connector->end}) {
            const auto* attached = std::get_if<AttachedEndpoint>(&endpoint->value);
            if (!attached) continue;
            const auto it = std::find_if(snapshot.objects.begin(), snapshot.objects.end(),
                [&](const auto& candidate) { return candidate.id == attached->target_object_id; });
            if (it != snapshot.objects.end() && it->kind == ObjectKind::kShape && it->kind_version >= 2U)
                required = true;
        }
    }
    if (!required) return true;
    if (snapshot.schema_version < 3U) return false;
#if !defined(CANVAS_SEMANTIC_PROTOBUF)
    return false;
#else
    auditoryworks::axiom::v1::CapabilityRequirementsV1 caps;
    if (!caps.ParseFromArray(snapshot.capability_requirements.data(), static_cast<int>(snapshot.capability_requirements.size()))) return false;
    for (const auto& id : caps.required_ids()) if (id == "connector-shape-attachment-v1") return true;
    return false;
#endif
}

SnapshotBootstrapResult rejectWithoutFailure(StatefulIssue issue) {
    SnapshotBootstrapResult result;
    result.semantic_error = StatefulResult{issue};
    return result;
}

SnapshotBootstrapResult rejectLoading(DocumentRuntimeState& state, StatefulIssue issue) {
    state = DocumentRuntimeState::kFailed;
    return rejectWithoutFailure(issue);
}

template <typename Store>
SnapshotBootstrapResult restoreImpl(const SemanticSnapshot& snapshot,
                                    SnapshotCapability capability,
                                    DocumentRuntimeState& state,
                                    Store& objects,
                                    SemanticGenerationState& generation,
                                    const SnapshotBootstrapper::CheckpointBinding* checkpoint = nullptr) {
    if (state != DocumentRuntimeState::kLoading) {
        return rejectWithoutFailure(StatefulIssue::kInvalidApplicability);
    }
    const SemanticGeneration generation_before = generation.current();
    if (objects.size() != 0U) {
        return rejectLoading(state, StatefulIssue::kInvalidApplicability);
    }
    if (!supportedSnapshotVersion(snapshot.schema_version) || snapshot.document_id.isZero()) {
        return rejectLoading(state, StatefulIssue::kInvalidApplicability);
    }
    if (!foundation::validDocumentBackground(snapshot.background) ||
        (snapshot.background != foundation::DocumentBackground{} &&
         (snapshot.schema_version < 3 || (static_cast<unsigned>(capability) & 32U) == 0)))
        return rejectLoading(state, StatefulIssue::kInvalidApplicability);
    if (!connectorShapeCapabilitySatisfied(snapshot))
        return rejectLoading(state, StatefulIssue::kInvalidKindVersion);
    if(snapshot.schema_version==3U) {
        if(!checkpoint || checkpoint->document!=snapshot.document_id ||
           checkpoint->source_revision!=snapshot.document_revision || checkpoint->frontier!=snapshot.recovery_frontier ||
           checkpoint->checkpoint_digest!=snapshot.checkpoint_digest || !SnapshotCodec::encode(snapshot,capability).ok())
            return rejectLoading(state,StatefulIssue::kInvalidApplicability);
    }
    if (snapshot.schema_version >= 3U && (!snapshot.manifest_present || !validateResourceManifest(snapshot.manifest) ||
        snapshot.digest_algorithm != 1U || snapshot.document_digest.size() != 32U ||
        snapshot.checkpoint_digest.size() != 32U || snapshot.recovery_frontier.empty() ||
        snapshot.capability_requirements.empty())) {
        return rejectLoading(state, StatefulIssue::kInvalidApplicability);
    }
    if (snapshot.schema_version < 3U && (snapshot.manifest_present || snapshot.document_revision!=0 || snapshot.digest_algorithm!=0 || !snapshot.recovery_frontier.empty() || !snapshot.document_digest.empty() || !snapshot.capability_requirements.empty() || !snapshot.checkpoint_digest.empty())) {
        return rejectLoading(state, StatefulIssue::kInvalidApplicability);
    }

    std::vector<ObjectRecord> ordered = snapshot.objects;
    std::sort(ordered.begin(), ordered.end(), [](const ObjectRecord& left, const ObjectRecord& right) {
        return left.id < right.id;
    });
    for (std::size_t index = 0U; index < ordered.size(); ++index) {
        if (!snapshotObjectSupported(snapshot.schema_version, capability, ordered[index])) {
            return rejectLoading(state, StatefulIssue::kInvalidKindVersion);
        }
        if (!internal::validateObjectRecord(ordered[index])) {
            return rejectLoading(state, StatefulIssue::kInvalidApplicability);
        }
        if (index > 0U && ordered[index - 1U].id == ordered[index].id) {
            return rejectLoading(state, StatefulIssue::kObjectAlreadyExists);
        }
    }

    ReferenceObjectStore validation_base;
    StagedObjectView staged(validation_base);
    std::vector<HierarchyEdit> hierarchy_edits;
    std::vector<ObjectId> affected_ids;
    hierarchy_edits.reserve(ordered.size());
    affected_ids.reserve(ordered.size());
    for (const ObjectRecord& object : ordered) {
        if (!staged.stageCreate(object)) {
            return rejectLoading(state, StatefulIssue::kObjectAlreadyExists);
        }
        hierarchy_edits.push_back(HierarchyEdit{object.id, object.placement});
        affected_ids.push_back(object.id);
    }
    // Resource availability is derived; the canonical manifest is restored as a
    // complete value only after every object and manifest invariant has passed.

    const StatefulResult hierarchy = validateStagedHierarchy(staged, hierarchy_edits);
    if (!hierarchy.ok()) return rejectLoading(state, hierarchy.issue);
    const StatefulResult capabilities =
        validateStagedHierarchyCapabilities(staged, affected_ids);
    if (!capabilities.ok()) return rejectLoading(state, capabilities.issue);
    for (const ObjectRecord& object : ordered) {
        if (const auto* connector = std::get_if<ConnectorContent>(&object.content)) {
            const StatefulResult references = validateConnectorReferences(staged, *connector);
            if (!references.ok()) return rejectLoading(state, references.issue);
        }
    }

    std::vector<ObjectId> inserted_ids;
    inserted_ids.reserve(ordered.size());
    for (const ObjectRecord& object : ordered) {
        if (!internal::ObjectStoreMutator::insertFresh(objects, object)) {
            for (const ObjectId& inserted : inserted_ids) {
                static_cast<void>(internal::ObjectStoreMutator::eraseExisting(objects, inserted));
            }
            return rejectLoading(state, StatefulIssue::kObjectAlreadyExists);
        }
        inserted_ids.push_back(object.id);
    }
    if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
        if (!internal::ObjectStoreMutator::indexMatchesRebuild(objects)) {
            for (const ObjectId& inserted : inserted_ids) {
                static_cast<void>(internal::ObjectStoreMutator::eraseExisting(objects, inserted));
            }
            return rejectLoading(state, StatefulIssue::kInvalidApplicability);
        }
    }
    if (generation.current() != generation_before) {
        return rejectLoading(state, StatefulIssue::kInvalidApplicability);
    }

    SnapshotBootstrapResult result;
    internal::ObjectStoreMutator::setDocumentBackground(objects, snapshot.background);
    result.restored = true;
    return result;
}

template <typename Store>
SnapshotBootstrapResult restoreWithManifestImpl(const SemanticSnapshot& snapshot,
    SnapshotCapability capability, DocumentRuntimeState& state, Store& objects,
    ResourceManifest& manifest, SemanticGenerationState& generation,
    const SnapshotBootstrapper::CheckpointBinding* checkpoint = nullptr) {
    if (snapshot.schema_version < 3U || !snapshot.manifest_present) return rejectWithoutFailure(StatefulIssue::kInvalidApplicability);
    const auto before = manifest;
    const auto result = restoreImpl(snapshot, capability, state, objects, generation,checkpoint);
    if (!result.restored) return result;
    manifest = snapshot.manifest;
    if (!validateResourceManifest(manifest)) { manifest = before; return rejectLoading(state, StatefulIssue::kInvalidApplicability); }
    return result;
}

} // namespace

SnapshotBootstrapResult SnapshotBootstrapper::restore(const SemanticSnapshot& snapshot,
    SnapshotCapability capability,DocumentRuntimeState& state,ReferenceObjectStore& objects,
    ResourceManifest& manifest,SemanticGenerationState& generation,const CheckpointBinding& checkpoint) {
    return restoreWithManifestImpl(snapshot,capability,state,objects,manifest,generation,&checkpoint);
}
SnapshotBootstrapResult SnapshotBootstrapper::restore(const SemanticSnapshot& snapshot,
    SnapshotCapability capability,DocumentRuntimeState& state,IndexedObjectStore& objects,
    ResourceManifest& manifest,SemanticGenerationState& generation,const CheckpointBinding& checkpoint) {
    return restoreWithManifestImpl(snapshot,capability,state,objects,manifest,generation,&checkpoint);
}

SnapshotBootstrapResult SnapshotBootstrapper::restore(
    const SemanticSnapshot& snapshot,
    DocumentRuntimeState& state,
    ReferenceObjectStore& objects,
    SemanticGenerationState& generation) {
    return restoreImpl(snapshot, SnapshotCapability::kLegacyV1, state, objects, generation);
}

SnapshotBootstrapResult SnapshotBootstrapper::restore(
    const SemanticSnapshot& snapshot, SnapshotCapability capability,
    DocumentRuntimeState& state, ReferenceObjectStore& objects,
    ResourceManifest& manifest, SemanticGenerationState& generation) {
    return restoreWithManifestImpl(snapshot, capability, state, objects, manifest, generation);
}

SnapshotBootstrapResult SnapshotBootstrapper::restore(
    const SemanticSnapshot& snapshot, SnapshotCapability capability,
    DocumentRuntimeState& state, IndexedObjectStore& objects,
    ResourceManifest& manifest, SemanticGenerationState& generation) {
    return restoreWithManifestImpl(snapshot, capability, state, objects, manifest, generation);
}

SnapshotBootstrapResult SnapshotBootstrapper::restore(
    const SemanticSnapshot& snapshot,
    DocumentRuntimeState& state,
    IndexedObjectStore& objects,
    SemanticGenerationState& generation) {
    return restoreImpl(snapshot, SnapshotCapability::kLegacyV1, state, objects, generation);
}

SnapshotBootstrapResult SnapshotBootstrapper::restore(
    const SemanticSnapshot& snapshot, SnapshotCapability capability,
    DocumentRuntimeState& state, ReferenceObjectStore& objects,
    SemanticGenerationState& generation) {
    return restoreImpl(snapshot, capability, state, objects, generation);
}

SnapshotBootstrapResult SnapshotBootstrapper::restore(
    const SemanticSnapshot& snapshot, SnapshotCapability capability,
    DocumentRuntimeState& state, IndexedObjectStore& objects,
    SemanticGenerationState& generation) {
    return restoreImpl(snapshot, capability, state, objects, generation);
}

} // namespace canvas::semantic
