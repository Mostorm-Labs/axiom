#include "canvas/scene/runtime_scene_projection.hpp"
#include "canvas/scene/scene.hpp"
#include "canvas/scene/scene_impact.hpp"
#include "canvas/scene/testing/fake_render_scene.hpp"
#include "canvas/scene/testing/fake_spatial_index.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "object_store_mutator.hpp"

#include <cassert>
#include <memory>

namespace {
canvas::semantic::ObjectRecord imageRecord(canvas::semantic::ImageContent content) {
  canvas::semantic::ObjectRecord record{};
  record.id = canvas::foundation::ObjectId::fromUint64(701U);
  record.kind = canvas::semantic::ObjectKind::kImage;
  record.kind_version = 1U;
  record.placement.order_key = canvas::semantic::OrderKey{{0x40U}};
  record.content = std::move(content);
  return record;
}

canvas::SceneRecord sceneRecord(std::uint64_t contentRevision,
                                canvas::WorldRect bounds) {
  return canvas::SceneRecord{
      .objectId = canvas::ObjectId::fromUint64(701U),
      .orderKey = canvas::SceneOrderKey(1U),
      .kind = canvas::SceneObjectKind::kImage,
      .flags = static_cast<canvas::SceneRecordFlags>(
          static_cast<std::uint32_t>(canvas::SceneRecordFlags::kVisible) |
          static_cast<std::uint32_t>(canvas::SceneRecordFlags::kHitTestable)),
      .worldBounds = bounds,
      .contentRevision = canvas::ContentRevision(contentRevision),
      .renderPayload = canvas::RenderPayloadRef{701U, static_cast<std::uint32_t>(contentRevision)},
      .hitGeometry = canvas::HitGeometryRef{701U, static_cast<std::uint32_t>(contentRevision)},
  };
}
}  // namespace

int main() {
  using namespace canvas;
  semantic::ImageContent base{};
  base.resource_id = semantic::ResourceId{foundation::ObjectId::fromUint64(88U)};
  base.intrinsic_width = 400.0;
  base.intrinsic_height = 200.0;
  base.width = 300.0;
  base.height = 300.0;
  base.content_mode = semantic::ImageContentMode::kFit;

  semantic::ReferenceObjectStore store;
  assert(semantic::internal::ObjectStoreMutator::insertFresh(store, imageRecord(base)));
  const auto first = projectRuntimeScene(store.allObjects(), semantic::SemanticGeneration(1));
  assert(first.records.size() == 1U);
  const auto firstDigest = first.records.front().referenceGeometryDigest;
  assert(first.records.front().geometryBounds.isFiniteAndOrdered());
  assert(first.records.front().geometryBounds.right > first.records.front().geometryBounds.left);
  assert(first.records.front().geometryBounds.isFiniteAndOrdered());

  auto cropOnly = base;
  cropOnly.source_rect = semantic::NormalizedRect{0.25, 0.0, 0.5, 1.0};
  assert(semantic::internal::ObjectStoreMutator::replaceExisting(store, imageRecord(cropOnly)));
  const auto cropProjection = projectRuntimeScene(store.allObjects(), semantic::SemanticGeneration(2));
  assert(cropProjection.records.front().referenceGeometryDigest != firstDigest);

  auto cropped = base;
  cropped.source_rect = semantic::NormalizedRect{0.25, 0.0, 0.5, 1.0};
  cropped.content_mode = semantic::ImageContentMode::kFill;
  auto changed = imageRecord(cropped);
  assert(semantic::internal::ObjectStoreMutator::replaceExisting(store, changed));
  const auto second = projectRuntimeScene(store.allObjects(), semantic::SemanticGeneration(3));
  assert(second.records.size() == 1U);
  assert(second.records.front().referenceGeometryDigest != firstDigest);
  assert(second.records.front().geometryBounds.isFiniteAndOrdered());
  assert(second.records.front().geometryBounds.right > second.records.front().geometryBounds.left);
  assert(second.records.front().geometryBounds.isFiniteAndOrdered());

  const auto impact = scene::classifyImpact(
      semantic::ObjectSemanticChange{changed.id, semantic::SemanticChangeFlags::kContent, {}},
      &changed);
  assert(impact.local_geometry == scene::DirtyState::kDirty);
  assert(impact.world_bounds == scene::DirtyState::kDirty);
  assert(impact.spatial == scene::DirtyState::kDirty);
  assert(impact.resource == scene::DirtyState::kDirty);

  auto render = std::make_unique<testing::FakeRenderScene>();
  auto spatial = std::make_unique<testing::FakeSpatialIndex>();
  Scene scene(std::move(render), std::move(spatial));
  assert(scene.replace(CompiledSceneSnapshot{SceneRevision(1),
      {sceneRecord(1U, {0, 0, 300, 300})}}).hasValue());
  const auto hitBefore = scene.hitTest(HitTestRequest{{150, 150}, 0.0F, {}, 1U});
  assert(hitBefore.hasValue() && !hitBefore.value().frontToBack.empty());
  const auto update = scene.apply(CompiledSceneDelta{
      SceneRevision(1), SceneRevision(2),
      {SceneMutation{SceneMutationKind::kUpdate, ObjectId::fromUint64(701U),
          sceneRecord(1U, {0, 0, 300, 300}), sceneRecord(2U, {20, 30, 320, 330})}},
      std::nullopt});
  assert(update.hasValue());
  const auto damage = scene.collectDamage(SceneRevision(1), SceneRevision(2));
  assert(!damage.rects.empty());
  const auto hitAfter = scene.hitTest(HitTestRequest{{310, 320}, 0.0F, {}, 1U});
  assert(hitAfter.hasValue() && !hitAfter.value().frontToBack.empty());
  return 0;
}
