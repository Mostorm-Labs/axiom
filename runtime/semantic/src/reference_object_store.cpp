#include "canvas/semantic/reference_object_store.hpp"

#include "object_store_mutator.hpp"

#include <algorithm>
#include <utility>

namespace canvas::semantic {

namespace {

auto recordById(const ObjectId& id) {
    return [id](const ObjectRecord& record) { return record.id == id; };
}

bool childBefore(const ObjectRecord& left, const ObjectRecord& right) {
    if (const auto order = left.placement.order_key <=> right.placement.order_key; order != 0) {
        return order < 0;
    }
    return left.id < right.id;
}

} // namespace

std::size_t ReferenceObjectStore::size() const noexcept {
    return records_.size();
}

bool ReferenceObjectStore::contains(const ObjectId& id) const noexcept {
    return find(id) != nullptr;
}

const ObjectRecord* ReferenceObjectStore::find(const ObjectId& id) const noexcept {
    const auto it = std::find_if(records_.begin(), records_.end(), recordById(id));
    return it == records_.end() ? nullptr : &*it;
}

std::vector<ObjectRecord> ReferenceObjectStore::allObjects() const {
    auto result = records_;
    std::sort(result.begin(), result.end(), [](const ObjectRecord& left, const ObjectRecord& right) {
        return left.id < right.id;
    });
    return result;
}

std::vector<ObjectRecord> ReferenceObjectStore::children(
    const std::optional<ObjectId>& parent_id) const {
    std::vector<ObjectRecord> result;
    for (const auto& record : records_) {
        if (record.placement.parent_id == parent_id) {
            result.push_back(record);
        }
    }
    std::sort(result.begin(), result.end(), childBefore);
    return result;
}

bool ReferenceObjectStore::insertFreshInternal(ObjectRecord record) {
    if (contains(record.id)) {
        return false;
    }
    records_.push_back(std::move(record));
    return true;
}

bool ReferenceObjectStore::replaceExistingInternal(ObjectRecord record) {
    const auto it = std::find_if(records_.begin(), records_.end(), recordById(record.id));
    if (it == records_.end()) {
        return false;
    }
    *it = std::move(record);
    return true;
}

bool ReferenceObjectStore::eraseExistingInternal(const ObjectId& id) {
    const auto it = std::find_if(records_.begin(), records_.end(), recordById(id));
    if (it == records_.end()) {
        return false;
    }
    records_.erase(it);
    return true;
}

} // namespace canvas::semantic

namespace canvas::semantic::internal {

bool ObjectStoreMutator::insertFresh(ReferenceObjectStore& store, ObjectRecord record) {
    return store.insertFreshInternal(std::move(record));
}

bool ObjectStoreMutator::replaceExisting(ReferenceObjectStore& store, ObjectRecord record) {
    return store.replaceExistingInternal(std::move(record));
}

bool ObjectStoreMutator::eraseExisting(ReferenceObjectStore& store, const ObjectId& id) {
    return store.eraseExistingInternal(id);
}

} // namespace canvas::semantic::internal
