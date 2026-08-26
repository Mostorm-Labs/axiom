#include "canvas/semantic/indexed_object_store.hpp"

#include "object_index.hpp"
#include "object_store_mutator.hpp"

#include <map>
#include <utility>

namespace canvas::semantic {

class IndexedObjectStore::Storage final {
  public:
    std::map<ObjectId, ObjectRecord> records;
};

IndexedObjectStore::IndexedObjectStore()
    : storage_(std::make_unique<Storage>()), object_index_(std::make_unique<internal::ObjectIndex>()) {}

IndexedObjectStore::~IndexedObjectStore() = default;

std::size_t IndexedObjectStore::size() const noexcept {
    return storage_->records.size();
}

bool IndexedObjectStore::contains(const ObjectId& id) const noexcept {
    return storage_->records.contains(id);
}

const ObjectRecord* IndexedObjectStore::find(const ObjectId& id) const noexcept {
    const auto record_it = storage_->records.find(id);
    return record_it == storage_->records.end() ? nullptr : &record_it->second;
}

std::vector<ObjectRecord> IndexedObjectStore::allObjects() const {
    std::vector<ObjectRecord> result;
    result.reserve(storage_->records.size());
    for (const auto& [id, record] : storage_->records) {
        static_cast<void>(id);
        result.push_back(record);
    }
    return result;
}

std::vector<ObjectRecord> IndexedObjectStore::children(
    const std::optional<ObjectId>& parent_id) const {
    std::vector<ObjectRecord> result;
    const std::vector<ObjectId> child_ids = object_index_->children(parent_id);
    result.reserve(child_ids.size());
    for (const ObjectId& child_id : child_ids) {
        const auto record_it = storage_->records.find(child_id);
        if (record_it != storage_->records.end()) {
            result.push_back(record_it->second);
        }
    }
    return result;
}

bool IndexedObjectStore::insertFreshInternal(ObjectRecord record) {
    const auto [record_it, inserted] = storage_->records.emplace(record.id, std::move(record));
    if (!inserted) {
        return false;
    }
    object_index_->insert(record_it->second);
    return true;
}

bool IndexedObjectStore::replaceExistingInternal(ObjectRecord record) {
    const auto record_it = storage_->records.find(record.id);
    if (record_it == storage_->records.end()) {
        return false;
    }

    object_index_->erase(record_it->second);
    record_it->second = std::move(record);
    object_index_->insert(record_it->second);
    return true;
}

bool IndexedObjectStore::eraseExistingInternal(const ObjectId& id) {
    const auto record_it = storage_->records.find(id);
    if (record_it == storage_->records.end()) {
        return false;
    }

    object_index_->erase(record_it->second);
    storage_->records.erase(record_it);
    return true;
}

bool IndexedObjectStore::indexMatchesRebuildInternal() const {
    internal::ObjectIndex rebuilt;
    for (const auto& [id, record] : storage_->records) {
        static_cast<void>(id);
        rebuilt.insert(record);
    }
    return object_index_->equals(rebuilt);
}

} // namespace canvas::semantic

namespace canvas::semantic::internal {

bool ObjectStoreMutator::insertFresh(IndexedObjectStore& store, ObjectRecord record) {
    return store.insertFreshInternal(std::move(record));
}

bool ObjectStoreMutator::replaceExisting(IndexedObjectStore& store, ObjectRecord record) {
    return store.replaceExistingInternal(std::move(record));
}

bool ObjectStoreMutator::eraseExisting(IndexedObjectStore& store, const ObjectId& id) {
    return store.eraseExistingInternal(id);
}

bool ObjectStoreMutator::indexMatchesRebuild(const IndexedObjectStore& store) {
    return store.indexMatchesRebuildInternal();
}

} // namespace canvas::semantic::internal
