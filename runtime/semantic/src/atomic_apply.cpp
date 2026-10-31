#include "atomic_apply.hpp"

#include "object_store_mutator.hpp"

#include <exception>
#include <new>
#include <type_traits>
#include <set>
#include <utility>
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
#include "qualification_checkpoint.hpp"
#define FC08_CONTEXT_PARAMETER , const canvas::qualification::ApplyContext* fc08Context
#define FC08_CONTEXT_ARGUMENT , fc08Context
#else
#define FC08_CONTEXT_PARAMETER
#define FC08_CONTEXT_ARGUMENT
#endif

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
AtomicApplyResult applyPreparedPlanInternal(Store& store, const PreparedApplyPlan& plan, bool reserved FC08_CONTEXT_PARAMETER) {
    if (!preflight(store, plan)) {
        return {};
    }

    if (plan.background) {
        if (!foundation::validDocumentBackground(*plan.background) || !plan.creates.empty() ||
            !plan.replacements.empty() || !plan.deletes.empty() || plan.manifest) return {};
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
        if (canvas::qualification::notify(fc08Context, canvas::qualification::Point::kSemPublish, plan.operation)) return {};
#endif
        ObjectStoreMutator::setDocumentBackground(store, *plan.background);
        return {AtomicApplyStatus::kApplied};
    }

    if (reserved || plan.operation.kind() == OperationKind::kSetSectionContent || plan.operation.kind() == OperationKind::kEditContainerTopology) {
        try {
            if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
                auto storage = ObjectStoreMutator::prepareContainerPlan(store, plan, reserved);
                if (!storage) return {};
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
                if (canvas::qualification::notify(fc08Context, canvas::qualification::Point::kSemPublish, plan.operation)) return {};
#endif
                storage->publish();
            } else {
                // Independent reference storage remains correctness-first.
                Store next = store;
                for (const auto& record : plan.creates)
                    if (!ObjectStoreMutator::insertFresh(next, record)) return {};
                for (const auto& record : plan.replacements)
                    if (!ObjectStoreMutator::replaceExisting(next, record)) return {};
                for (const auto& id : plan.deletes)
                    if (!ObjectStoreMutator::eraseExisting(next, id)) return {};
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
                if (canvas::qualification::notify(fc08Context, canvas::qualification::Point::kSemPublish, plan.operation)) return {};
#endif
                ObjectStoreMutator::swapContents(store, next);
            }
            return {AtomicApplyStatus::kApplied};
        } catch (const std::bad_alloc&) {
            return {};
        }
    }

#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
    if ((!plan.creates.empty() || !plan.replacements.empty() || !plan.deletes.empty()) &&
        canvas::qualification::notify(fc08Context, canvas::qualification::Point::kSemPublish, plan.operation)) {
        return {};
    }
#endif
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

AtomicApplyResult applyPreparedPlan(ReferenceObjectStore& store, const PreparedApplyPlan& plan, bool reserved FC08_CONTEXT_PARAMETER) {
    return applyPreparedPlanInternal(store, plan, reserved FC08_CONTEXT_ARGUMENT);
}

AtomicApplyResult applyPreparedPlan(IndexedObjectStore& store, const PreparedApplyPlan& plan, bool reserved FC08_CONTEXT_PARAMETER) {
    return applyPreparedPlanInternal(store, plan, reserved FC08_CONTEXT_ARGUMENT);
}

AtomicApplyResult applyPreparedPlan(ReferenceObjectStore& store, ResourceManifest& manifest, const PreparedApplyPlan& plan, bool reserved FC08_CONTEXT_PARAMETER) {
    auto result = applyPreparedPlan(store, plan, reserved FC08_CONTEXT_ARGUMENT);
    if (result.status == AtomicApplyStatus::kApplied && plan.manifest.has_value()) manifest = *plan.manifest;
    return result;
}
AtomicApplyResult applyPreparedPlan(IndexedObjectStore& store, ResourceManifest& manifest, const PreparedApplyPlan& plan, bool reserved FC08_CONTEXT_PARAMETER) {
    auto result = applyPreparedPlan(store, plan, reserved FC08_CONTEXT_ARGUMENT);
    if (result.status == AtomicApplyStatus::kApplied && plan.manifest.has_value()) manifest = *plan.manifest;
    return result;
}

} // namespace canvas::semantic::internal
