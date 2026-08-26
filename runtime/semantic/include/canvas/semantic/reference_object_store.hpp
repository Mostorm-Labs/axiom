#pragma once

#include "canvas/semantic/object_store.hpp"

#include <vector>

namespace canvas::semantic {

namespace internal {
class ObjectStoreMutator;
}

// Correctness-first independent storage oracle. It deliberately does not share
// IndexedObjectStore's lookup container or ObjectIndex update algorithm.
class ReferenceObjectStore final : public ObjectStore {
  public:
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

    std::vector<ObjectRecord> records_;
};

} // namespace canvas::semantic
