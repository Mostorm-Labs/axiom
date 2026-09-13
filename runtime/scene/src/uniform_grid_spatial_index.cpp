#include "canvas/scene/uniform_grid_spatial_index.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace canvas {
class UniformGridSpatialIndex::PreparedGridUpdate final : public IPreparedSpatialUpdate {
  public:
    struct MutationPlan {
        SpatialMutation mutation;
        SpatialEntryId entry = 0;
        std::vector<std::int64_t> removed;
        std::vector<std::int64_t> retained;
        std::vector<std::int64_t> added;
        bool overflowBefore = false;
        std::optional<WorldRect> overflowAfter;
    };

    SceneRevision revision;
    SpatialEntryId nextEntryId = 1;
    std::map<SpatialEntryId, SpatialRecord> records;
    std::map<ObjectId, SpatialEntryId> index;
    std::map<std::int64_t, std::vector<SpatialEntryId>> cells;
    std::map<SpatialEntryId, WorldRect> overflow;
    std::vector<MutationPlan> plans;
    std::uint64_t affectedEntryCount = 0;
    std::uint64_t oldCoverageUnitsVisited = 0;
    std::uint64_t newCoverageUnitsVisited = 0;
    std::uint64_t membershipRemovalCount = 0;
    std::uint64_t membershipRetainedCount = 0;
    std::uint64_t membershipAddCount = 0;
    std::uint64_t localizedMutationCount = 0;
    bool localized = false;
};

namespace {

foundation::Error makeError(foundation::ErrorCode code, const char* message) {
    return foundation::Error{code, message};
}

std::int64_t cellKey(std::int32_t x, std::int32_t y) {
    return static_cast<std::int64_t>(
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32U) |
        static_cast<std::uint32_t>(y));
}

bool canUseOverflow(const WorldRect& bounds, float cellSize) {
    if (!bounds.isFiniteAndOrdered()) {
        return false;
    }
    const double firstXValue = std::floor(bounds.left / cellSize);
    const double firstYValue = std::floor(bounds.top / cellSize);
    const float lastXCoordinate =
        bounds.right > bounds.left
            ? std::nextafter(bounds.right, -std::numeric_limits<float>::infinity())
            : bounds.right;
    const float lastYCoordinate =
        bounds.bottom > bounds.top
            ? std::nextafter(bounds.bottom, -std::numeric_limits<float>::infinity())
            : bounds.bottom;
    const double lastXValue = std::floor(lastXCoordinate / cellSize);
    const double lastYValue = std::floor(lastYCoordinate / cellSize);
    if (firstXValue < std::numeric_limits<std::int32_t>::min() ||
        lastXValue > std::numeric_limits<std::int32_t>::max() ||
        firstYValue < std::numeric_limits<std::int32_t>::min() ||
        lastYValue > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }
    return bounds.right - bounds.left <= 1048576.0F &&
           bounds.bottom - bounds.top <= 1048576.0F;
}

