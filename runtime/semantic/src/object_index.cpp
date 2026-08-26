#include "object_index.hpp"

namespace canvas::semantic::internal {

void ObjectIndex::insert(const ObjectRecord& record) {
    children_by_parent_[record.placement.parent_id].insert(
        ChildEntry{record.placement.order_key, record.id});
}

void ObjectIndex::erase(const ObjectRecord& record) {
    const auto parent_it = children_by_parent_.find(record.placement.parent_id);
    if (parent_it == children_by_parent_.end()) {
        return;
    }

    parent_it->second.erase(ChildEntry{record.placement.order_key, record.id});
    if (parent_it->second.empty()) {
        children_by_parent_.erase(parent_it);
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
    return children_by_parent_ == other.children_by_parent_;
}

} // namespace canvas::semantic::internal
