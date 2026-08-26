#pragma once

#include "canvas/semantic/object_store.hpp"

#include <memory>
#include <vector>

namespace canvas::semantic {

namespace internal {
class ObjectIndex;
class ObjectStoreMutator;
} // namespace internal

// Production-oriented canonical record store. Its only derived acceleration is
// a private, rebuildable parent/children index; neither that index nor its
// traversal tie-break is a serialized or product-facing semantic contract.
class IndexedObjectStore final : public ObjectStore {
  public:
    IndexedObjectStore();
    ~IndexedObjectStore() override;

    IndexedObjectStore(const IndexedObjectStore&) = delete;
    IndexedObjectStore& operator=(const IndexedObjectStore&) = delete;
    IndexedObjectStore(IndexedObjectStore&&) = delete;
    IndexedObjectStore& operator=(IndexedObjectStore&&) = delete;

    [[nodiscard]] std::size_t size() const noexcept override;
    [[nodiscard]] bool contains(const ObjectId& id) const noexcept override;
    [[nodiscard]] const ObjectRecord* find(const ObjectId& id) const noexcept override;
    [[nodiscard]] std::vector<ObjectRecord> allObjects() const override;
    [[nodiscard]] std::vector<ObjectRecord> children(
        const std::optional<ObjectId>& parent_id) const override;

  private:
    friend class internal::ObjectStoreMutator;

    [[nodiscard]] bool insertFreshInternal(ObjectRecord record);
    [[nodiscard]] bool replaceExistingInternal(ObjectRecord record);
    [[nodiscard]] bool eraseExistingInternal(const ObjectId& id);
    [[nodiscard]] bool indexMatchesRebuildInternal() const;

    class Storage;
    std::unique_ptr<Storage> storage_;
    std::unique_ptr<internal::ObjectIndex> object_index_;
};

} // namespace canvas::semantic
