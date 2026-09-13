#include "canvas/scene/scene_record_store.hpp"

#include <algorithm>
#include <new>
#include <unordered_set>
#include <utility>

namespace canvas {
namespace {

using foundation::Error;
using foundation::ErrorCode;

Error makeError(ErrorCode code, const char* message) {
    return Error{code, message};
}

bool recordLess(const SceneRecord& left, const SceneRecord& right) {
    if (left.orderKey != right.orderKey) {
        return left.orderKey < right.orderKey;
    }
    return left.objectId < right.objectId;
}

bool isKnownKind(SceneObjectKind kind) {
    switch (kind) {
    case SceneObjectKind::kShape:
    case SceneObjectKind::kImage:
    case SceneObjectKind::kVectorPath:
    case SceneObjectKind::kRichText:
    case SceneObjectKind::kVectorStroke:
    case SceneObjectKind::kDabStroke:
        return true;
    }
    return false;
}

foundation::Result<SceneRecord> normalizeRecord(SceneRecord record) {
    if (record.objectId.isZero() || !isKnownKind(record.kind) ||
        !record.worldBounds.isFiniteAndOrdered()) {
        return foundation::Result<SceneRecord>::failure(
            makeError(ErrorCode::kInvalidRecord, "SceneRecord is invalid"));
    }
    if (record.renderPayload.generation == 0 || record.hitGeometry.generation == 0) {
        return foundation::Result<SceneRecord>::failure(
            makeError(ErrorCode::kInvalidReference, "SceneRecord reference is invalid"));
    }
    record.worldBounds = foundation::canonicalizeRect(record.worldBounds);
    return foundation::Result<SceneRecord>::success(std::move(record));
}

const SceneRecord*
findRecord(std::span<const SceneRecord> records,
           const std::unordered_map<ObjectId, std::size_t, foundation::ObjectIdHash>& index,
           ObjectId objectId) {
    const auto found = index.find(objectId);
    return found == index.end() ? nullptr : &records[found->second];
}

WorldRect calculateContentBounds(std::span<const SceneRecord> records) {
    if (records.empty()) {
        return WorldRect{};
    }
    WorldRect bounds = records.front().worldBounds;
    for (const SceneRecord& record : records.subspan(1)) {
        bounds = foundation::unionRects(bounds, record.worldBounds);
    }
    return bounds;
}

} // namespace

foundation::Result<SceneRecordStore::PreparedUpdate>
SceneRecordStore::buildPreparedUpdate(std::vector<SceneRecord> records) {
    try {
        std::sort(records.begin(), records.end(), recordLess);
        std::unordered_map<ObjectId, std::size_t, foundation::ObjectIdHash> index;
        index.reserve(records.size());
        for (std::size_t position = 0; position < records.size(); ++position) {
            if (!index.emplace(records[position].objectId, position).second) {
                return foundation::Result<PreparedUpdate>::failure(
                    makeError(ErrorCode::kDuplicateObject, "Scene contains a duplicate ObjectId"));
            }
        }
        return foundation::Result<PreparedUpdate>::success(
            PreparedUpdate(std::move(records), std::move(index)));
    } catch (const std::bad_alloc&) {
        return foundation::Result<PreparedUpdate>::failure(
            makeError(ErrorCode::kOutOfMemory, "Unable to prepare SceneRecordStore"));
    }
}

const SceneRecord* SceneRecordStore::PreparedUpdate::find(ObjectId objectId) const {
    return findRecord(_records, _index, objectId);
}

WorldRect SceneRecordStore::PreparedUpdate::contentBounds() const {
    return calculateContentBounds(_records);
}

const SceneRecord* SceneRecordStore::find(ObjectId objectId) const {
    materializeOrderedCache();
    return findRecord(_records, _index, objectId);
}

RecordHandle SceneRecordStore::handleFor(ObjectId objectId) const noexcept {
    const auto found = _handles.find(objectId);
    return found == _handles.end() ? 0 : found->second;
}

foundation::Result<std::vector<SceneRecord>> SceneRecordStore::materializeSnapshot() const {
    try {
        materializeOrderedCache();
        return foundation::Result<std::vector<SceneRecord>>::success(_records);
    } catch (const std::bad_alloc&) {
        return foundation::Result<std::vector<SceneRecord>>::failure(
            makeError(ErrorCode::kOutOfMemory, "Unable to materialize Scene snapshot"));
    }
}

WorldRect SceneRecordStore::contentBounds() const {
    materializeOrderedCache();
    return calculateContentBounds(_records);
}

void SceneRecordStore::materializeOrderedCache() const {
    if (_orderedCacheValid) {
        return;
    }
    _records.clear();
    _index.clear();
    _records.reserve(_orderIndex.size());
    for (const auto& [key, handle] : _orderIndex) {
        static_cast<void>(key);
        const auto found = _arena.find(handle);
        if (found == _arena.end()) {
            continue;
        }
        _index.emplace(found->second.objectId, _records.size());
        _records.push_back(found->second);
    }
    _orderedCacheValid = true;
}

std::size_t SceneRecordStore::estimatedBytes() const {
    return sizeof(*this) + _records.capacity() * sizeof(SceneRecord) +
           _index.size() * (sizeof(ObjectId) + sizeof(std::size_t) + 24U);
}

foundation::Result<SceneRecordStore::PreparedUpdate>
SceneRecordStore::prepareReplace(std::span<const SceneRecord> records) const {
    try {
        ++_localityDiagnostics.fullRecordCloneCount;
        ++_localityDiagnostics.fullSortCount;
        ++_localityDiagnostics.fullReindexCount;
        ++_localityDiagnostics.fullRebuildCount;
        std::vector<SceneRecord> normalized;
        normalized.reserve(records.size());
        for (const SceneRecord& record : records) {
            auto result = normalizeRecord(record);
            if (!result) {
                return foundation::Result<PreparedUpdate>::failure(result.error());
            }
            normalized.push_back(std::move(result.value()));
        }
        return buildPreparedUpdate(std::move(normalized));
    } catch (const std::bad_alloc&) {
        return foundation::Result<PreparedUpdate>::failure(
            makeError(ErrorCode::kOutOfMemory, "Unable to prepare Scene replacement"));
    }
}

foundation::Result<SceneRecordStore::PreparedUpdate>
SceneRecordStore::prepareApply(std::span<const SceneMutation> mutations) const {
    try {
        std::unordered_set<ObjectId, foundation::ObjectIdHash> mutatedIds;
        mutatedIds.reserve(mutations.size());
        PreparedDelta preparedDelta;
        preparedDelta.sceneDelta.generationFrom = SceneRevision{};
        preparedDelta.sceneDelta.generationTo = SceneRevision{};
        preparedDelta.sceneDelta.mutations.reserve(mutations.size());
        preparedDelta.slotWrites.reserve(mutations.size());
        preparedDelta.objectIndexUpdates.reserve(mutations.size());
        preparedDelta.orderIndexUpdates.reserve(mutations.size() * 2);
        for (const SceneMutation& mutation : mutations) {
            if (mutation.objectId.isZero() || !mutatedIds.insert(mutation.objectId).second) {
                return foundation::Result<PreparedUpdate>::failure(makeError(
                    ErrorCode::kDuplicateObject, "Delta mutates an ObjectId more than once"));
            }

            const auto handleIt = _handles.find(mutation.objectId);
            const bool exists = handleIt != _handles.end();
            const bool hasBefore = mutation.before.has_value();
            const bool hasAfter = mutation.after.has_value();
            if ((hasBefore && mutation.before->objectId != mutation.objectId) ||
                (hasAfter && mutation.after->objectId != mutation.objectId)) {
                return foundation::Result<PreparedUpdate>::failure(makeError(
                    ErrorCode::kInvalidRecord, "Mutation ObjectId does not match its record"));
            }

            std::optional<SceneRecord> normalizedAfter;
            if (hasAfter) {
                auto result = normalizeRecord(*mutation.after);
                if (!result) {
                    return foundation::Result<PreparedUpdate>::failure(result.error());
                }
                normalizedAfter = std::move(result.value());
            }

            switch (mutation.kind) {
            case SceneMutationKind::kInsert: {
                if (hasBefore || !normalizedAfter) {
                    return foundation::Result<PreparedUpdate>::failure(makeError(
                        ErrorCode::kInvalidRecord, "Insert requires only an after record"));
                }
                if (exists) {
                    return foundation::Result<PreparedUpdate>::failure(
                        makeError(ErrorCode::kDuplicateObject, "Insert ObjectId already exists"));
                }
                const RecordHandle handle = _nextHandle + preparedDelta.slotWrites.size();
                preparedDelta.slotWrites.push_back(SlotWrite{handle, std::nullopt, *normalizedAfter});
                preparedDelta.objectIndexUpdates.push_back(ObjectIndexUpdate{mutation.objectId, handle, false});
                preparedDelta.orderIndexUpdates.push_back(OrderIndexUpdate{mutation.objectId, normalizedAfter->orderKey, handle, false});
                preparedDelta.sceneDelta.mutations.push_back(mutation);
                break;
            }
            case SceneMutationKind::kUpdate: {
                if (!hasBefore || !normalizedAfter) {
                    return foundation::Result<PreparedUpdate>::failure(makeError(
                        ErrorCode::kInvalidRecord, "Update requires before and after records"));
                }
                if (!exists) {
                    return foundation::Result<PreparedUpdate>::failure(
                        makeError(ErrorCode::kMissingObject, "Update ObjectId does not exist"));
                }
                if (_arena.at(handleIt->second) != *mutation.before) {
                    return foundation::Result<PreparedUpdate>::failure(
                        makeError(ErrorCode::kBeforeImageMismatch,
                                  "Update before image does not match Scene"));
                }
                const RecordHandle handle = handleIt->second;
                preparedDelta.slotWrites.push_back(SlotWrite{handle, mutation.before, *normalizedAfter});
                preparedDelta.objectIndexUpdates.push_back(ObjectIndexUpdate{mutation.objectId, handle, false});
                preparedDelta.orderIndexUpdates.push_back(OrderIndexUpdate{mutation.objectId, mutation.before->orderKey, handle, true});
                preparedDelta.orderIndexUpdates.push_back(OrderIndexUpdate{mutation.objectId, normalizedAfter->orderKey, handle, false});
                preparedDelta.sceneDelta.mutations.push_back(mutation);
                break;
            }
            case SceneMutationKind::kRemove: {
                if (!hasBefore || hasAfter) {
                    return foundation::Result<PreparedUpdate>::failure(makeError(
                        ErrorCode::kInvalidRecord, "Remove requires only a before record"));
                }
                if (!exists) {
                    return foundation::Result<PreparedUpdate>::failure(
                        makeError(ErrorCode::kMissingObject, "Remove ObjectId does not exist"));
                }
                if (_arena.at(handleIt->second) != *mutation.before) {
                    return foundation::Result<PreparedUpdate>::failure(
                        makeError(ErrorCode::kBeforeImageMismatch,
                                  "Remove before image does not match Scene"));
                }
                const RecordHandle handle = handleIt->second;
                preparedDelta.slotWrites.push_back(SlotWrite{handle, mutation.before, std::nullopt});
                preparedDelta.objectIndexUpdates.push_back(ObjectIndexUpdate{mutation.objectId, handle, true});
                preparedDelta.orderIndexUpdates.push_back(OrderIndexUpdate{mutation.objectId, mutation.before->orderKey, handle, true});
                preparedDelta.sceneDelta.mutations.push_back(mutation);
                break;
            }
            default:
                return foundation::Result<PreparedUpdate>::failure(
                    makeError(ErrorCode::kInvalidRecord, "Mutation kind is unknown"));
            }
        }
        return foundation::Result<PreparedUpdate>::success(
            PreparedUpdate(std::move(preparedDelta)));
    } catch (const std::bad_alloc&) {
        return foundation::Result<PreparedUpdate>::failure(
            makeError(ErrorCode::kOutOfMemory, "Unable to prepare Scene delta"));
    }
}

void SceneRecordStore::commit(PreparedUpdate update) noexcept {
    if (!update._delta.sceneDelta.mutations.empty()) {
        for (const SceneMutation& mutation : update._delta.sceneDelta.mutations) {
            switch (mutation.kind) {
            case SceneMutationKind::kInsert: {
                if (_handles.find(mutation.objectId) == _handles.end()) {
                    const RecordHandle handle = _nextHandle++;
                    _handles[mutation.objectId] = handle;
                    _arena.emplace(handle, *mutation.after);
                    _orderIndex.emplace(std::make_pair(mutation.after->orderKey,
                                                        mutation.objectId), handle);
                }
                break;
            }
            case SceneMutationKind::kUpdate: {
                const auto handleIt = _handles.find(mutation.objectId);
                if (handleIt == _handles.end()) break;
                const SceneRecord replacement = *mutation.after;
                const RecordHandle handle = handleIt->second;
                const SceneRecord previous = _arena[handle];
                _orderIndex.erase(std::make_pair(previous.orderKey, mutation.objectId));
                _arena[handle] = replacement;
                _orderIndex.emplace(std::make_pair(replacement.orderKey, mutation.objectId), handle);
                break;
            }
            case SceneMutationKind::kRemove: {
                const auto handleIt = _handles.find(mutation.objectId);
                if (handleIt == _handles.end()) break;
                const RecordHandle handle = handleIt->second;
                const SceneRecord previous = _arena[handle];
                _orderIndex.erase(std::make_pair(previous.orderKey, mutation.objectId));
                _arena.erase(handle);
                _handles.erase(mutation.objectId);
                break;
            }
            }
        }
        _localityDiagnostics.localizedMutationCount += update._delta.sceneDelta.mutations.size();
        _orderedCacheValid = false;
        return;
    }
    _records.swap(update._records);
    _index.swap(update._index);
    _handles.clear();
    _arena.clear();
    _orderIndex.clear();
    for (const SceneRecord& record : _records) {
        const RecordHandle handle = _nextHandle++;
        _handles.emplace(record.objectId, handle);
        _arena.emplace(handle, record);
        _orderIndex.emplace(std::make_pair(record.orderKey, record.objectId), handle);
    }
    _orderedCacheValid = true;
}

} // namespace canvas
