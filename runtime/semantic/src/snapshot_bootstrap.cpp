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

namespace canvas::semantic {
namespace {

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
                                    DocumentRuntimeState& state,
                                    Store& objects,
                                    SemanticGenerationState& generation) {
    if (state != DocumentRuntimeState::kLoading) {
        return rejectWithoutFailure(StatefulIssue::kInvalidApplicability);
    }
    const SemanticGeneration generation_before = generation.current();
    if (objects.size() != 0U) {
        return rejectLoading(state, StatefulIssue::kInvalidApplicability);
    }
    if (snapshot.schema_version != 1U || snapshot.document_id.isZero()) {
        return rejectLoading(state, StatefulIssue::kInvalidApplicability);
    }

    std::vector<ObjectRecord> ordered = snapshot.objects;
    std::sort(ordered.begin(), ordered.end(), [](const ObjectRecord& left, const ObjectRecord& right) {
        return left.id < right.id;
    });
    for (std::size_t index = 0U; index < ordered.size(); ++index) {
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
    result.restored = true;
    return result;
}

} // namespace

SnapshotBootstrapResult SnapshotBootstrapper::restore(
    const SemanticSnapshot& snapshot,
    DocumentRuntimeState& state,
    ReferenceObjectStore& objects,
    SemanticGenerationState& generation) {
    return restoreImpl(snapshot, state, objects, generation);
}

SnapshotBootstrapResult SnapshotBootstrapper::restore(
    const SemanticSnapshot& snapshot,
    DocumentRuntimeState& state,
    IndexedObjectStore& objects,
    SemanticGenerationState& generation) {
    return restoreImpl(snapshot, state, objects, generation);
}

} // namespace canvas::semantic
