#pragma once

#include "canvas/semantic/apply_plan.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/resource_manifest.hpp"

#include <cstdint>
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
namespace canvas::qualification { struct ApplyContext; }
#endif

namespace canvas::semantic::internal {

enum class AtomicApplyStatus : std::uint8_t {
    kApplied = 0,
    kPreconditionFailed,
};

struct AtomicApplyResult final {
    AtomicApplyStatus status = AtomicApplyStatus::kPreconditionFailed;
};

[[nodiscard]] AtomicApplyResult applyPreparedPlan(
    ReferenceObjectStore& store,
    const PreparedApplyPlan& plan
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
    , const canvas::qualification::ApplyContext* context = nullptr
#endif
    );

[[nodiscard]] AtomicApplyResult applyPreparedPlan(
    IndexedObjectStore& store,
    const PreparedApplyPlan& plan
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
    , const canvas::qualification::ApplyContext* context = nullptr
#endif
    );

[[nodiscard]] AtomicApplyResult applyPreparedPlan(
    ReferenceObjectStore& store,
    ResourceManifest& manifest,
    const PreparedApplyPlan& plan
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
    , const canvas::qualification::ApplyContext* context = nullptr
#endif
    );

[[nodiscard]] AtomicApplyResult applyPreparedPlan(
    IndexedObjectStore& store,
    ResourceManifest& manifest,
    const PreparedApplyPlan& plan
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
    , const canvas::qualification::ApplyContext* context = nullptr
#endif
    );

} // namespace canvas::semantic::internal
