#pragma once

#include "g1_08_locality_workload.hpp"

#include <cstddef>

namespace canvas::verification::g1_08 {

struct VerificationSummary final {
    bool correctness_pass = false;
    bool locality_pass = false;
    std::size_t scales_checked = 0;
    LocalityWorkloadResult locality{};
    bool delete_reverse_scan_observed = false;
};

[[nodiscard]] VerificationSummary verifyReferenceIndexedAndLocality();

} // namespace canvas::verification::g1_08
