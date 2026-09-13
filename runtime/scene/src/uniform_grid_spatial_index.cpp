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
        std::optional<WorldRect> overflowAfter;
    };
    SceneRevision revision;
    std::vector<SpatialRecord> records;
    std::vector<SpatialEntryId> entryIds;
    std::unordered_map<std::int64_t, std::vector<SpatialEntryId>> cells;
    std::unordered_map<SpatialEntryId, WorldRect> overflow;
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
        update->records = std::move(records);
        update->entryIds.resize(update->records.size());
        for (std::size_t index = 0; index < update->entryIds.size(); ++index) {
            update->entryIds[index] = static_cast<SpatialEntryId>(index + 1U);
        }
        for (std::size_t index = 0; index < update->records.size(); ++index) {
            auto cellsResult = cellsFor(update->records[index].worldBounds, _cellSize);
            if (!cellsResult) {
                const auto& bounds = update->records[index].worldBounds;
                if (!bounds.isFiniteAndOrdered() || bounds.right - bounds.left > 1048576.0F ||
                    bounds.bottom - bounds.top > 1048576.0F) {
                    return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
                        cellsResult.error());
                }
                update->overflow.emplace(static_cast<SpatialEntryId>(index + 1U),
                                         update->records[index].worldBounds);
            } else {
                for (std::int64_t key : cellsResult.value()) {
                    update->cells[key].push_back(static_cast<SpatialEntryId>(index + 1U));
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
UniformGridSpatialIndex::prepareApply(std::span<const SpatialMutation> mutations,
                                      SceneRevision beforeRevision,
                                      SceneRevision afterRevision) const {
    ++_prepareCount;
    if (beforeRevision != _revision || afterRevision <= beforeRevision) {
        return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
            makeError(foundation::ErrorCode::kInvalidRevision,
                      "UniformGridSpatialIndex delta revision is invalid"));
    }
    try {
        std::vector<SpatialRecord> nextRecords = _records;
        for (const SpatialMutation& mutation : mutations) {
            const auto found = std::find_if(
                nextRecords.begin(), nextRecords.end(), [&mutation](const SpatialRecord& record) {
                    return record.objectId == mutation.objectId;
                });
            switch (mutation.kind) {
            case SceneMutationKind::kInsert:
                if (found != nextRecords.end() || mutation.before || !mutation.after) {
                    return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
                        makeError(foundation::ErrorCode::kInvalidRecord,
                                  "UniformGridSpatialIndex insert is inconsistent"));
                }
                nextRecords.push_back(SpatialRecord{mutation.objectId, *mutation.after});
                break;
            case SceneMutationKind::kUpdate:
                if (found == nextRecords.end() || !mutation.before || !mutation.after ||
                    found->worldBounds != *mutation.before) {
                    return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
                        makeError(foundation::ErrorCode::kBeforeImageMismatch,
                                  "UniformGridSpatialIndex update is inconsistent"));
                }
                found->worldBounds = *mutation.after;
                break;
            case SceneMutationKind::kRemove:
                if (found == nextRecords.end() || !mutation.before || mutation.after ||
                    found->worldBounds != *mutation.before) {
                    return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
                        makeError(foundation::ErrorCode::kBeforeImageMismatch,
                                  "UniformGridSpatialIndex remove is inconsistent"));
                }
                nextRecords.erase(found);
                break;
            default:
                return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
                    makeError(foundation::ErrorCode::kInvalidRecord,
                              "UniformGridSpatialIndex mutation kind is unknown"));
            }
        }
        return prepareRecords(std::move(nextRecords), afterRevision);
    } catch (const std::bad_alloc&) {
        return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
            makeError(foundation::ErrorCode::kOutOfMemory,
                      "UniformGridSpatialIndex could not prepare delta"));
    }
}

foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>
UniformGridSpatialIndex::prepareDelta(const SceneDelta& delta,
                                      SceneRevision beforeRevision,
                                      SceneRevision afterRevision) const {
    ++_prepareCount;
    if (beforeRevision != _revision || afterRevision <= beforeRevision) {
        return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
            makeError(foundation::ErrorCode::kInvalidRevision,
                      "UniformGridSpatialIndex delta revision is invalid"));
    }
    if (delta.generationFrom != beforeRevision || delta.generationTo != afterRevision) {
        return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
            makeError(foundation::ErrorCode::kInvalidRevision,
                      "UniformGridSpatialIndex SceneDelta generation is invalid"));
    }
    try {
        auto update = std::make_unique<PreparedGridUpdate>();
        update->revision = afterRevision;
        update->localized = true;
        update->plans.reserve(delta.mutations.size());
        auto makeSet = [](const std::vector<std::int64_t>& values) {
            return std::unordered_set<std::int64_t>(values.begin(), values.end());
        };
        std::unordered_set<ObjectId, foundation::ObjectIdHash> seen;
        seen.reserve(delta.mutations.size());
        for (const SceneMutation& mutation : delta.mutations) {
            if (!seen.insert(mutation.objectId).second) {
                return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
                    makeError(foundation::ErrorCode::kDuplicateObject,
                              "UniformGridSpatialIndex delta identity is duplicated"));
            }
            auto found = _index.find(mutation.objectId);
            if (mutation.kind != SceneMutationKind::kInsert && found == _index.end()) {
                return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
                    makeError(foundation::ErrorCode::kMissingObject,
                              "UniformGridSpatialIndex delta identity is missing"));
            }
            if (mutation.kind == SceneMutationKind::kInsert &&
                (found != _index.end() || mutation.before || !mutation.after)) {
                return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
                    makeError(foundation::ErrorCode::kDuplicateObject,
                              "UniformGridSpatialIndex delta identity is duplicated"));
            }
            if (mutation.kind == SceneMutationKind::kUpdate &&
                (!mutation.before || !mutation.after ||
                 _records[found->second].worldBounds != mutation.before->worldBounds)) {
                return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
                    makeError(foundation::ErrorCode::kBeforeImageMismatch,
                              "UniformGridSpatialIndex update before image mismatches"));
            }
            if (mutation.kind == SceneMutationKind::kRemove &&
                (!mutation.before || mutation.after ||
                 _records[found->second].worldBounds != mutation.before->worldBounds)) {
                return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
                    makeError(foundation::ErrorCode::kBeforeImageMismatch,
                              "UniformGridSpatialIndex remove before image mismatches"));
            }
            PreparedGridUpdate::MutationPlan plan;
            plan.mutation = SpatialMutation{mutation.kind, mutation.objectId,
                                             mutation.before ? std::optional(mutation.before->worldBounds) : std::nullopt,
                                             mutation.after ? std::optional(mutation.after->worldBounds) : std::nullopt};
            plan.entry = found == _index.end() ? _nextEntryId + update->plans.size()
                                               : _entryIds[found->second];
            std::vector<std::int64_t> oldCells;
            std::vector<std::int64_t> newCells;
            bool oldOverflow = false;
            if (mutation.before) {
                auto oldCellsResult = cellsFor(mutation.before->worldBounds, _cellSize);
                if (oldCellsResult) oldCells = std::move(oldCellsResult.value());
                else if (mutation.before->worldBounds.isFiniteAndOrdered()) oldOverflow = true;
                else return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(oldCellsResult.error());
            }
            if (mutation.after) {
                auto newCellsResult = cellsFor(mutation.after->worldBounds, _cellSize);
                if (newCellsResult) newCells = std::move(newCellsResult.value());
                else if (mutation.after->worldBounds.isFiniteAndOrdered() &&
                         mutation.after->worldBounds.right - mutation.after->worldBounds.left <= 1048576.0F &&
                         mutation.after->worldBounds.bottom - mutation.after->worldBounds.top <= 1048576.0F) {
                    newCells.clear();
                    plan.overflowAfter = mutation.after->worldBounds;
                } else return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(newCellsResult.error());
            }
            auto oldSet = makeSet(oldCells);
            auto newSet = makeSet(newCells);
            for (auto key : oldCells) {
                if (newSet.count(key)) plan.retained.push_back(key); else plan.removed.push_back(key);
            }
            for (auto key : newCells) if (!oldSet.count(key)) plan.added.push_back(key);
            if (oldOverflow) plan.removed.clear();
            update->affectedEntryCount += 1;
            update->oldCoverageUnitsVisited += oldCells.size();
            update->newCoverageUnitsVisited += newCells.size();
            update->membershipRemovalCount += plan.removed.size();
            update->membershipRetainedCount += plan.retained.size();
            update->membershipAddCount += plan.added.size();
            update->plans.push_back(std::move(plan));
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
UniformGridSpatialIndex::prepareSpatialDelta(const SpatialDelta& delta,
                                             SceneRevision beforeRevision,
                                             SceneRevision afterRevision) const {
    if (delta.generationFrom != beforeRevision || delta.generationTo != afterRevision) {
        return foundation::Result<std::unique_ptr<IPreparedSpatialUpdate>>::failure(
            makeError(foundation::ErrorCode::kInvalidRevision,
                      "UniformGridSpatialIndex spatial delta generation is invalid"));
    }
    SceneDelta sceneDelta{
        .generationFrom = delta.generationFrom,
        .generationTo = delta.generationTo,
        .mutations = {},
    };
    sceneDelta.mutations.reserve(delta.mutations.size());
    for (const auto& mutation : delta.mutations) {
        sceneDelta.mutations.push_back(SceneMutation{
            .kind = mutation.kind,
            .objectId = mutation.objectId,
            .before = mutation.before ? std::optional(SceneRecord{mutation.objectId, {}, SceneObjectKind::kShape,
                                                                    SceneRecordFlags::kNone, *mutation.before, {}, {}, {}}) : std::nullopt,
            .after = mutation.after ? std::optional(SceneRecord{mutation.objectId, {}, SceneObjectKind::kShape,
                                                                  SceneRecordFlags::kNone, *mutation.after, {}, {}, {}}) : std::nullopt,
        });
    }
    return prepareDelta(sceneDelta, beforeRevision, afterRevision);
}

