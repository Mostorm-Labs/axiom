#include "canvas/semantic/delete_closure.hpp"

#include "canvas/semantic/object_content.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace canvas::semantic {
namespace {

void sortUnique(std::vector<ObjectId>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

using ReverseRelation = std::map<ObjectId, std::vector<ObjectId>>;

ReverseRelation buildReverseRelation(const StagedObjectView& staged) {
    ReverseRelation relation;
    for (const ObjectRecord& record : staged.allObjects()) {
        if (record.kind != ObjectKind::kConnector || record.kind_version != 1U) continue;
        const auto* content = std::get_if<ConnectorContent>(&record.content);
        if (content == nullptr) continue;
        std::set<ObjectId> targets;
        for (const ConnectorEndpoint* endpoint : {&content->start, &content->end}) {
            if (const auto* attached = std::get_if<AttachedEndpoint>(&endpoint->value)) {
                targets.insert(attached->target_object_id);
            }
        }
        for (const ObjectId& target : targets) relation[target].push_back(record.id);
    }
    for (auto& [target, connectors] : relation) {
        static_cast<void>(target);
        sortUnique(connectors);
    }
    return relation;
}

} // namespace

StatefulResult resolveDeleteClosure(
    const StagedObjectView& staged,
    std::span<const ObjectId> requested_ids,
    DeleteClosure* out) {
    DeleteClosure result{};
    result.requested_delete_ids.assign(requested_ids.begin(), requested_ids.end());

    for (const ObjectId& requested : result.requested_delete_ids) {
        if (staged.find(requested) == nullptr) {
            return {StatefulIssue::kObjectMissing};
        }
    }

    const ReverseRelation reverse = buildReverseRelation(staged);
    std::set<ObjectId> admitted(result.requested_delete_ids.begin(), result.requested_delete_ids.end());
    std::set<ObjectId> hierarchy;
    std::set<ObjectId> connectors;
    std::vector<ObjectId> frontier = result.requested_delete_ids;

    while (!frontier.empty()) {
        std::vector<ObjectId> hierarchy_additions;
        for (const ObjectId& parent : frontier) {
            for (const ObjectRecord& child : staged.children(parent)) {
                if (admitted.insert(child.id).second) {
                    hierarchy.insert(child.id);
                    hierarchy_additions.push_back(child.id);
                }
            }
        }
        sortUnique(hierarchy_additions);

        std::vector<ObjectId> relevant = frontier;
        relevant.insert(relevant.end(), hierarchy_additions.begin(), hierarchy_additions.end());
        sortUnique(relevant);
        std::vector<ObjectId> connector_additions;
        for (const ObjectId& target : relevant) {
            const auto it = reverse.find(target);
            if (it == reverse.end()) continue;
            for (const ObjectId& connector_id : it->second) {
                if (admitted.insert(connector_id).second) {
                    connectors.insert(connector_id);
                    connector_additions.push_back(connector_id);
                }
            }
        }
        sortUnique(connector_additions);
        frontier = hierarchy_additions;
        frontier.insert(frontier.end(), connector_additions.begin(), connector_additions.end());
        sortUnique(frontier);
    }

    result.resolved_hierarchy_closure.assign(hierarchy.begin(), hierarchy.end());
    result.resolved_connector_cascade_closure.assign(connectors.begin(), connectors.end());
    result.final_delete_set.assign(admitted.begin(), admitted.end());
    if (out != nullptr) *out = std::move(result);
    return {};
}

} // namespace canvas::semantic
