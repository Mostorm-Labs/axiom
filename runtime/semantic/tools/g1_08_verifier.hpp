#pragma once

#include "g1_08_locality_workload.hpp"

#include <cstddef>
#include <array>

namespace canvas::verification::g1_08 {

struct VerificationSummary final {
    bool correctness_pass = false;
    bool locality_pass = false;
    std::size_t scales_checked = 0;
    LocalityWorkloadResult locality{};
    std::array<LocalityWorkloadResult, 3> local_mutation{};
    std::array<LocalityWorkloadResult, 3> hierarchy{};
    std::array<LocalityWorkloadResult, 3> connector_delete{};
    std::array<LocalityWorkloadResult, 3> controlled_cascade{};
    bool local_mutation_access_shape_pass = false;
    bool hierarchy_access_shape_pass = false;
    bool cascade_access_shape_pass = false;
    bool delete_reverse_scan_observed = false;
};

[[nodiscard]] inline bool sameIndexedAccessShape(
    const canvas::semantic::internal::IndexedAccessProbeSnapshot& left,
    const canvas::semantic::internal::IndexedAccessProbeSnapshot& right) {
    return left.all_objects_calls == right.all_objects_calls &&
           left.all_objects_records_materialized == right.all_objects_records_materialized &&
           left.find_calls == right.find_calls &&
           left.contains_calls == right.contains_calls &&
           left.children_calls == right.children_calls &&
           left.children_records_materialized == right.children_records_materialized &&
           left.insert_fresh_calls == right.insert_fresh_calls &&
           left.replace_existing_calls == right.replace_existing_calls &&
           left.erase_existing_calls == right.erase_existing_calls &&
           left.index_rebuild_check_calls == right.index_rebuild_check_calls;
}
[[nodiscard]] inline bool controlledCascadeAccessShapeMatchesClosure(
    const LocalityWorkloadResult& workload) {
    const auto& access = workload.access;
    return workload.total_objects == 100000U && workload.affected_objects > 0U &&
           access.all_objects_calls == 0U && access.all_objects_records_materialized == 0U &&
           access.find_calls == 1U && access.contains_calls == workload.affected_objects &&
           access.children_calls == workload.affected_objects &&
           access.children_records_materialized == 0U && access.insert_fresh_calls == 0U &&
           access.replace_existing_calls == 0U &&
           access.erase_existing_calls == workload.affected_objects &&
           access.index_rebuild_check_calls == 0U;
}
[[nodiscard]] VerificationSummary verifyReferenceIndexedAndLocality();

} // namespace canvas::verification::g1_08
