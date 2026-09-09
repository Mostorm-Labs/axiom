#include "g1_08_locality_workload.hpp"

#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/object_record.hpp"
#include "object_store_mutator.hpp"

namespace canvas::verification::g1_08 {
namespace {
canvas::semantic::ObjectRecord record(std::size_t value) {
    using namespace canvas::semantic;
    ObjectRecord result{};
    result.id = ObjectId::fromUint64(static_cast<std::uint64_t>(value + 1U));
    result.kind = ObjectKind::kShape;
    result.kind_version = 1U;
    result.placement = Placement{std::nullopt, OrderKey({1U})};
    result.transform = Transform2D{1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
    result.content = ShapeContent{1U, 1.0, 1.0};
    return result;
}
} // namespace

LocalityWorkloadResult runIndexedLocalityWorkload(std::size_t total_objects) {
    canvas::semantic::IndexedObjectStore store;
    for (std::size_t i = 0U; i < total_objects; ++i) {
        static_cast<void>(canvas::semantic::internal::ObjectStoreMutator::insertFresh(store, record(i)));
    }
    canvas::semantic::internal::resetIndexedAccessProbe();
    canvas::semantic::internal::enableIndexedAccessProbe(true);
    static_cast<void>(store.find(canvas::semantic::ObjectId::fromUint64(1U)));
    const auto access = canvas::semantic::internal::snapshotIndexedAccessProbe();
    canvas::semantic::internal::enableIndexedAccessProbe(false);
    return {total_objects, access};
}

} // namespace canvas::verification::g1_08
