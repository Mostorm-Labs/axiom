#include "canvas/semantic/staged_object_view.hpp"

#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/object_content.hpp"
#include "g1_08_indexed_access_probe_internal.hpp"

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

std::vector<ObjectRecord> StagedObjectView::allObjects() const {
    const std::vector<ObjectRecord> base_records = base_.allObjects();
    std::vector<ObjectRecord> result;
    result.reserve(base_records.size() - std::min(base_records.size(), deletes_.size()) + creates_.size());

    auto create_it = creates_.begin();
    for (const ObjectRecord& base_record : base_records) {
        if (deletes_.contains(base_record.id)) continue;
        if (const auto replacement = replacements_.find(base_record.id); replacement != replacements_.end()) {
            while (create_it != creates_.end() && create_it->first < base_record.id) {
                result.push_back(create_it->second);
                ++create_it;
            }
            result.push_back(replacement->second);
        } else {
            while (create_it != creates_.end() && create_it->first < base_record.id) {
                result.push_back(create_it->second);
                ++create_it;
            }
            result.push_back(base_record);
        }
    }
    while (create_it != creates_.end()) {
        result.push_back(create_it->second);
        ++create_it;
    }
    return result;
}

bool StagedObjectView::childBefore(const ObjectRecord& left, const ObjectRecord& right) {
    if (const auto order = left.placement.order_key <=> right.placement.order_key; order != 0) {
        return order < 0;
    }
    return left.id < right.id;
}

std::vector<ObjectRecord> StagedObjectView::children(
    const std::optional<ObjectId>& parent_id) const {
    std::vector<ObjectRecord> result = base_.children(parent_id);
    result.erase(std::remove_if(result.begin(), result.end(), [&](const ObjectRecord& record) {
                     return deletes_.contains(record.id) ||
                            (replacements_.contains(record.id) &&
                             replacements_.at(record.id).placement.parent_id != parent_id);
                 }),
                 result.end());
    for (auto& record : result) {
        if (const auto replacement = replacements_.find(record.id); replacement != replacements_.end()) {
            record = replacement->second;
        }
    }
    for (const auto& [id, record] : replacements_) {
        const ObjectRecord* base_record = base_.find(id);
        if (base_record != nullptr && base_record->placement.parent_id != parent_id &&
            record.placement.parent_id == parent_id && !deletes_.contains(id)) {
            result.push_back(record);
        }
    }
    for (const auto& [id, record] : creates_) {
        static_cast<void>(id);
        if (record.placement.parent_id == parent_id) result.push_back(record);
    }
    std::sort(result.begin(), result.end(), childBefore);
    return result;
}

namespace internal {
namespace {
bool referencesTarget(const ObjectRecord& record, const ObjectId& target) {
    if (record.kind != ObjectKind::kConnector || record.kind_version != 1U) return false;
    const auto* content = std::get_if<ConnectorContent>(&record.content);
    if (content == nullptr) return false;
    for (const ConnectorEndpoint* endpoint : {&content->start, &content->end}) {
        if (const auto* attached = std::get_if<AttachedEndpoint>(&endpoint->value);
            attached != nullptr && attached->target_object_id == target) return true;
    }
    return false;
}
} // namespace

bool usesIndexedConnectorLookup(const StagedObjectView& staged) {
    return dynamic_cast<const IndexedObjectStore*>(&staged.base_) != nullptr;
}

std::map<ObjectId, std::vector<ObjectId>> referenceConnectorReverseRelation(
    const StagedObjectView& staged) {
    std::map<ObjectId, std::vector<ObjectId>> relation;
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
    return relation;
}

std::vector<ObjectId> connectorsReferencing(const StagedObjectView& staged,
                                            const ObjectId& target) {
    std::vector<ObjectId> result;
    if (const auto* indexed = dynamic_cast<const IndexedObjectStore*>(&staged.base_)) {
        result = indexedConnectorsReferencing(*indexed, target);
    } else {
        for (const ObjectRecord& record : staged.base_.allObjects())
            if (referencesTarget(record, target)) result.push_back(record.id);
    }
    auto references = [&](const ObjectRecord& record) {
        if (record.kind != ObjectKind::kConnector || record.kind_version != 1U) return false;
        const auto* content = std::get_if<ConnectorContent>(&record.content);
        if (content == nullptr) return false;
        for (const ConnectorEndpoint* endpoint : {&content->start, &content->end}) {
            if (const auto* attached = std::get_if<AttachedEndpoint>(&endpoint->value);
                attached != nullptr && attached->target_object_id == target) return true;
        }
        return false;
    };
    std::set<ObjectId> ids(result.begin(), result.end());
    for (const ObjectId& id : staged.deletes_) ids.erase(id);
    for (const auto& [id, replacement] : staged.replacements_) {
        ids.erase(id);
        if (references(replacement)) ids.insert(id);
    }
    for (const auto& [id, created] : staged.creates_) if (references(created)) ids.insert(id);
    result.assign(ids.begin(), ids.end());
    return result;
}
} // namespace internal

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
