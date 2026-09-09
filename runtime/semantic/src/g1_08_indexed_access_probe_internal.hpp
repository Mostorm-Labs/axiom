#pragma once

#include <cstddef>

namespace canvas::semantic::internal {

struct IndexedAccessProbeSnapshot final {
    std::size_t all_objects_calls = 0;
    std::size_t all_objects_records_materialized = 0;
    std::size_t find_calls = 0;
    std::size_t contains_calls = 0;
    std::size_t children_calls = 0;
    std::size_t children_records_materialized = 0;
    std::size_t insert_fresh_calls = 0;
    std::size_t replace_existing_calls = 0;
    std::size_t erase_existing_calls = 0;
    std::size_t index_rebuild_check_calls = 0;
};

void resetIndexedAccessProbe();
void enableIndexedAccessProbe(bool enabled);
[[nodiscard]] IndexedAccessProbeSnapshot snapshotIndexedAccessProbe();

} // namespace canvas::semantic::internal
