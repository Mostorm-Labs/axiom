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

[[nodiscard]] VerificationSummary verifyReferenceIndexedAndLocality();

} // namespace canvas::verification::g1_08