foundation::Result<std::vector<std::int64_t>> cellsFor(const WorldRect& bounds, float cellSize) {
    if (!bounds.isFiniteAndOrdered()) {
        return foundation::Result<std::vector<std::int64_t>>::failure(
            makeError(foundation::ErrorCode::kInvalidArgument, "Spatial bounds are invalid"));
    }
    const double firstXValue = std::floor(bounds.left / cellSize);
    const double firstYValue = std::floor(bounds.top / cellSize);
    const float lastXCoordinate =
        bounds.right > bounds.left
            ? std::nextafter(bounds.right, -std::numeric_limits<float>::infinity())
            : bounds.right;
    const float lastYCoordinate =
        bounds.bottom > bounds.top
            ? std::nextafter(bounds.bottom, -std::numeric_limits<float>::infinity())
            : bounds.bottom;
    const double lastXValue = std::floor(lastXCoordinate / cellSize);
    const double lastYValue = std::floor(lastYCoordinate / cellSize);
    if (firstXValue < std::numeric_limits<std::int32_t>::min() ||
        lastXValue > std::numeric_limits<std::int32_t>::max() ||
        firstYValue < std::numeric_limits<std::int32_t>::min() ||
        lastYValue > std::numeric_limits<std::int32_t>::max()) {
        return foundation::Result<std::vector<std::int64_t>>::failure(
            makeError(foundation::ErrorCode::kInvalidArgument, "Spatial cell overflow"));
    }
    const std::int32_t firstX = static_cast<std::int32_t>(firstXValue);
    const std::int32_t firstY = static_cast<std::int32_t>(firstYValue);
    const std::int32_t lastX = static_cast<std::int32_t>(lastXValue);
    const std::int32_t lastY = static_cast<std::int32_t>(lastYValue);
    const std::uint64_t width = static_cast<std::uint64_t>(static_cast<std::int64_t>(lastX) -
                                                           static_cast<std::int64_t>(firstX) + 1);
    const std::uint64_t height = static_cast<std::uint64_t>(static_cast<std::int64_t>(lastY) -
                                                            static_cast<std::int64_t>(firstY) + 1);
    if (width > 65536U || height > 65536U || width * height > 1048576U) {
        return foundation::Result<std::vector<std::int64_t>>::failure(makeError(
            foundation::ErrorCode::kInvalidArgument, "Spatial record covers too many cells"));
    }
    try {
        std::vector<std::int64_t> result;
        result.reserve(static_cast<std::size_t>(width * height));
        for (std::int64_t y = firstY; y <= lastY; ++y) {
            for (std::int64_t x = firstX; x <= lastX; ++x) {
                result.push_back(
                    cellKey(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)));
            }
        }
        return foundation::Result<std::vector<std::int64_t>>::success(std::move(result));
    } catch (const std::bad_alloc&) {
        return foundation::Result<std::vector<std::int64_t>>::failure(
            makeError(foundation::ErrorCode::kOutOfMemory, "Unable to enumerate spatial cells"));
    }
}

} // namespace

UniformGridSpatialIndex::UniformGridSpatialIndex(float cellSize) : _cellSize(cellSize) {
    if (!std::isfinite(cellSize) || cellSize <= 0.0F) {
        throw std::invalid_argument("spatial cell size must be finite and positive");
    }
}

foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>
UniformGridSpatialIndex::prepareRecords(std::vector<SpatialRecord> records,
                                        SceneRevision revision) const {
    try {
        auto update = std::make_unique<PreparedGridUpdate>();
        update->revision = revision;
        update->nextEntryId = 1;
        for (const SpatialRecord& record : records) {
            const SpatialEntryId entry = update->nextEntryId++;
            if (!update->index.emplace(record.objectId, entry).second) {
                return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
                    makeError(foundation::ErrorCode::kDuplicateObject,
                              "UniformGridSpatialIndex replacement identity is duplicated"));
            }
            update->records.emplace(entry, record);
            auto cellsResult = cellsFor(record.worldBounds, _cellSize);
            if (!cellsResult) {
                if (cellsResult.error().code != foundation::ErrorCode::kInvalidArgument ||
                    !canUseOverflow(record.worldBounds, _cellSize)) {
                    return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
                        cellsResult.error());
                }
                update->overflow.emplace(entry, record.worldBounds);
            } else {
                for (std::int64_t key : cellsResult.value()) {
                    update->cells[key].push_back(entry);
                }
            }
        }
        return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::success(
            std::move(update));
    } catch (const std::bad_alloc&) {
        return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
            makeError(foundation::ErrorCode::kOutOfMemory,
                      "UniformGridSpatialIndex could not prepare update"));
    }
}

foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>
UniformGridSpatialIndex::prepareReplace(std::span<const SpatialRecord> records,
                                        SceneRevision revision) const {
    ++_prepareCount;
    try {
        return prepareRecords(std::vector<SpatialRecord>(records.begin(), records.end()), revision);
    } catch (const std::bad_alloc&) {
        return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
            makeError(foundation::ErrorCode::kOutOfMemory,
                      "UniformGridSpatialIndex could not prepare replacement"));
    }
}

foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>
UniformGridSpatialIndex::prepareMutations(std::span<const SpatialMutation> mutations,
                                          SceneRevision beforeRevision,
                                          SceneRevision afterRevision) const {
    if (beforeRevision != _revision || afterRevision <= beforeRevision) {
        return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
            makeError(foundation::ErrorCode::kInvalidRevision,
                      "UniformGridSpatialIndex delta revision is invalid"));
    }
    try {
        auto update = std::make_unique<PreparedGridUpdate>();
        update->revision = afterRevision;
        update->nextEntryId = _nextEntryId;
        update->localized = true;
        update->plans.reserve(mutations.size());

        auto makeSet = [](const std::vector<std::int64_t>& values) {
            return std::unordered_set<std::int64_t>(values.begin(), values.end());
        };
        std::unordered_set<ObjectId, foundation::ObjectIdHash> seen;
        seen.reserve(mutations.size());

        for (const SpatialMutation& mutation : mutations) {
            if (!seen.insert(mutation.objectId).second) {
                return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
                    makeError(foundation::ErrorCode::kDuplicateObject,
                              "UniformGridSpatialIndex delta identity is duplicated"));
            }

            const auto indexIt = _index.find(mutation.objectId);
            const bool exists = indexIt != _index.end();
            const auto recordIt = exists ? _records.find(indexIt->second) : _records.end();
            if (exists && recordIt == _records.end()) {
                return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
                    makeError(foundation::ErrorCode::kInternalError,
                              "UniformGridSpatialIndex identity storage is inconsistent"));
            }

            switch (mutation.kind) {
            case SceneMutationKind::kInsert:
                if (exists || mutation.before || !mutation.after) {
                    return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
                        makeError(foundation::ErrorCode::kDuplicateObject,
                                  "UniformGridSpatialIndex insert is inconsistent"));
                }
                break;
            case SceneMutationKind::kUpdate:
                if (!exists || !mutation.before || !mutation.after ||
                    recordIt->second.worldBounds != *mutation.before) {
                    return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
                        makeError(foundation::ErrorCode::kBeforeImageMismatch,
                                  "UniformGridSpatialIndex update before image mismatches"));
                }
                break;
            case SceneMutationKind::kRemove:
                if (!exists || !mutation.before || mutation.after ||
                    recordIt->second.worldBounds != *mutation.before) {
                    return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
                        makeError(foundation::ErrorCode::kBeforeImageMismatch,
                                  "UniformGridSpatialIndex remove before image mismatches"));
                }
                break;
            default:
                return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
                    makeError(foundation::ErrorCode::kInvalidRecord,
                              "UniformGridSpatialIndex mutation kind is unknown"));
            }

            PreparedGridUpdate::MutationPlan plan;
            plan.mutation = mutation;
            if (exists) {
                plan.entry = indexIt->second;
            } else {
                plan.entry = update->nextEntryId++;
                update->records.emplace(plan.entry,
                                        SpatialRecord{mutation.objectId, *mutation.after});
                update->index.emplace(mutation.objectId, plan.entry);
            }

            std::vector<std::int64_t> oldCells;
            std::vector<std::int64_t> newCells;
            if (mutation.before) {
                auto oldCellsResult = cellsFor(*mutation.before, _cellSize);
                if (oldCellsResult) {
                    oldCells = std::move(oldCellsResult.value());
                } else if (oldCellsResult.error().code == foundation::ErrorCode::kInvalidArgument &&
                           canUseOverflow(*mutation.before, _cellSize)) {
                    plan.overflowBefore = true;
                } else {
                    return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
                        oldCellsResult.error());
                }
            }
            if (mutation.after) {
                auto newCellsResult = cellsFor(*mutation.after, _cellSize);
                if (newCellsResult) {
                    newCells = std::move(newCellsResult.value());
                } else if (newCellsResult.error().code == foundation::ErrorCode::kInvalidArgument &&
                           canUseOverflow(*mutation.after, _cellSize)) {
                    plan.overflowAfter = *mutation.after;
                    update->overflow.emplace(plan.entry, *mutation.after);
                } else {
                    return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
                        newCellsResult.error());
                }
            }

            auto oldSet = makeSet(oldCells);
            auto newSet = makeSet(newCells);
            for (const std::int64_t key : oldCells) {
                if (newSet.count(key) != 0U) {
                    plan.retained.push_back(key);
                } else {
                    plan.removed.push_back(key);
                }
            }
            for (const std::int64_t key : newCells) {
                if (oldSet.count(key) == 0U) {
                    plan.added.push_back(key);
                }
            }

            update->affectedEntryCount += 1U;
            update->oldCoverageUnitsVisited += oldCells.size();
            update->newCoverageUnitsVisited += newCells.size();
            update->membershipRemovalCount += plan.removed.size();
            update->membershipRetainedCount += plan.retained.size();
            update->membershipAddCount += plan.added.size();
            update->plans.push_back(std::move(plan));
        }

        auto preparedCell = [&](std::int64_t key) -> std::vector<SpatialEntryId>& {
            auto preparedIt = update->cells.find(key);
            if (preparedIt != update->cells.end()) {
                return preparedIt->second;
            }
            const auto liveIt = _cells.find(key);
            if (liveIt == _cells.end()) {
                return update->cells.emplace(key, std::vector<SpatialEntryId>{}).first->second;
            }
            return update->cells.emplace(key, liveIt->second).first->second;
        };

        for (const auto& plan : update->plans) {
            for (const std::int64_t key : plan.removed) {
                auto& values = preparedCell(key);
                values.erase(std::remove(values.begin(), values.end(), plan.entry), values.end());
            }
            for (const std::int64_t key : plan.added) {
                preparedCell(key).push_back(plan.entry);
            }
        }

        update->localizedMutationCount = update->plans.size();
        return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::success(
            std::move(update));
    } catch (const std::bad_alloc&) {
        return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
            makeError(foundation::ErrorCode::kOutOfMemory,
                      "UniformGridSpatialIndex could not prepare delta"));
    }
}

foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>
UniformGridSpatialIndex::prepareApply(std::span<const SpatialMutation> mutations,
                                      SceneRevision beforeRevision,
                                      SceneRevision afterRevision) const {
    ++_prepareCount;
    return prepareMutations(mutations, beforeRevision, afterRevision);
}

foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>
UniformGridSpatialIndex::prepareDelta(const SceneDelta& delta,
                                      SceneRevision beforeRevision,
                                      SceneRevision afterRevision) const {
    ++_prepareCount;
    if (delta.generationFrom != beforeRevision || delta.generationTo != afterRevision) {
        return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
            makeError(foundation::ErrorCode::kInvalidRevision,
                      "UniformGridSpatialIndex SceneDelta generation is invalid"));
    }
    try {
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
        return prepareMutations(mutations, beforeRevision, afterRevision);
    } catch (const std::bad_alloc&) {
        return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
            makeError(foundation::ErrorCode::kOutOfMemory,
                      "UniformGridSpatialIndex could not prepare SceneDelta"));
    }
}

foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>
UniformGridSpatialIndex::prepareSpatialDelta(const SpatialDelta& delta,
                                             SceneRevision beforeRevision,
                                             SceneRevision afterRevision) const {
    ++_prepareCount;
    if (delta.generationFrom != beforeRevision || delta.generationTo != afterRevision) {
        return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
            makeError(foundation::ErrorCode::kInvalidRevision,
                      "UniformGridSpatialIndex spatial delta generation is invalid"));
    }
    return prepareMutations(delta.mutations, beforeRevision, afterRevision);
}

void UniformGridSpatialIndex::commit(std::unique_ptr<IPreparedSpatialUpdate> prepared) noexcept {
    auto* update = static_cast<PreparedGridUpdate*>(prepared.get());
    if (update->localized) {
        while (!update->cells.empty()) {
            auto node = update->cells.extract(update->cells.begin());
            const auto liveIt = _cells.find(node.key());
            if (liveIt == _cells.end()) {
                if (!node.mapped().empty()) {
                    static_cast<void>(_cells.insert(std::move(node)));
                }
            } else {
                liveIt->second.swap(node.mapped());
                if (liveIt->second.empty()) {
                    _cells.erase(liveIt);
                }
            }
        }

        for (const auto& plan : update->plans) {
            if (plan.mutation.kind == SceneMutationKind::kUpdate && plan.mutation.after) {
                const auto recordIt = _records.find(plan.entry);
                if (recordIt != _records.end()) {
                    recordIt->second.worldBounds = *plan.mutation.after;
                }
            } else if (plan.mutation.kind == SceneMutationKind::kRemove) {
                _index.erase(plan.mutation.objectId);
                _records.erase(plan.entry);
            }
            if (plan.overflowBefore && !plan.overflowAfter) {
                _overflow.erase(plan.entry);
            }
        }

        while (!update->records.empty()) {
            static_cast<void>(_records.insert(update->records.extract(update->records.begin())));
        }
        while (!update->index.empty()) {
            static_cast<void>(_index.insert(update->index.extract(update->index.begin())));
        }
        while (!update->overflow.empty()) {
            auto node = update->overflow.extract(update->overflow.begin());
            const auto liveIt = _overflow.find(node.key());
            if (liveIt == _overflow.end()) {
                static_cast<void>(_overflow.insert(std::move(node)));
            } else {
                liveIt->second = node.mapped();
            }
        }

        _nextEntryId = update->nextEntryId;
        _revision = update->revision;
        _affectedEntryCount += update->affectedEntryCount;
        _oldCoverageUnitsVisited += update->oldCoverageUnitsVisited;
        _newCoverageUnitsVisited += update->newCoverageUnitsVisited;
        _membershipRemovalCount += update->membershipRemovalCount;
        _membershipRetainedCount += update->membershipRetainedCount;
        _membershipAddCount += update->membershipAddCount;
        _localizedMutationCount += update->localizedMutationCount;
        ++_commitCount;
        return;
    }

    _records.swap(update->records);
    _index.swap(update->index);
    _cells.swap(update->cells);
    _overflow.swap(update->overflow);
    _nextEntryId = update->nextEntryId;
    _revision = update->revision;
    ++_commitCount;
}

