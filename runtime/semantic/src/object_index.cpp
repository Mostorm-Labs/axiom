#include "object_index.hpp"

#include "canvas/semantic/object_content.hpp"

namespace canvas::semantic::internal {

ObjectIndex::ParentPatch ObjectIndex::prepareParentChanges(
    const std::vector<ParentEntry>& before, const std::vector<ParentEntry>& after) const {
    ParentPatch patch;
    const auto prepare = [&](const ParentScope& parent) -> ChildSet& {
        auto [entry, inserted] = patch.try_emplace(parent);
        if (inserted) {
            const auto current = children_by_parent_.find(parent);
            if (current != children_by_parent_.end()) entry->second = current->second;
        }
        return entry->second;
    };
    for (const auto& entry : before) prepare(entry.placement.parent_id).erase({entry.placement.order_key, entry.id});
    for (const auto& entry : after) prepare(entry.placement.parent_id).insert({entry.placement.order_key, entry.id});
    return patch;
}

void ObjectIndex::publishParentChanges(ParentPatch&& patch) noexcept {
    while (!patch.empty()) {
        auto next = patch.begin();
        const auto current = children_by_parent_.find(next->first);
        if (next->second.empty()) {
            if (current != children_by_parent_.end()) children_by_parent_.erase(current);
        } else if (current != children_by_parent_.end()) {
            current->second.swap(next->second);
        } else {
            children_by_parent_.insert(patch.extract(next));
            continue;
        }
        patch.erase(next);
    }
}

void ObjectIndex::insert(const ObjectRecord& record) {
    children_by_parent_[record.placement.parent_id].insert(
        ChildEntry{record.placement.order_key, record.id});
    if (record.kind != ObjectKind::kConnector || record.kind_version != 1U) return;
    const auto* content = std::get_if<ConnectorContent>(&record.content);
    if (content == nullptr) return;
    for (const ConnectorEndpoint* endpoint : {&content->start, &content->end}) {
        if (const auto* attached = std::get_if<AttachedEndpoint>(&endpoint->value)) {
            connectors_by_target_[attached->target_object_id].insert(record.id);
        }
    }
}

void ObjectIndex::erase(const ObjectRecord& record) {
    const auto parent_it = children_by_parent_.find(record.placement.parent_id);
    if (parent_it != children_by_parent_.end()) {
        parent_it->second.erase(ChildEntry{record.placement.order_key, record.id});
        if (parent_it->second.empty()) {
            children_by_parent_.erase(parent_it);
        }
    }
    if (record.kind != ObjectKind::kConnector || record.kind_version != 1U) return;
    const auto* content = std::get_if<ConnectorContent>(&record.content);
    if (content == nullptr) return;
    for (const ConnectorEndpoint* endpoint : {&content->start, &content->end}) {
        if (const auto* attached = std::get_if<AttachedEndpoint>(&endpoint->value)) {
            const auto target_it = connectors_by_target_.find(attached->target_object_id);
            if (target_it == connectors_by_target_.end()) continue;
            target_it->second.erase(record.id);
            if (target_it->second.empty()) connectors_by_target_.erase(target_it);
        }
    }
}

std::vector<ObjectId> ObjectIndex::children(const std::optional<ObjectId>& parent_id) const {
    const auto parent_it = children_by_parent_.find(parent_id);
    if (parent_it == children_by_parent_.end()) {
        return {};
    }

    std::vector<ObjectId> result;
    result.reserve(parent_it->second.size());
    for (const ChildEntry& child : parent_it->second) {
        result.push_back(child.id);
    }
    return result;
}

bool ObjectIndex::equals(const ObjectIndex& other) const noexcept {
    return children_by_parent_ == other.children_by_parent_ &&
           connectors_by_target_ == other.connectors_by_target_;
}

std::vector<ObjectId> ObjectIndex::connectorsReferencing(const ObjectId& target) const {
    const auto it = connectors_by_target_.find(target);
    if (it == connectors_by_target_.end()) return {};
    return {it->second.begin(), it->second.end()};
}

} // namespace canvas::semantic::internal
