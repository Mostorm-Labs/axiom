#pragma once

#include "canvas/scene/spatial_index.hpp"

namespace canvas {

class LinearSpatialIndex final : public ISpatialIndex {
  public:
    foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>> prepareReplace(
        std::span<const SpatialRecord> records, SceneRevision revision) const override;
    foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>> prepareApply(
        std::span<const SpatialMutation> mutations, SceneRevision beforeRevision,
        SceneRevision afterRevision) const override;
    foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>> prepareDelta(
        const SceneDelta& delta, SceneRevision beforeRevision,
        SceneRevision afterRevision) const override;
    void commit(std::unique_ptr<IPreparedSpatialUpdate> update) noexcept override;
    foundation::Result<SpatialQueryResult> query(const WorldRect& worldRect) const override;
    [[nodiscard]] SpatialIndexDiagnostics diagnostics() const override;

  private:
    class Prepared;
    mutable std::uint64_t _prepareCount = 0;
    std::uint64_t _commitCount = 0;
    SceneRevision _revision;
    std::vector<SpatialRecord> _records;
};

} // namespace canvas
