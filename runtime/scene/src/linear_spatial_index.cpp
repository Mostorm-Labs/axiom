#include "canvas/scene/linear_spatial_index.hpp"

#include <algorithm>
#include <new>

namespace canvas {
class LinearSpatialIndex::Prepared final : public IPreparedSpatialUpdate {
  public:
    std::vector<SpatialRecord> records;
    SceneRevision revision;
};

namespace {
foundation::Error error(foundation::ErrorCode code, const char* message) { return {code, message}; }
}

foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>> LinearSpatialIndex::prepareReplace(
    std::span<const SpatialRecord> records, SceneRevision revision) const {
    ++_prepareCount;
    try {
        auto update = std::make_unique<Prepared>();
        update->records.assign(records.begin(), records.end());
        update->revision = revision;
        return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::success(std::move(update));
    } catch (const std::bad_alloc&) {
        return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(error(foundation::ErrorCode::kOutOfMemory, "Linear spatial prepare failed"));
    }
}

foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>> LinearSpatialIndex::prepareApply(
    std::span<const SpatialMutation> mutations, SceneRevision beforeRevision,
    SceneRevision afterRevision) const {
    ++_prepareCount;
    if (beforeRevision != _revision || afterRevision <= beforeRevision)
        return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(error(foundation::ErrorCode::kInvalidRevision, "Linear spatial revision invalid"));
    try {
        std::vector<SpatialRecord> next = _records;
        for (const auto& mutation : mutations) {
            auto it = std::find_if(next.begin(), next.end(), [&](const auto& r){ return r.objectId == mutation.objectId; });
            if (mutation.kind == SceneMutationKind::kInsert) {
                if (it != next.end() || !mutation.after || mutation.before) return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(error(foundation::ErrorCode::kDuplicateObject, "Linear insert invalid"));
                next.push_back({mutation.objectId, *mutation.after});
            } else if (it == next.end() || !mutation.before || (it->worldBounds != *mutation.before)) {
                return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(error(foundation::ErrorCode::kBeforeImageMismatch, "Linear before image invalid"));
            } else if (mutation.kind == SceneMutationKind::kUpdate && mutation.after) it->worldBounds = *mutation.after;
            else if (mutation.kind == SceneMutationKind::kRemove && !mutation.after) next.erase(it);
            else return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(error(foundation::ErrorCode::kInvalidRecord, "Linear mutation invalid"));
        }
        auto update = std::make_unique<Prepared>(); update->records = std::move(next); update->revision = afterRevision;
        return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::success(std::move(update));
    } catch (const std::bad_alloc&) { return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(error(foundation::ErrorCode::kOutOfMemory, "Linear spatial prepare failed")); }
}

foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>> LinearSpatialIndex::prepareDelta(
    const SceneDelta& delta, SceneRevision beforeRevision, SceneRevision afterRevision) const {
    std::vector<SpatialMutation> mutations;
    mutations.reserve(delta.mutations.size());
    for (const auto& mutation : delta.mutations) {
        mutations.push_back({mutation.kind, mutation.objectId,
                              mutation.before ? std::optional(mutation.before->worldBounds) : std::nullopt,
                              mutation.after ? std::optional(mutation.after->worldBounds) : std::nullopt});
    }
    return prepareApply(mutations, beforeRevision, afterRevision);
}

void LinearSpatialIndex::commit(std::unique_ptr<IPreparedSpatialUpdate> update) noexcept {
    auto* prepared = static_cast<Prepared*>(update.get()); _records.swap(prepared->records); _revision = prepared->revision; ++_commitCount;
}

foundation::Result<SpatialQueryResult> LinearSpatialIndex::query(const WorldRect& worldRect) const {
    if (!worldRect.isFiniteAndOrdered()) return foundation::Result<SpatialQueryResult>::failure(error(foundation::ErrorCode::kInvalidArgument, "Linear query bounds invalid"));
    SpatialQueryResult result; result.examinedRecords = _records.size();
    for (const auto& record : _records) if (record.worldBounds.intersects(worldRect)) result.candidates.push_back(record.objectId);
    return foundation::Result<SpatialQueryResult>::success(std::move(result));
}

SpatialIndexDiagnostics LinearSpatialIndex::diagnostics() const { return {.revision = _revision, .recordCount = _records.size(), .prepareCount = _prepareCount, .commitCount = _commitCount, .lastExaminedRecords = _records.size()}; }
} // namespace canvas
