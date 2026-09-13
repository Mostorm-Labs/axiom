#pragma once

#include "canvas/foundation/result.hpp"
#include "canvas/scene/scene_types.hpp"
#include "canvas/scene/scene_delta.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace canvas {

class SceneRecordStore final {
  public:
    struct LocalityDiagnostics final {
        std::uint64_t fullRecordCloneCount = 0;
        std::uint64_t fullSortCount = 0;
        std::uint64_t fullReindexCount = 0;
        std::uint64_t fullRebuildCount = 0;
        std::uint64_t localizedMutationCount = 0;
    };

    class PreparedUpdate final {
      public:
        PreparedUpdate(PreparedUpdate&&) noexcept = default;
        PreparedUpdate& operator=(PreparedUpdate&&) noexcept = default;

        PreparedUpdate(const PreparedUpdate&) = delete;
        PreparedUpdate& operator=(const PreparedUpdate&) = delete;

        [[nodiscard]] std::span<const SceneRecord> records() const {
            return _records;
        }
        [[nodiscard]] const SceneRecord* find(ObjectId objectId) const;
        [[nodiscard]] WorldRect contentBounds() const;

      private:
        friend class SceneRecordStore;

        PreparedUpdate(std::vector<SceneRecord> records,
                       std::unordered_map<ObjectId, std::size_t, foundation::ObjectIdHash> index)
            : _records(std::move(records)), _index(std::move(index)) {}

        explicit PreparedUpdate(std::vector<SceneMutation> mutations)
            : _mutations(std::move(mutations)) {}

        std::vector<SceneRecord> _records;
        std::unordered_map<ObjectId, std::size_t, foundation::ObjectIdHash> _index;
        std::vector<SceneMutation> _mutations;
    };

    [[nodiscard]] std::span<const SceneRecord> records() const {
        materializeOrderedCache();
        return _records;
    }
    [[nodiscard]] const SceneRecord* find(ObjectId objectId) const;
    [[nodiscard]] RecordHandle handleFor(ObjectId objectId) const noexcept;
    [[nodiscard]] foundation::Result<std::vector<SceneRecord>> materializeSnapshot() const;
    [[nodiscard]] WorldRect contentBounds() const;
    [[nodiscard]] std::size_t estimatedBytes() const;

    foundation::Result<PreparedUpdate> prepareReplace(std::span<const SceneRecord> records) const;
    foundation::Result<PreparedUpdate> prepareApply(std::span<const SceneMutation> mutations) const;
    void commit(PreparedUpdate update) noexcept;
    [[nodiscard]] LocalityDiagnostics localityDiagnostics() const noexcept {
        return _localityDiagnostics;
    }

  private:
    static foundation::Result<PreparedUpdate> buildPreparedUpdate(std::vector<SceneRecord> records);
    void materializeOrderedCache() const;

    mutable std::vector<SceneRecord> _records;
    mutable std::unordered_map<ObjectId, std::size_t, foundation::ObjectIdHash> _index;
    std::unordered_map<ObjectId, RecordHandle, foundation::ObjectIdHash> _handles;
    std::unordered_map<RecordHandle, SceneRecord> _arena;
    std::map<std::pair<SceneOrderKey, ObjectId>, RecordHandle> _orderIndex;
    mutable bool _orderedCacheValid = false;
    RecordHandle _nextHandle = 1;
    mutable LocalityDiagnostics _localityDiagnostics;
};

} // namespace canvas
