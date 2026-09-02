#include "canvas/semantic/hierarchy_capability_validation.hpp"

#include "canvas/semantic/operation_state_validator.hpp"

#include <set>

namespace canvas::semantic {

StatefulResult validateStagedHierarchyCapabilities(
    const StagedObjectView& staged,
    std::span<const ObjectId> affected_child_ids) {
    std::set<ObjectId> sticky_parents;
    for (const ObjectId& child_id : affected_child_ids) {
        const ObjectRecord* child = staged.find(child_id);
        if (child == nullptr) return StatefulResult{StatefulIssue::kObjectMissing};
        const StatefulResult child_kind =
            validateRecordStateForOperation(*child, StateRule::kCreateAbsent);
        if (!child_kind.ok()) return child_kind;
        if (!child->placement.parent_id.has_value()) continue;

        const ObjectRecord* parent = staged.find(*child->placement.parent_id);
        if (parent == nullptr) return StatefulResult{StatefulIssue::kInvalidReference};
        const StatefulResult parent_kind =
            validateRecordStateForOperation(*parent, StateRule::kCreateAbsent);
        if (!parent_kind.ok()) return parent_kind;
        if (parent->kind == ObjectKind::kGroup) continue;
        if (parent->kind != ObjectKind::kSticky) {
            return StatefulResult{StatefulIssue::kInvalidApplicability};
        }
        if (child->kind != ObjectKind::kRichText) {
            return StatefulResult{StatefulIssue::kInvalidApplicability};
        }
        sticky_parents.insert(parent->id);
    }

    for (const ObjectId& sticky_id : sticky_parents) {
        std::size_t rich_text_count = 0U;
        for (const ObjectRecord& child : staged.children(sticky_id)) {
            const StatefulResult child_kind =
                validateRecordStateForOperation(child, StateRule::kCreateAbsent);
            if (!child_kind.ok()) return child_kind;
            if (child.kind != ObjectKind::kRichText || ++rich_text_count > 1U) {
                return StatefulResult{StatefulIssue::kInvalidApplicability};
            }
        }
    }
    return StatefulResult{};
}

} // namespace canvas::semantic
