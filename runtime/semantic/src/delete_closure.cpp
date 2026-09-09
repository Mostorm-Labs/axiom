#include "canvas/semantic/delete_closure.hpp"

#include "canvas/semantic/object_content.hpp"
#include "delete_closure_internal.hpp"
#include "g1_08_indexed_access_probe_internal.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace canvas::semantic {
namespace {

void sortUnique(std::vector<ObjectId>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

} // namespace

namespace internal {

StatefulResult resolveDeleteClosureWithTrace(
    const StagedObjectView& staged,
    std::span<const ObjectId> requested_ids,
    DeleteClosure* out,
    DeleteClosureTrace* trace) {
    if (trace != nullptr) *trace = {};
    DeleteClosure result{};
    result.requested_delete_ids.assign(requested_ids.begin(), requested_ids.end());

    for (const ObjectId& requested : result.requested_delete_ids) {
        if (staged.find(requested) == nullptr) {
            return {StatefulIssue::kObjectMissing};
        }
    }

    std::set<ObjectId> admitted(result.requested_delete_ids.begin(), result.requested_delete_ids.end());
    const bool indexed_store = internal::stagedUsesIndexed(staged);
    std::map<ObjectId, std::vector<ObjectId>> reference_reverse;
    if (!indexed_store) {
        for (const ObjectRecord& record : staged.allObjects()) {
            if (record.kind != ObjectKind::kConnector || record.kind_version != 1U) continue;
            const auto* content = std::get_if<ConnectorContent>(&record.content);
            if (content == nullptr) continue;
            std::set<ObjectId> targets;
            for (const ConnectorEndpoint* endpoint : {&content->start, &content->end}) {
                if (const auto* attached = std::get_if<AttachedEndpoint>(&endpoint->value))
                    targets.insert(attached->target_object_id);
            }
            for (const ObjectId& target : targets) reference_reverse[target].push_back(record.id);
        }
    }
    std::set<ObjectId> hierarchy;
    std::set<ObjectId> connectors;
    std::vector<ObjectId> frontier = result.requested_delete_ids;

    while (!frontier.empty()) {
        DeleteClosureWave wave;
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
            std::vector<ObjectId> connector_ids;
            if (indexed_store) {
                connector_ids = internal::stagedConnectorsReferencing(staged, target);
            } else if (const auto reference_it = reference_reverse.find(target);
                       reference_it != reference_reverse.end()) {
                connector_ids = reference_it->second;
            }
            for (const ObjectId& connector_id : connector_ids) {
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
        if (trace != nullptr) {
            ++trace->fixed_point_waves;
            trace->reverse_relation_lookups += relevant.size();
            wave.hierarchy_additions = hierarchy_additions;
            wave.connector_additions = connector_additions;
            trace->waves.push_back(std::move(wave));
        }
    }

    result.resolved_hierarchy_closure.assign(hierarchy.begin(), hierarchy.end());
    result.resolved_connector_cascade_closure.assign(connectors.begin(), connectors.end());
    result.final_delete_set.assign(admitted.begin(), admitted.end());
    if (out != nullptr) *out = std::move(result);
    return {};
}

} // namespace internal

StatefulResult resolveDeleteClosure(
    const StagedObjectView& staged,
    std::span<const ObjectId> requested_ids,
    DeleteClosure* out) {
    return internal::resolveDeleteClosureWithTrace(staged, requested_ids, out, nullptr);
}

} // namespace canvas::semantic
