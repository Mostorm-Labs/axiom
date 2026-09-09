#pragma once

#include "g1_08_indexed_access_probe_internal.hpp"

#include <cstddef>


namespace canvas::verification::g1_08 {

struct LocalityWorkloadResult final {
    std::size_t total_objects = 0;
    canvas::semantic::internal::IndexedAccessProbeSnapshot access{};
};

[[nodiscard]] LocalityWorkloadResult runIndexedLocalityWorkload(std::size_t total_objects);

} // namespace canvas::verification::g1_08
