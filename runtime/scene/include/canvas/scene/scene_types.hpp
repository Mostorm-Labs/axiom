#pragma once

#include "canvas/foundation/result.hpp"
#include "canvas/foundation/object_id.hpp"
#include "canvas/foundation/revision.hpp"
#include "canvas/foundation/stable_order_key.hpp"
#include "canvas/foundation/world_geometry.hpp"
#include "canvas/semantic/object_record.hpp"
#include "canvas/semantic/semantic_generation.hpp"
#include "canvas/semantic/semantic_read_view.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace canvas {

using foundation::ContentRevision;
using foundation::ObjectId;
using foundation::SceneRevision;
using foundation::WorldPoint;
using foundation::WorldRect;

using SceneOrderKey = foundation::StableOrderKey;

enum class SceneObjectKind : std::uint8_t {
    kShape = 1,
    kImage = 2,
    kVectorPath = 3,
    kRichText = 4,
    kVectorStroke = 5,
    kDabStroke = 6,
};

// Renderer-neutral projection consumed by Scene Core and inspection tooling.
// It intentionally owns semantic values only; render payloads, frame state,
// GPU resources, and viewport state belong to later Render Core layers.
struct RuntimeSceneRecord final {
    semantic::ObjectId objectId{};
    semantic::ObjectKind kind = semantic::ObjectKind::kShape;
    std::uint32_t kindVersion = 0;
    semantic::Placement placement{};
    semantic::Transform2D transform{};
    semantic::PropertyBag properties{};
    semantic::ObjectContent content{};
    std::vector<semantic::EraseMaskRecord> eraseMasks;

    bool operator==(const RuntimeSceneRecord&) const = default;
};

struct RuntimeSceneProjection final {
    semantic::SemanticGeneration generation{};
    std::vector<RuntimeSceneRecord> records;

    [[nodiscard]] const RuntimeSceneRecord* find(semantic::ObjectId id) const noexcept {
        const auto it = std::find_if(records.begin(), records.end(),
                                     [id](const RuntimeSceneRecord& record) {
                                         return record.objectId == id;
                                     });
        return it == records.end() ? nullptr : &*it;
    }
};

// Renderer-neutral RuntimeScene facade. This declaration intentionally lives
// with the logical scene types, so consumers of this projection do not need to
// include the RF-01 render-participant interface.
class RuntimeScene final {
  public:
    RuntimeScene() = default;

    [[nodiscard]] semantic::SemanticGeneration generation() const noexcept {
        return _projection.generation;
    }
    [[nodiscard]] std::span<const RuntimeSceneRecord> records() const noexcept {
        return _projection.records;
    }
    [[nodiscard]] const RuntimeSceneRecord* find(semantic::ObjectId id) const noexcept {
        return _projection.find(id);
    }

    foundation::Result<RuntimeSceneProjection> replace(
        const semantic::SemanticReadView& post_state);
    foundation::Result<RuntimeSceneProjection> apply(
        const semantic::SemanticReadView& post_state);

  private:
    RuntimeSceneProjection _projection;
};

[[nodiscard]] inline RuntimeSceneProjection projectRuntimeScene(
    std::span<const semantic::ObjectRecord> source,
    semantic::SemanticGeneration generation = {}) {
    RuntimeSceneProjection projection;
    projection.generation = generation;
    projection.records.reserve(source.size());
    for (const semantic::ObjectRecord& record : source) {
        projection.records.push_back(RuntimeSceneRecord{
            .objectId = record.id,
            .kind = record.kind,
            .kindVersion = record.kind_version,
            .placement = record.placement,
            .transform = record.transform,
            .properties = record.properties,
            .content = record.content,
            .eraseMasks = record.erase_masks,
        });
    }
    std::sort(projection.records.begin(), projection.records.end(),
              [](const RuntimeSceneRecord& left, const RuntimeSceneRecord& right) {
                  if (left.placement.order_key != right.placement.order_key) {
                      return left.placement.order_key < right.placement.order_key;
                  }
                  return left.objectId < right.objectId;
              });
    return projection;
}

enum class SceneRecordFlags : std::uint32_t {
    kNone = 0,
    kVisible = 1U << 0U,
    kLocked = 1U << 1U,
    kHitTestable = 1U << 2U,
};

struct RenderPayloadRef final {
    std::uint32_t slot = 0;
    std::uint32_t generation = 0;

    bool operator==(const RenderPayloadRef&) const = default;
};

struct HitGeometryRef final {
    std::uint32_t slot = 0;
    std::uint32_t generation = 0;

    bool operator==(const HitGeometryRef&) const = default;
};

enum class InvalidationHintFlags : std::uint32_t {
    kNone = 0,
    kLayoutChanged = 1U << 0U,
    kResourceChanged = 1U << 1U,
    kOrderChanged = 1U << 2U,
};

struct InvalidationHints final {
    std::optional<WorldRect> worldDirty;
    InvalidationHintFlags flags = InvalidationHintFlags::kNone;
    // A hint is trusted only when both stamps match the delta that carries it.
    std::optional<SceneRevision> beforeRevision;
    std::optional<SceneRevision> afterRevision;
};

struct SceneRecord final {
    ObjectId objectId;
    SceneOrderKey orderKey;
    SceneObjectKind kind = SceneObjectKind::kShape;
    SceneRecordFlags flags = SceneRecordFlags::kNone;
    WorldRect worldBounds;
    ContentRevision contentRevision;
    RenderPayloadRef renderPayload;
    HitGeometryRef hitGeometry;

    bool operator==(const SceneRecord&) const = default;
};

enum class SceneMutationKind : std::uint8_t {
    kInsert,
    kUpdate,
    kRemove,
};

struct SceneMutation final {
    SceneMutationKind kind = SceneMutationKind::kInsert;
    ObjectId objectId;
    std::optional<SceneRecord> before;
    std::optional<SceneRecord> after;
};

struct CompiledSceneSnapshot final {
    SceneRevision sourceRevision;
    std::vector<SceneRecord> records;
};

struct CompiledSceneDelta final {
    SceneRevision beforeRevision;
    SceneRevision afterRevision;
    std::vector<SceneMutation> mutations;
    std::optional<InvalidationHints> hints;
};

} // namespace canvas
