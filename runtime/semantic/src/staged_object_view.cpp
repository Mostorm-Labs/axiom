#include "canvas/semantic/staged_object_view.hpp"

#include <algorithm>

namespace canvas::semantic {

bool StagedObjectView::contains(const ObjectId& id) const noexcept {
    return find(id) != nullptr;
}

const ObjectRecord* StagedObjectView::find(const ObjectId& id) const noexcept {
    if (deletes_.contains(id)) return nullptr;
    if (const auto it = replacements_.find(id); it != replacements_.end()) return &it->second;
    if (const auto it = creates_.find(id); it != creates_.end()) return &it->second;
    return base_.find(id);
}

std::vector<ObjectRecord> StagedObjectView::materialize() const {
    std::map<ObjectId, ObjectRecord> records;
    for (const auto& record : base_.allObjects()) records.emplace(record.id, record);
    for (const ObjectId& id : deletes_) records.erase(id);
    for (const auto& [id, record] : replacements_) records[id] = record;
    for (const auto& [id, record] : creates_) records[id] = record;

    std::vector<ObjectRecord> result;
    result.reserve(records.size());
    for (auto& [id, record] : records) {
        static_cast<void>(id);
        result.push_back(std::move(record));
    }
    return result;
}

std::vector<ObjectRecord> StagedObjectView::allObjects() const { return materialize(); }

bool StagedObjectView::childBefore(const ObjectRecord& left, const ObjectRecord& right) {
    if (const auto order = left.placement.order_key <=> right.placement.order_key; order != 0) {
        return order < 0;
    }
    return left.id < right.id;
}

std::vector<ObjectRecord> StagedObjectView::children(
    const std::optional<ObjectId>& parent_id) const {
    std::vector<ObjectRecord> result;
    for (auto& record : materialize()) {
        if (record.placement.parent_id == parent_id) result.push_back(std::move(record));
    }
    std::sort(result.begin(), result.end(), childBefore);
    return result;
}

bool StagedObjectView::stageCreate(ObjectRecord record) {
    if (deletes_.contains(record.id) || contains(record.id) || creates_.contains(record.id) ||
        replacements_.contains(record.id)) {
        return false;
    }
    creates_.emplace(record.id, std::move(record));
    return true;
}

bool StagedObjectView::stageReplace(ObjectRecord record) {
    if (deletes_.contains(record.id) || !contains(record.id)) return false;
    if (creates_.contains(record.id)) {
        creates_[record.id] = std::move(record);
        return true;
    }
    replacements_[record.id] = std::move(record);
    return true;
}

bool StagedObjectView::stageDelete(const ObjectId& id) {
    if (!contains(id)) return false;
    creates_.erase(id);
    replacements_.erase(id);
    deletes_.insert(id);
    return true;
}

} // namespace canvas::semantic
