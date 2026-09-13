#pragma once

#include "canvas/foundation/result.hpp"
#include "canvas/scene/scene_types.hpp"
#include "canvas/scene/scene_delta.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace canvas {

struct SpatialRecord final {
    ObjectId objectId;
    WorldRect worldBounds;

    bool operator==(const SpatialRecord&) const = default;
};

struct SpatialMutation final {
    SceneMutationKind kind = SceneMutationKind::kInsert;
    ObjectId objectId;
    std::optional<WorldRect> before;
    std::optional<WorldRect> after;
};

using SpatialEntryId = std::uint64_t;

struct SpatialDelta final {
    SceneRevision generationFrom;
    SceneRevision generationTo;
    std::vector<SpatialMutation> mutations;
};

class IPreparedSpatialUpdate {
  public:
    virtual ~IPreparedSpatialUpdate() = default;
};

struct SpatialQueryResult final {
    std::vector<ObjectId> candidates;
    std::uint64_t examinedRecords = 0;
};

struct SpatialIndexDiagnostics final {
    SceneRevision revision;
    std::uint64_t recordCount = 0;
    std::uint64_t prepareCount = 0;
    std::uint64_t commitCount = 0;
    std::uint64_t lastExaminedRecords = 0;
    std::uint64_t lastReturnedCandidates = 0;
    std::uint64_t lastCellVisits = 0;
    std::size_t estimatedBytes = 0;
    std::uint64_t fullRecordCloneCount = 0;
    std::uint64_t fullSortCount = 0;
    std::uint64_t fullReindexCount = 0;
    std::uint64_t fullRebuildCount = 0;
    std::uint64_t localizedMutationCount = 0;
    std::uint64_t fullSpatialRebuildCount = 0;
    std::uint64_t fullSpatialRecordCloneCount = 0;
    std::uint64_t fullCellScanCount = 0;
    std::uint64_t affectedEntryCount = 0;
    std::uint64_t oldCoverageUnitsVisited = 0;
    std::uint64_t newCoverageUnitsVisited = 0;
    std::uint64_t membershipRemovalCount = 0;
    std::uint64_t membershipRetainedCount = 0;
    std::uint64_t membershipAddCount = 0;
};

class ISpatialIndex {
  public:
    virtual ~ISpatialIndex() = default;

    virtual foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>
    prepareReplace(std::span<const SpatialRecord> records, SceneRevision revision) const = 0;

    virtual foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>
    prepareApply(std::span<const SpatialMutation> mutations,
                 SceneRevision beforeRevision,
                 SceneRevision afterRevision) const = 0;

    virtual foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>
    prepareDelta(const SceneDelta& delta,
                 SceneRevision beforeRevision,
                 SceneRevision afterRevision) const {
        std::vector<SpatialMutation> mutations;
        mutations.reserve(delta.mutations.size());
        for (const SceneMutation& mutation : delta.mutations) {
            mutations.push_back(SpatialMutation{
                .kind = mutation.kind,
                .objectId = mutation.objectId,
                .before = mutation.before ? std::optional(mutation.before->worldBounds)
                                           : std::nullopt,
                .after = mutation.after ? std::optional(mutation.after->worldBounds)
                                         : std::nullopt,
            });
        }
        return prepareApply(mutations, beforeRevision, afterRevision);
    }

    virtual void commit(std::unique_ptr<IPreparedSpatialUpdate> update) noexcept = 0;

    virtual foundation::Result<SpatialQueryResult> query(const WorldRect& worldRect) const = 0;
    [[nodiscard]] virtual SpatialIndexDiagnostics diagnostics() const = 0;
};

} // namespace canvas
