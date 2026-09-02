#include "change_set_builder.hpp"

#include <algorithm>
#include <utility>

namespace canvas::semantic::internal {
namespace {

void appendCreated(
    std::vector<ObjectSemanticChange>& changes,
    const std::vector<ObjectRecord>& records) {
    changes.reserve(changes.size() + records.size());
    for (const ObjectRecord& record : records) {
        changes.push_back({record.id, SemanticChangeFlags::kCreated, {}});
    }
}

void appendDeleted(
    std::vector<ObjectSemanticChange>& changes,
    const std::vector<ObjectId>& ids) {
    changes.reserve(changes.size() + ids.size());
    for (const ObjectId& id : ids) {
        changes.push_back({id, SemanticChangeFlags::kDeleted, {}});
    }
}

SemanticChangeFlags replacementFlag(OperationKind kind) {
    switch (kind) {
    case OperationKind::kSetPlacements:
        return SemanticChangeFlags::kPlacement;
    case OperationKind::kSetTransforms:
        return SemanticChangeFlags::kTransform;
    case OperationKind::kPatchProperties:
        return SemanticChangeFlags::kProperties;
    case OperationKind::kAddEraseMasks:
    case OperationKind::kRemoveEraseMasks:
        return SemanticChangeFlags::kEraseMasks;
    case OperationKind::kSetObjectSize:
    case OperationKind::kSetVectorPathGeometry:
    case OperationKind::kSetImageContent:
    case OperationKind::kEditRichText:
    case OperationKind::kSetConnectorContent:
        return SemanticChangeFlags::kContent;
    default:
        return SemanticChangeFlags::kNone;
    }
}

void normalize(std::vector<ObjectSemanticChange>& changes) {
    std::sort(changes.begin(), changes.end(), [](const auto& left, const auto& right) {
        return left.object_id < right.object_id;
    });
    std::vector<ObjectSemanticChange> merged;
    merged.reserve(changes.size());
    for (auto& change : changes) {
        std::sort(change.changed_fields.begin(), change.changed_fields.end());
        change.changed_fields.erase(
            std::unique(change.changed_fields.begin(), change.changed_fields.end()),
            change.changed_fields.end());
        if (!merged.empty() && merged.back().object_id == change.object_id) {
            auto& existing = merged.back();
            existing.flags = existing.flags | change.flags;
            existing.changed_fields.insert(
                existing.changed_fields.end(),
                change.changed_fields.begin(),
                change.changed_fields.end());
            std::sort(existing.changed_fields.begin(), existing.changed_fields.end());
            existing.changed_fields.erase(
                std::unique(existing.changed_fields.begin(), existing.changed_fields.end()),
                existing.changed_fields.end());
        } else {
            merged.push_back(std::move(change));
        }
    }
    changes = std::move(merged);
}

} // namespace

std::optional<PreparedChangeSet> prepareChangeSet(const PreparedApplyPlan& plan) {
    PreparedChangeSet prepared;
    appendCreated(prepared.changes, plan.creates);
    appendDeleted(prepared.changes, plan.deletes);

    const SemanticChangeFlags replacement_flag = replacementFlag(plan.operation.kind());
    for (const ObjectRecord& replacement : plan.replacements) {
        prepared.changes.push_back({replacement.id, replacement_flag, {}});
    }

    if (plan.operation.kind() == OperationKind::kPatchProperties) {
        const auto& patch = std::get<PatchPropertiesOp>(plan.operation.payload);
        for (const PropertyPatch& item : patch.patches) {
            prepared.changes.push_back({
                item.object_id,
                SemanticChangeFlags::kProperties,
                {item.field_id}});
        }
    }
    normalize(prepared.changes);
    return prepared;
}

ChangeSet finalizeChangeSet(
    PreparedChangeSet prepared,
    SemanticGeneration before,
    SemanticGeneration after) {
    return ChangeSet(before, after, std::move(prepared.changes));
}

} // namespace canvas::semantic::internal