foundation::Result<SpatialQueryResult>
UniformGridSpatialIndex::query(const WorldRect& worldRect) const {
    auto cellsResult = cellsFor(worldRect, _cellSize);
    if (!cellsResult) {
        return foundation::Result<SpatialQueryResult>::failure(cellsResult.error());
    }
    try {
        SpatialQueryResult result;
        std::unordered_set<SpatialEntryId> visited;
        visited.reserve(_records.size());
        _lastCellVisits = cellsResult.value().size();
        for (const std::int64_t key : cellsResult.value()) {
            const auto found = _cells.find(key);
            if (found == _cells.end()) {
                continue;
            }
            for (const SpatialEntryId entry : found->second) {
                if (!visited.insert(entry).second) {
                    continue;
                }
                const auto recordIt = _records.find(entry);
                if (recordIt == _records.end()) {
                    continue;
                }
                ++result.examinedRecords;
                if (recordIt->second.worldBounds.intersects(worldRect)) {
                    result.candidates.push_back(recordIt->second.objectId);
                }
            }
        }
        for (const auto& [entry, bounds] : _overflow) {
            if (!bounds.intersects(worldRect) || !visited.insert(entry).second) {
                continue;
            }
            const auto recordIt = _records.find(entry);
            if (recordIt == _records.end()) {
                continue;
            }
            ++result.examinedRecords;
            result.candidates.push_back(recordIt->second.objectId);
        }
        _lastExaminedRecords = result.examinedRecords;
        _lastReturnedCandidates = result.candidates.size();
        return foundation::Result<SpatialQueryResult>::success(std::move(result));
    } catch (const std::bad_alloc&) {
        return foundation::Result<SpatialQueryResult>::failure(
            makeError(foundation::ErrorCode::kOutOfMemory, "Spatial query could not allocate"));
    }
}

SpatialIndexDiagnostics UniformGridSpatialIndex::diagnostics() const {
    std::size_t estimatedBytes =
        sizeof(*this) + _records.size() * (sizeof(SpatialEntryId) + sizeof(SpatialRecord) + 48U);
    estimatedBytes += _index.size() * (sizeof(ObjectId) + sizeof(SpatialEntryId) + 48U);
    for (const auto& [key, values] : _cells) {
        static_cast<void>(key);
        estimatedBytes += sizeof(std::int64_t) + 48U + values.capacity() * sizeof(SpatialEntryId);
    }
    estimatedBytes += _overflow.size() * (sizeof(SpatialEntryId) + sizeof(WorldRect) + 48U);
    return SpatialIndexDiagnostics{
        .revision = _revision,
        .recordCount = _records.size(),
        .prepareCount = _prepareCount,
        .commitCount = _commitCount,
        .lastExaminedRecords = _lastExaminedRecords,
        .lastReturnedCandidates = _lastReturnedCandidates,
        .lastCellVisits = _lastCellVisits,
        .estimatedBytes = estimatedBytes,
        .localizedMutationCount = _localizedMutationCount,
        .fullSpatialRebuildCount = _fullSpatialRebuildCount,
        .fullSpatialRecordCloneCount = _fullSpatialRecordCloneCount,
        .fullCellScanCount = _fullCellScanCount,
        .affectedEntryCount = _affectedEntryCount,
        .oldCoverageUnitsVisited = _oldCoverageUnitsVisited,
        .newCoverageUnitsVisited = _newCoverageUnitsVisited,
        .membershipRemovalCount = _membershipRemovalCount,
        .membershipRetainedCount = _membershipRetainedCount,
        .membershipAddCount = _membershipAddCount,
    };
}

} // namespace canvas
