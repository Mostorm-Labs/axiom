#pragma once

#include <cstddef>
#include <vector>
#include "canvas/semantic/object_record.hpp"

namespace canvas::semantic { class IndexedObjectStore; }

namespace canvas::semantic { class StagedObjectView; class ObjectStore; }

namespace canvas::semantic::internal {

void noteStagedBase(const canvas::semantic::StagedObjectView& staged, const canvas::semantic::ObjectStore& store);
void noteStagedCreate(const canvas::semantic::StagedObjectView& staged, const ObjectRecord& record);
void noteStagedReplace(const canvas::semantic::StagedObjectView& staged, const ObjectRecord& record);
void noteStagedDelete(const canvas::semantic::StagedObjectView& staged, const ObjectId& id);
[[nodiscard]] bool stagedUsesIndexed(const canvas::semantic::StagedObjectView& staged);
[[nodiscard]] std::vector<ObjectId> stagedConnectorsReferencing(
    const canvas::semantic::StagedObjectView& staged, const ObjectId& target);

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
[[nodiscard]] std::vector<ObjectId> indexedConnectorsReferencing(
    const IndexedObjectStore& store, const ObjectId& target);

} // namespace canvas::semantic::internal
