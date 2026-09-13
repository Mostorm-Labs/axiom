#pragma once

#include "canvas/scene/spatial_index.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <vector>

namespace canvas {

class UniformGridSpatialIndex final : public ISpatialIndex {
  public:
    explicit UniformGridSpatialIndex(float cellSize = 256.0F);

    foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>
    prepareReplace(std::span<const SpatialRecord> records, SceneRevision revision) const override;
    foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>
    prepareApply(std::span<const SpatialMutation> mutations,
                 SceneRevision beforeRevision,
                 SceneRevision afterRevision) const override;
    foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>
    prepareDelta(const SceneDelta& delta,
                 SceneRevision beforeRevision,
                 SceneRevision afterRevision) const override;
    foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>
    prepareSpatialDelta(const SpatialDelta& delta,
                        SceneRevision beforeRevision,
                        SceneRevision afterRevision) const override;
    void commit(std::unique_ptr<IPreparedSpatialUpdate> update) noexcept override;
    foundation::Result<SpatialQueryResult> query(const WorldRect& worldRect) const override;
    [[nodiscard]] SpatialIndexDiagnostics diagnostics() const override;

  private:
    struct PreparedMutation;
    class PreparedGridUpdate;

    foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>
    prepareRecords(std::vector<SpatialRecord> records, SceneRevision revision) const;
    foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>
    prepareMutations(std::span<const SpatialMutation> mutations,
                     SceneRevision beforeRevision,
                     SceneRevision afterRevision) const;

    float _cellSize;
    mutable std::uint64_t _prepareCount = 0;
    mutable std::uint64_t _lastExaminedRecords = 0;
    mutable std::uint64_t _lastReturnedCandidates = 0;
    mutable std::uint64_t _lastCellVisits = 0;
    std::uint64_t _commitCount = 0;
    mutable std::uint64_t _localizedMutationCount = 0;
    mutable std::uint64_t _fullSpatialRebuildCount = 0;
    mutable std::uint64_t _fullSpatialRecordCloneCount = 0;
    mutable std::uint64_t _fullCellScanCount = 0;
    mutable std::uint64_t _affectedEntryCount = 0;
    mutable std::uint64_t _oldCoverageUnitsVisited = 0;
    mutable std::uint64_t _newCoverageUnitsVisited = 0;
    mutable std::uint64_t _membershipRemovalCount = 0;
    mutable std::uint64_t _membershipRetainedCount = 0;
    mutable std::uint64_t _membershipAddCount = 0;
    SceneRevision _revision;
    std::map<SpatialEntryId, SpatialRecord> _records;
    SpatialEntryId _nextEntryId = 1;
    std::map<ObjectId, SpatialEntryId> _index;
    std::map<std::int64_t, std::vector<SpatialEntryId>> _cells;
    std::map<SpatialEntryId, WorldRect> _overflow;
};

} // namespace canvas
