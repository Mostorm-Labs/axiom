#include "atomic_apply.hpp"

#include "object_store_mutator.hpp"

#include <exception>
#include <set>
#include <utility>

namespace canvas::semantic::internal {
namespace {

template <typename Store>
bool preflight(const Store& store, const PreparedApplyPlan& plan) {
    std::set<ObjectId> create_ids;
    std::set<ObjectId> replacement_ids;
    std::set<ObjectId> delete_ids;

    for (const ObjectRecord& record : plan.creates) {
        if (!create_ids.insert(record.id).second || store.contains(record.id)) {
            return false;
        }
    }
    for (const ObjectRecord& record : plan.replacements) {
        if (!replacement_ids.insert(record.id).second || !store.contains(record.id)) {
            return false;
        }
    }
    for (const ObjectId& object_id : plan.deletes) {
        if (!delete_ids.insert(object_id).second || !store.contains(object_id)) {
            return false;
        }
    }

    for (const ObjectId& object_id : create_ids) {
        if (replacement_ids.contains(object_id) || delete_ids.contains(object_id)) {
            return false;
        }
    }
    for (const ObjectId& object_id : replacement_ids) {
        if (delete_ids.contains(object_id)) {
            return false;
        }
    }
    return true;
}

template <typename Store>
AtomicApplyResult applyPreparedPlanInternal(Store& store, const PreparedApplyPlan& plan) {
    if (!preflight(store, plan)) {
        return {};
    }

    for (const ObjectRecord& record : plan.creates) {
        if (!ObjectStoreMutator::insertFresh(store, record)) {
            std::terminate();
        }
    }
    for (const ObjectRecord& record : plan.replacements) {
        if (!ObjectStoreMutator::replaceExisting(store, record)) {
            std::terminate();
        }
    }
    for (const ObjectId& object_id : plan.deletes) {
        if (!ObjectStoreMutator::eraseExisting(store, object_id)) {
            std::terminate();
        }
    }
    return {AtomicApplyStatus::kApplied};
}

} // namespace

AtomicApplyResult applyPreparedPlan(ReferenceObjectStore& store, const PreparedApplyPlan& plan) {
    return applyPreparedPlanInternal(store, plan);
}

AtomicApplyResult applyPreparedPlan(IndexedObjectStore& store, const PreparedApplyPlan& plan) {
    return applyPreparedPlanInternal(store, plan);
}

} // namespace canvas::semantic::internal
