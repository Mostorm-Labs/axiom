#pragma once

#include "canvas/foundation/result.hpp"
#include "canvas/scene/scene_types.hpp"
#include "canvas/scene/scene_delta.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace canvas {

// Legacy RF-01 render-participant seam. RuntimeScene's G2 production
// projection is declared in scene.hpp/scene_types.hpp and does not depend on
// this interface. DirectRenderScene and ShadowRenderScene remain transition
// and reference-test assets until the G3 render migration.

class IPreparedRenderSceneUpdate {
  public:
    virtual ~IPreparedRenderSceneUpdate() = default;
};

struct PreciseHitRequest final {
    ObjectId objectId;
    WorldPoint worldPoint;
    float tolerance = 0.0F;
};

struct PreciseHit final {
    bool hit = false;
    float distance = 0.0F;
};

struct SceneDrawItem final {
    ObjectId objectId;
    SceneOrderKey orderKey;
    RenderPayloadRef renderPayload;
};

struct SceneDrawList final {
    SceneRevision revision;
    std::vector<SceneDrawItem> items;
};

struct RenderSceneDiagnostics final {
    SceneRevision revision;
    std::uint64_t nodeCount = 0;
    std::uint64_t prepareCount = 0;
    std::uint64_t commitCount = 0;
    std::uint64_t fullRecordCloneCount = 0;
    std::uint64_t fullSortCount = 0;
    std::uint64_t fullReindexCount = 0;
    std::uint64_t fullRebuildCount = 0;
    std::uint64_t localizedMutationCount = 0;
};

class IRenderScene {
  public:
    virtual ~IRenderScene() = default;

    virtual foundation::Result<std::unique_ptr<IPreparedRenderSceneUpdate>>
    prepareReplace(std::span<const SceneRecord> records, SceneRevision revision) const = 0;

    virtual foundation::Result<std::unique_ptr<IPreparedRenderSceneUpdate>>
    prepareApply(std::span<const SceneMutation> mutations,
                 SceneRevision beforeRevision,
                 SceneRevision afterRevision) const = 0;

    virtual foundation::Result<std::unique_ptr<IPreparedRenderSceneUpdate>>
    prepareDelta(const SceneDelta& delta,
                 SceneRevision beforeRevision,
                 SceneRevision afterRevision) const {
        return prepareApply(delta.mutations, beforeRevision, afterRevision);
    }

    virtual void commit(std::unique_ptr<IPreparedRenderSceneUpdate> update) noexcept = 0;

    virtual foundation::Result<PreciseHit>
    preciseHitTest(const PreciseHitRequest& request) const = 0;

    virtual foundation::Result<SceneDrawList>
    buildDrawList(std::span<const ObjectId> backToFront) const = 0;

    [[nodiscard]] virtual RenderSceneDiagnostics diagnostics() const = 0;
};

} // namespace canvas
