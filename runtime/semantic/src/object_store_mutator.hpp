#pragma once

#include "canvas/semantic/reference_object_store.hpp"
#include <memory>

namespace canvas::semantic {
class IndexedObjectStore;
class ObjectStore;
struct PreparedApplyPlan;
}

namespace canvas::semantic::internal {
using IntentPrepareTestObserver = void (*)(void*, unsigned, std::size_t) noexcept;
void setIntentPrepareTestObserver(IntentPrepareTestObserver, void*) noexcept;
void observeIntentPrepare(unsigned, std::size_t) noexcept;

// Private, closed-domain reservation within the existing canonical store owner.
// It is valid only during one synchronous OperationEngine commit.
class PreparedIndexedContainerApply final {
  public:
    ~PreparedIndexedContainerApply();
    void publish() noexcept;
  private:
    friend class ObjectStoreMutator;
    class State;
    explicit PreparedIndexedContainerApply(std::unique_ptr<State>);
    std::unique_ptr<State> state_;
};

// Internal/test/bootstrap-only mutation seam. Its boolean result reports only
// a store precondition; it is deliberately not an Operation outcome.
class ObjectStoreMutator final {
  public:
    static void setDocumentBackground(ObjectStore& store, const foundation::DocumentBackground& value) noexcept {
        store.background_ = value;
    }
    static void swapDocumentBackground(ObjectStore& left, ObjectStore& right) noexcept {
        std::swap(left.background_, right.background_);
    }
    [[nodiscard]] static bool insertFresh(ReferenceObjectStore& store, ObjectRecord record);
    [[nodiscard]] static bool replaceExisting(ReferenceObjectStore& store, ObjectRecord record);
    [[nodiscard]] static bool eraseExisting(ReferenceObjectStore& store, const ObjectId& id);
    static void swapContents(ReferenceObjectStore&, ReferenceObjectStore&) noexcept;

    [[nodiscard]] static bool insertFresh(IndexedObjectStore& store, ObjectRecord record);
    [[nodiscard]] static bool replaceExisting(IndexedObjectStore& store, ObjectRecord record);
    [[nodiscard]] static bool eraseExisting(IndexedObjectStore& store, const ObjectId& id);
    [[nodiscard]] static std::unique_ptr<PreparedIndexedContainerApply> prepareContainerPlan(
        IndexedObjectStore&, const PreparedApplyPlan&, bool admitted = false,
        const ObjectStore* stagedBefore = nullptr, std::size_t intentOperation = 0);
    [[nodiscard]] static bool indexMatchesRebuild(const IndexedObjectStore& store);
    [[nodiscard]] static std::vector<ObjectId> connectorsReferencing(
        const IndexedObjectStore& store, const ObjectId& target);
};

} // namespace canvas::semantic::internal