void UniformGridSpatialIndex::commit(std::unique_ptr<IPreparedSpatialUpdate> prepared) noexcept {
    auto* update = static_cast<PreparedGridUpdate*>(prepared.get());
    if (update->localized) {
        for (const auto& plan : update->plans) {
            const SpatialMutation& mutation = plan.mutation;
            const auto indexIt = _index.find(mutation.objectId);
            SpatialRecord* found = indexIt == _index.end() ? nullptr : &_records[indexIt->second];
            if (mutation.kind == SceneMutationKind::kInsert && mutation.after) {
                const std::uint32_t index = static_cast<std::uint32_t>(_records.size());
                _records.push_back(SpatialRecord{mutation.objectId, *mutation.after});
                _entryIds.push_back(plan.entry);
                _entrySlots[plan.entry] = index;
                _nextEntryId = std::max(_nextEntryId, plan.entry + 1U);
                _index.emplace(mutation.objectId, index);
                if (plan.overflowAfter) _overflow[plan.entry] = *plan.overflowAfter;
                else for (auto key : plan.added) _cells[key].push_back(plan.entry);
            } else if (mutation.kind == SceneMutationKind::kUpdate && found != nullptr && mutation.after) {
                for (auto key : plan.removed) {
                    auto& values = _cells.find(key)->second;
                    values.erase(std::remove(values.begin(), values.end(), plan.entry), values.end());
                }
                found->worldBounds = *mutation.after;
                _overflow.erase(plan.entry);
                if (plan.overflowAfter) _overflow[plan.entry] = *plan.overflowAfter;
                else for (auto key : plan.added) _cells[key].push_back(plan.entry);
            } else if (mutation.kind == SceneMutationKind::kRemove && found != nullptr) {
                for (auto key : plan.removed) {
                    auto& values = _cells.find(key)->second;
                    values.erase(std::remove(values.begin(), values.end(), plan.entry), values.end());
                }
                found->objectId = ObjectId{};
                found->worldBounds = WorldRect{};
                _index.erase(mutation.objectId);
                _overflow.erase(plan.entry);
            }
        }
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
    _entryIds.swap(update->entryIds);
    _entrySlots.assign(_entryIds.size() + 1U, 0U);
    for (std::size_t i = 0; i < _entryIds.size(); ++i) {
        _entrySlots[_entryIds[i]] = static_cast<std::uint32_t>(i);
    }
    _nextEntryId = static_cast<SpatialEntryId>(_entryIds.size() + 1U);
    _cells.swap(update->cells);
    _overflow.swap(update->overflow);
    _index.clear();
    for (std::size_t i = 0; i < _records.size(); ++i) {
        if (!_records[i].objectId.isZero()) {
            _index.emplace(_records[i].objectId, static_cast<std::uint32_t>(i));
        }
    }
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
        std::unordered_set<std::uint32_t> visited;
        visited.reserve(_records.size());
        _lastCellVisits = cellsResult.value().size();
        for (std::int64_t key : cellsResult.value()) {
            const auto found = _cells.find(key);
            if (found == _cells.end()) {
                continue;
            }
            for (SpatialEntryId entry : found->second) {
                if (entry >= _entrySlots.size()) continue;
                const std::uint32_t index = _entrySlots[entry];
                if (!visited.insert(index).second) {
                    continue;
                }
                if (_records[index].objectId.isZero()) continue;
                ++result.examinedRecords;
                if (_records[index].worldBounds.intersects(worldRect)) {
                    result.candidates.push_back(_records[index].objectId);
                }
            }
        }
        for (const auto& [entry, bounds] : _overflow) {
            if (!bounds.intersects(worldRect) || entry >= _entrySlots.size()) continue;
            const std::uint32_t index = _entrySlots[entry];
            if (!visited.insert(index).second || _records[index].objectId.isZero()) continue;
            ++result.examinedRecords;
            result.candidates.push_back(_records[index].objectId);
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
    std::size_t estimatedBytes = sizeof(*this) + _records.capacity() * sizeof(SpatialRecord);
    for (const auto& [key, values] : _cells) {
        static_cast<void>(key);
        estimatedBytes += sizeof(std::int64_t) + 32U + values.capacity() * sizeof(std::uint32_t);
    }
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
