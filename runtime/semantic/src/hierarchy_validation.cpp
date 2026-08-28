#include "canvas/semantic/hierarchy_validation.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace canvas::semantic {
namespace {

using Projection = std::map<ObjectId, ObjectRecord>;

const ObjectRecord* projectedFind(const StagedObjectView& staged, const Projection& projection,
                                  const ObjectId& id) {
    if (const auto it = projection.find(id); it != projection.end()) return &it->second;
    return staged.find(id);
}

} // namespace

StatefulResult validateStagedHierarchy(const StagedObjectView& staged,
                                       std::span<const HierarchyEdit> edits) {
    Projection projection;
    for (const auto& edit : edits) {
        const ObjectRecord* current = staged.find(edit.object_id);
        if (current == nullptr) return StatefulResult{StatefulIssue::kObjectMissing};
        ObjectRecord replacement = *current;
        replacement.placement = edit.placement;
        projection[edit.object_id] = std::move(replacement);
    }

    for (const auto& [id, replacement] : projection) {
        static_cast<void>(id);
        if (replacement.placement.parent_id.has_value() &&
            projectedFind(staged, projection, *replacement.placement.parent_id) == nullptr) {
            return StatefulResult{StatefulIssue::kInvalidReference};
        }
    }

    for (const auto& [start, replacement] : projection) {
        static_cast<void>(replacement);
        std::set<ObjectId> visited;
        ObjectId current = start;
        while (true) {
            if (!visited.insert(current).second) {
                return StatefulResult{StatefulIssue::kHierarchyCycle};
            }
            const ObjectRecord* record = projectedFind(staged, projection, current);
            if (record == nullptr || !record->placement.parent_id.has_value()) break;
            current = *record->placement.parent_id;
        }
    }
    return StatefulResult{};
}

std::vector<ObjectId> resolveDescendants(const StagedObjectView& staged, ObjectId root_id) {
    std::vector<ObjectId> result;
    std::set<ObjectId> visited;
    visited.insert(root_id);
    const auto visit = [&](const auto& self, const ObjectId& parent) -> void {
        for (const auto& child : staged.children(parent)) {
            const ObjectId id = child.id;
            if (!visited.insert(id).second) continue;
            result.push_back(id);
            self(self, id);
        }
    };
    visit(visit, root_id);
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace canvas::semantic
