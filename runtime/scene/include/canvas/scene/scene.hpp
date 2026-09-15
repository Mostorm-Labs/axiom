#pragma once

#include "canvas/foundation/result.hpp"
#include "canvas/scene/damage_tracker.hpp"
#include "canvas/scene/render_scene.hpp"
#include "canvas/scene/scene_frame.hpp"
#include "canvas/scene/scene_query.hpp"
#include "canvas/scene/scene_record_store.hpp"
#include "canvas/scene/scene_types.hpp"
#include "canvas/scene/scene_commit_input.hpp"
#include "canvas/scene/spatial_index.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace canvas {

struct SceneApplyReceipt final {
    SceneRevision beforeRevision;
    SceneRevision afterRevision;
    std::uint64_t recordsTouched = 0;
    std::uint64_t renderNodesTouched = 0;
    std::uint64_t spatialRecordsTouched = 0;
    DamageSet damage;
};

struct SceneCommitDiagnostics final {
    std::uint64_t transactionCount = 0;
    std::uint8_t recordStoreStage = 0;
    std::uint8_t renderSceneStage = 0;
    std::uint8_t spatialIndexStage = 0;
    std::uint8_t damageStage = 0;
    std::uint8_t revisionStage = 0;
};

class SceneReadView final {
  public:
    [[nodiscard]] SceneRevision revision() const {
        return _revision;
    }
    [[nodiscard]] std::span<const SceneRecord> records() const {
        return _records;
    }

    [[nodiscard]] const SceneRecord* find(ObjectId objectId) const;

  private:
    friend class Scene;

    SceneReadView(SceneRevision revision, std::span<const SceneRecord> records)
        : _revision(revision), _records(records) {}

    SceneRevision _revision;
    std::span<const SceneRecord> _records;
};

class Scene final {
  public:
    Scene(std::unique_ptr<IRenderScene> renderScene, std::unique_ptr<ISpatialIndex> spatialIndex);

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    [[nodiscard]] SceneRevision revision() const {
        return _publicationGate != nullptr && _publicationGate->transactionActive
                   ? _publicationGate->previousRevision : _revision;
    }
    [[nodiscard]] semantic::SemanticGeneration semanticGeneration() const {
        return _publicationGate != nullptr && _publicationGate->transactionActive
                   ? _publicationGate->previousGeneration : _semanticGeneration;
    }
    [[nodiscard]] SceneReadView read() const {
        return SceneReadView(_revision, _records.records());
    }

    foundation::Result<SceneApplyReceipt> replace(CompiledSceneSnapshot snapshot);
    foundation::Result<SceneApplyReceipt> apply(CompiledSceneDelta delta);

    foundation::Result<SceneQueryResult> query(const SceneQuery& query) const;
    foundation::Result<HitTestResult> hitTest(const HitTestRequest& request) const;
    foundation::Result<SceneDrawList> buildDrawList(const SceneQueryResult& visible) const;
    foundation::Result<SceneFrameInput> buildFrame(const SceneQuery& query,
                                                    SceneRevision afterExclusive) const;

    [[nodiscard]] DamageSet collectDamage(SceneRevision afterExclusive,
                                          SceneRevision throughInclusive) const;
    void compactDamageThrough(SceneRevision revision);

    foundation::Result<SceneApplyReceipt> replace(const SceneCommitInput& input,
                                                  CompiledSceneSnapshot snapshot);
    foundation::Result<SceneApplyReceipt> apply(const SceneCommitInput& input,
                                                CompiledSceneDelta delta);

    [[nodiscard]] SceneCommitDiagnostics commitDiagnostics() const {
        return _commitDiagnostics;
    }
    [[nodiscard]] DamageDiagnostics damageDiagnostics() const {
        return _damageTracker.diagnostics();
    }

  private:
    using TransactionCheckpointFn = bool (*)(void*, std::uint8_t) noexcept;
    using PublicationFn = void (*)(void*) noexcept;
    friend class SceneBinding;
    friend class IncrementalRuntimeCoordinator;

    void setPublicationGate(ScenePublicationGate* gate) noexcept { _publicationGate = gate; }

    foundation::Result<SceneApplyReceipt> applyPreparedDelta(
        CompiledSceneDelta delta,
        TransactionCheckpointFn checkpoint,
        void* checkpointContext,
        PublicationFn publish,
        void* publishContext);

    SceneRecordStore _records;
    std::unique_ptr<IRenderScene> _renderScene;
    std::unique_ptr<ISpatialIndex> _spatialIndex;
    DamageTracker _damageTracker;
    SceneRevision _revision;
    semantic::SemanticGeneration _semanticGeneration{};
    SceneCommitDiagnostics _commitDiagnostics;
    ScenePublicationGate* _publicationGate = nullptr;
};

} // namespace canvas
