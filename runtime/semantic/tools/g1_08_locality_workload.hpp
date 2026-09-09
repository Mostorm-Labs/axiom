#pragma once

#include "g1_08_indexed_access_probe_internal.hpp"

#include <cstddef>


namespace canvas::verification::g1_08 {

struct LocalityWorkloadResult final {
    const char* workload = "";
    std::size_t total_objects = 0;
    std::size_t affected_objects = 0;
    bool applied = false;
    canvas::semantic::internal::IndexedAccessProbeSnapshot access{};
};

[[nodiscard]] LocalityWorkloadResult runIndexedLocalityWorkload(std::size_t total_objects);
[[nodiscard]] LocalityWorkloadResult runHierarchyWorkload(std::size_t total_objects);
[[nodiscard]] LocalityWorkloadResult runConnectorDeleteWorkload(std::size_t total_objects);
[[nodiscard]] LocalityWorkloadResult runControlledCascadeWorkload(std::size_t connector_count);

} // namespace canvas::verification::g1_08
