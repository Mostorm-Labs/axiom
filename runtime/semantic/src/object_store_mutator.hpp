#pragma once

#include "canvas/semantic/reference_object_store.hpp"

namespace canvas::semantic {
class IndexedObjectStore;
}

namespace canvas::semantic::internal {

// Internal/test/bootstrap-only mutation seam. Its boolean result reports only
// a store precondition; it is deliberately not an Operation outcome.
class ObjectStoreMutator final {
  public:
    [[nodiscard]] static bool insertFresh(ReferenceObjectStore& store, ObjectRecord record);
    [[nodiscard]] static bool replaceExisting(ReferenceObjectStore& store, ObjectRecord record);
    [[nodiscard]] static bool eraseExisting(ReferenceObjectStore& store, const ObjectId& id);

    [[nodiscard]] static bool insertFresh(IndexedObjectStore& store, ObjectRecord record);
    [[nodiscard]] static bool replaceExisting(IndexedObjectStore& store, ObjectRecord record);
    [[nodiscard]] static bool eraseExisting(IndexedObjectStore& store, const ObjectId& id);
    [[nodiscard]] static bool indexMatchesRebuild(const IndexedObjectStore& store);
};

} // namespace canvas::semantic::internal
