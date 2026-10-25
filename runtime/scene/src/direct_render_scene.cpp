#include "canvas/scene/direct_render_scene.hpp"

#include <algorithm>
#include <cmath>
#include <new>
#include <utility>

namespace canvas {
class DirectRenderScene::PreparedUpdate final : public IPreparedRenderSceneUpdate {
  public:
    SceneRevision revision;
    std::vector<RenderRecord> records;
    std::unordered_map<ObjectId, std::size_t, foundation::ObjectIdHash> index;
    std::vector<SceneMutation> mutations;
    bool localized = false;
};

namespace {

foundation::Error makeError(foundation::ErrorCode code, const char* message) {
    return foundation::Error{code, message};
}

float distanceToRect(const WorldPoint& point, const WorldRect& rect) {
    const float dx = std::max({rect.left - point.x, 0.0F, point.x - rect.right});
    const float dy = std::max({rect.top - point.y, 0.0F, point.y - rect.bottom});
    return std::hypot(dx, dy);
}

bool hasVisibleFill(const semantic::PropertyBag& properties) noexcept {
    for (const auto& entry : properties.entries) {
        if (entry.field_id != 0x100U) continue;
        if (const auto* fill = std::get_if<semantic::FillStyleValue>(&entry.value)) {
            if (const auto* solid = std::get_if<semantic::SolidFill>(fill)) return solid->color.a > 0.0F;
            return false;
        }
    }
    return true;
}

float strokeHalfWidth(const semantic::PropertyBag& properties) noexcept {
    for (const auto& entry : properties.entries) {
        if (entry.field_id != 0x101U) continue;
        if (const auto* stroke = std::get_if<semantic::StrokeStyleValue>(&entry.value)) {
            if (const auto* solid = std::get_if<semantic::SolidStroke>(stroke))
                return static_cast<float>(std::max(0.0, solid->width) * 0.5);
            return 0.0F;
        }
    }
    return 0.0F;
}

} // namespace

foundation::Result<std::unique_ptr<IPreparedRenderSceneUpdate>>
DirectRenderScene::prepareRecords(std::vector<RenderRecord> records, SceneRevision revision) const {
    try {
        std::sort(records.begin(),
                  records.end(),
                  [](const RenderRecord& left, const RenderRecord& right) {
                      return left.orderKey < right.orderKey ||
                             (left.orderKey == right.orderKey && left.objectId < right.objectId);
                  });
        auto update = std::make_unique<PreparedUpdate>();
        update->revision = revision;
        update->records = std::move(records);
        update->index.reserve(update->records.size());
        for (std::size_t index = 0; index < update->records.size(); ++index) {
            if (!update->index.emplace(update->records[index].objectId, index).second) {
                return foundation::Result<std::unique_ptr<IPreparedRenderSceneUpdate>>::failure(
                    makeError(foundation::ErrorCode::kDuplicateObject,
                              "DirectRenderScene contains a duplicate ObjectId"));
            }
        }
        return foundation::Result<std::unique_ptr<IPreparedRenderSceneUpdate>>::success(
            std::move(update));
    } catch (const std::bad_alloc&) {
        return foundation::Result<std::unique_ptr<IPreparedRenderSceneUpdate>>::failure(makeError(
            foundation::ErrorCode::kOutOfMemory, "DirectRenderScene could not prepare update"));
    }
}

foundation::Result<std::unique_ptr<IPreparedRenderSceneUpdate>>
DirectRenderScene::prepareReplace(std::span<const SceneRecord> records,
                                  SceneRevision revision) const {
    ++_prepareCount;
    try {
        std::vector<RenderRecord> nextRecords;
        nextRecords.reserve(records.size());
        for (const SceneRecord& record : records) {
            nextRecords.push_back(RenderRecord{
                record.objectId,
                record.orderKey,
                record.worldBounds,
                record.renderPayload,
                record.derivedGeometry,
                record.localTransform,
                record.properties,
            });
        }
        return prepareRecords(std::move(nextRecords), revision);
    } catch (const std::bad_alloc&) {
        return foundation::Result<std::unique_ptr<IPreparedRenderSceneUpdate>>::failure(
            makeError(foundation::ErrorCode::kOutOfMemory,
                      "DirectRenderScene could not prepare replacement"));
    }
}

foundation::Result<std::unique_ptr<IPreparedRenderSceneUpdate>>
DirectRenderScene::prepareApply(std::span<const SceneMutation> mutations,
                                SceneRevision beforeRevision,
                                SceneRevision afterRevision) const {
    ++_prepareCount;
    if (beforeRevision != _revision || afterRevision <= beforeRevision) {
        return foundation::Result<std::unique_ptr<IPreparedRenderSceneUpdate>>::failure(
            makeError(foundation::ErrorCode::kInvalidRevision,
                      "DirectRenderScene delta revision is invalid"));
    }
    try {
        std::vector<RenderRecord> nextRecords = _records;
        for (const SceneMutation& mutation : mutations) {
            const auto found = std::find_if(
                nextRecords.begin(), nextRecords.end(), [&mutation](const RenderRecord& record) {
                    return record.objectId == mutation.objectId;
                });
            switch (mutation.kind) {
            case SceneMutationKind::kInsert:
                if (found != nextRecords.end() || !mutation.after) {
                    return foundation::Result<std::unique_ptr<IPreparedRenderSceneUpdate>>::failure(
                        makeError(foundation::ErrorCode::kInvalidRecord,
                                  "DirectRenderScene insert is inconsistent"));
                }
                nextRecords.push_back(RenderRecord{
                    mutation.after->objectId,
                    mutation.after->orderKey,
                    mutation.after->worldBounds,
                    mutation.after->renderPayload,
                    mutation.after->derivedGeometry,
                    mutation.after->localTransform,
                    mutation.after->properties,
                });
                break;
            case SceneMutationKind::kUpdate:
                if (found == nextRecords.end() || !mutation.before || !mutation.after) {
                    return foundation::Result<std::unique_ptr<IPreparedRenderSceneUpdate>>::failure(
                        makeError(foundation::ErrorCode::kInvalidRecord,
                                  "DirectRenderScene update is inconsistent"));
                }
                *found = RenderRecord{
                    mutation.after->objectId,
                    mutation.after->orderKey,
                    mutation.after->worldBounds,
                    mutation.after->renderPayload,
                    mutation.after->derivedGeometry,
                    mutation.after->localTransform,
                    mutation.after->properties,
                };
                break;
            case SceneMutationKind::kRemove:
                if (found == nextRecords.end() || !mutation.before || mutation.after) {
                    return foundation::Result<std::unique_ptr<IPreparedRenderSceneUpdate>>::failure(
                        makeError(foundation::ErrorCode::kInvalidRecord,
                                  "DirectRenderScene remove is inconsistent"));
                }
                nextRecords.erase(found);
                break;
            default:
                return foundation::Result<std::unique_ptr<IPreparedRenderSceneUpdate>>::failure(
                    makeError(foundation::ErrorCode::kInvalidRecord,
                              "DirectRenderScene mutation kind is unknown"));
            }
        }
        return prepareRecords(std::move(nextRecords), afterRevision);
    } catch (const std::bad_alloc&) {
        return foundation::Result<std::unique_ptr<IPreparedRenderSceneUpdate>>::failure(makeError(
            foundation::ErrorCode::kOutOfMemory, "DirectRenderScene could not prepare delta"));
    }
}

foundation::Result<std::unique_ptr<IPreparedRenderSceneUpdate>>
DirectRenderScene::prepareDelta(const SceneDelta& delta,
                                SceneRevision beforeRevision,
                                SceneRevision afterRevision) const {
    ++_prepareCount;
    if (beforeRevision != _revision || afterRevision <= beforeRevision) {
        return foundation::Result<std::unique_ptr<IPreparedRenderSceneUpdate>>::failure(
            makeError(foundation::ErrorCode::kInvalidRevision,
                      "DirectRenderScene delta revision is invalid"));
    }
    try {
        std::vector<RenderRecord> changed;
        changed.reserve(delta.mutations.size());
        for (const SceneMutation& mutation : delta.mutations) {
            if (mutation.kind != SceneMutationKind::kRemove && mutation.after) {
                changed.push_back(RenderRecord{mutation.after->objectId,
                                               mutation.after->orderKey,
                                               mutation.after->worldBounds,
                                               mutation.after->renderPayload, mutation.after->derivedGeometry, mutation.after->localTransform, mutation.after->properties});
            }
        }
        auto update = std::make_unique<PreparedUpdate>();
        update->revision = afterRevision;
        update->records = std::move(changed);
        update->mutations = delta.mutations;
        update->localized = true;
        _localizedMutationCount += delta.mutations.size();
        return foundation::Result<std::unique_ptr<IPreparedRenderSceneUpdate>>::success(
            std::move(update));
    } catch (const std::bad_alloc&) {
        return foundation::Result<std::unique_ptr<IPreparedRenderSceneUpdate>>::failure(
            makeError(foundation::ErrorCode::kOutOfMemory,
                      "DirectRenderScene could not prepare delta"));
    }
}

void DirectRenderScene::commit(std::unique_ptr<IPreparedRenderSceneUpdate> prepared) noexcept {
    auto* update = static_cast<PreparedUpdate*>(prepared.get());
    if (update->localized) {
        for (const SceneMutation& mutation : update->mutations) {
            const auto found = _index.find(mutation.objectId);
            if (mutation.kind == SceneMutationKind::kInsert && mutation.after) {
                RenderRecord record{mutation.after->objectId, mutation.after->orderKey,
                                    mutation.after->worldBounds, mutation.after->renderPayload, mutation.after->derivedGeometry, mutation.after->localTransform, mutation.after->properties};
                _index[record.objectId] = _records.size();
                _records.push_back(record);
            } else if (mutation.kind == SceneMutationKind::kUpdate && found != _index.end() &&
                       mutation.after) {
                _records[found->second] = RenderRecord{mutation.after->objectId,
                                                       mutation.after->orderKey,
                                                       mutation.after->worldBounds,
                                                       mutation.after->renderPayload, mutation.after->derivedGeometry, mutation.after->localTransform, mutation.after->properties};
            } else if (mutation.kind == SceneMutationKind::kRemove && found != _index.end()) {
                const std::size_t removed = found->second;
                const std::size_t last = _records.size() - 1;
                if (removed != last) {
                    _records[removed] = _records[last];
                    _index[_records[removed].objectId] = removed;
                }
                _records.pop_back();
                _index.erase(found->first);
            }
        }
        _revision = update->revision;
        ++_commitCount;
        return;
    }
    _records.swap(update->records);
    _index.swap(update->index);
    _revision = update->revision;
    ++_commitCount;
}

foundation::Result<PreciseHit>
DirectRenderScene::preciseHitTest(const PreciseHitRequest& request) const {
    if (!std::isfinite(request.worldPoint.x) || !std::isfinite(request.worldPoint.y) ||
        !std::isfinite(request.tolerance) || request.tolerance < 0.0F) {
        return foundation::Result<PreciseHit>::failure(
            makeError(foundation::ErrorCode::kInvalidArgument, "Precise hit request is invalid"));
    }
    const auto found = _index.find(request.objectId);
    if (found == _index.end()) {
        return foundation::Result<PreciseHit>::failure(
            makeError(foundation::ErrorCode::kMissingObject, "Precise hit ObjectId is missing"));
    }
    const RenderRecord& record = _records[found->second];
    float distance = distanceToRect(request.worldPoint, record.worldBounds);
    bool hit = distance <= request.tolerance;
    const auto world = [&](WorldPoint point) {
        const auto& transform = record.localTransform;
        return WorldPoint{static_cast<float>(transform.a * point.x + transform.c * point.y + transform.tx),
                          static_cast<float>(transform.b * point.x + transform.d * point.y + transform.ty)};
    };
    if (const auto* triangle = std::get_if<scene::TriangleGeometry>(&record.derivedGeometry)) {
        const scene::TriangleGeometry transformed{world(triangle->top), world(triangle->right), world(triangle->left), {}};
        distance = std::min({scene::distanceToSegment(request.worldPoint, transformed.top, transformed.right),
                             scene::distanceToSegment(request.worldPoint, transformed.right, transformed.left),
                             scene::distanceToSegment(request.worldPoint, transformed.left, transformed.top)});
        const float stroke = strokeHalfWidth(record.properties);
        hit = (hasVisibleFill(record.properties) && scene::pointInTriangle(request.worldPoint, transformed)) ||
              distance <= request.tolerance + stroke;
        if (hit) distance = 0.0F;
    } else if (const auto* connector = std::get_if<scene::ConnectorGeometry>(&record.derivedGeometry)) {
        std::vector<foundation::WorldPoint> transformedRoute;
        transformedRoute.reserve(connector->route.size());
        for (const auto point : connector->route) transformedRoute.push_back(world(point));
        const scene::ConnectorGeometry transformed{world(connector->start), world(connector->end), std::move(transformedRoute), connector->arrow_end};
        hit = scene::pointOnConnector(request.worldPoint, transformed, request.tolerance);
        distance = scene::distanceToSegment(request.worldPoint, transformed.start, transformed.end);
        if (connector->arrow_end) {
            const auto head = scene::arrowHead(*connector);
            const scene::TriangleGeometry transformedHead{world(head.tip), world(head.left), world(head.right), {}};
            const float headDistance = std::min({
                scene::distanceToSegment(request.worldPoint, transformedHead.top, transformedHead.right),
                scene::distanceToSegment(request.worldPoint, transformedHead.right, transformedHead.left),
                scene::distanceToSegment(request.worldPoint, transformedHead.left, transformedHead.top)});
            hit = hit || scene::pointInTriangle(request.worldPoint, transformedHead) ||
                  headDistance <= request.tolerance;
            distance = std::min(distance, headDistance);
        }
    }
    return foundation::Result<PreciseHit>::success(PreciseHit{hit, distance});
}

foundation::Result<SceneDrawList>
DirectRenderScene::buildDrawList(std::span<const ObjectId> backToFront) const {
    try {
        SceneDrawList drawList{
            .revision = _revision,
            .items = {},
        };
        drawList.items.reserve(backToFront.size());
        for (ObjectId objectId : backToFront) {
            const auto found = _index.find(objectId);
            if (found == _index.end()) {
                return foundation::Result<SceneDrawList>::failure(makeError(
                    foundation::ErrorCode::kMissingObject, "Draw-list ObjectId is missing"));
            }
            const RenderRecord& record = _records[found->second];
            drawList.items.push_back(SceneDrawItem{
                record.objectId,
                record.orderKey,
                record.renderPayload,
            });
        }
        return foundation::Result<SceneDrawList>::success(std::move(drawList));
    } catch (const std::bad_alloc&) {
        return foundation::Result<SceneDrawList>::failure(makeError(
            foundation::ErrorCode::kOutOfMemory, "DirectRenderScene could not build draw list"));
    }
}

RenderSceneDiagnostics DirectRenderScene::diagnostics() const {
    return RenderSceneDiagnostics{
        .revision = _revision,
        .nodeCount = _records.size(),
        .prepareCount = _prepareCount,
        .commitCount = _commitCount,
        .localizedMutationCount = _localizedMutationCount,
    };
}

} // namespace canvas
