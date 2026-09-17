#include "canvas/render/direct_reference_source.hpp"
#include "canvas/render/reference_draw_list.hpp"

#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "object_store_mutator.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

using canvas::ObjectId;
using canvas::RuntimeScene;
using canvas::RuntimeSceneRecord;
using canvas::foundation::SceneRevision;
using canvas::foundation::WorldPoint;
using canvas::foundation::WorldRect;
using canvas::render::CameraGeneration;
using canvas::render::CameraState;
using canvas::render::DirectReferenceSource;
using canvas::render::FrameId;
using canvas::render::FrameState;
using canvas::render::MetricsGeneration;
using canvas::render::ReferenceDrawList;
using canvas::render::SurfaceGeneration;
using canvas::render::SurfaceMetrics;
using canvas::render::ViewId;
using canvas::render::VisibilityResult;
using canvas::semantic::ObjectKind;
using canvas::semantic::ObjectRecord;
using canvas::semantic::IndexedObjectStore;
using canvas::semantic::ReferenceObjectStore;
using canvas::semantic::SemanticGeneration;
using canvas::semantic::SemanticReadView;
using canvas::semantic::internal::ObjectStoreMutator;

ObjectRecord fixtureRecord(std::uint64_t id, ObjectKind kind, std::uint64_t order) {
    ObjectRecord record;
    record.id = ObjectId::fromUint64(id);
    record.kind = kind;
    record.kind_version = 1;
    record.placement.order_key = canvas::semantic::OrderKey(
        std::vector<std::uint8_t>{static_cast<std::uint8_t>(order), 1U});
    record.transform.tx = static_cast<double>(id);
    record.properties.entries = {{0x10U,
                                  canvas::semantic::PropertyValue{static_cast<float>(id)}}};
    record.erase_masks = {{ObjectId::fromUint64(9000U + id),
                           canvas::semantic::FilledPathMask{}}};
    switch (kind) {
    case ObjectKind::kShape: record.content = canvas::semantic::ShapeContent{7U, 10.0, 20.0}; break;
    case ObjectKind::kImage: record.content = canvas::semantic::ImageContent{}; break;
    case ObjectKind::kVectorPath: record.content = canvas::semantic::VectorPathContent{}; break;
    case ObjectKind::kRichText: record.content = canvas::semantic::RichTextContent{}; break;
    case ObjectKind::kVectorStroke: record.content = canvas::semantic::VectorStrokeContent{}; break;
    case ObjectKind::kDabStroke: record.content = canvas::semantic::DabStrokeContent{}; break;
    case ObjectKind::kConnector: record.content = canvas::semantic::ConnectorContent{}; break;
    case ObjectKind::kSticky: record.content = canvas::semantic::StickyContent{3.0, 4.0}; break;
    case ObjectKind::kGroup: record.content = canvas::semantic::GroupContent{}; break;
    }
    return record;
}

FrameState frame(WorldPoint center = WorldPoint{3.0F, -4.0F},
                 WorldRect viewport = WorldRect{-10.0F, -20.0F, 100.0F, 200.0F},
                 ViewId view = ViewId{9}) {
    return FrameState{
        .viewId = view,
        .camera = CameraState{center, 2.0F, 0.25F, CameraGeneration{5}},
        .worldViewport = viewport,
        .metrics = SurfaceMetrics{800.0F, 600.0F, 1600U, 1200U, 2.0F, 1.5F},
        .sceneGeneration = SemanticGeneration{7},
        .sceneReadToken = SceneRevision{7},
        .surfaceGeneration = SurfaceGeneration{11},
        .metricsGeneration = MetricsGeneration{13},
        .frameId = FrameId{17},
    };
}

RuntimeScene makeRuntimeScene(ReferenceObjectStore& store) {
    const ObjectKind kinds[] = {ObjectKind::kShape, ObjectKind::kImage,
                                ObjectKind::kVectorPath, ObjectKind::kRichText,
                                ObjectKind::kVectorStroke, ObjectKind::kDabStroke,
                                ObjectKind::kConnector, ObjectKind::kSticky,
                                ObjectKind::kGroup};
    for (std::uint64_t i = 0; i < 9; ++i) {
        assert(ObjectStoreMutator::insertFresh(store, fixtureRecord(100U + i, kinds[i], 20U - i)));
    }
    RuntimeScene scene;
    SemanticReadView view(store, SemanticGeneration{7});
    assert(scene.replace(view));
    return scene;
}

void emitsNineKindsInVisibilityOrder() {
    ReferenceObjectStore store;
    std::vector<ObjectRecord> expectedRecords;
    RuntimeScene scene = makeRuntimeScene(store);
    const FrameState input = frame();
    const VisibilityResult visibility{
        .sceneGeneration = SemanticGeneration{7},
        .sceneReadToken = SceneRevision{7},
        .queryWorldRect = input.worldViewport,
        .candidatesExamined = 9,
        .visibleRecords = 9,
        .backToFront = {ObjectId::fromUint64(108), ObjectId::fromUint64(100),
                        ObjectId::fromUint64(103), ObjectId::fromUint64(101),
                        ObjectId::fromUint64(107), ObjectId::fromUint64(102),
                        ObjectId::fromUint64(106), ObjectId::fromUint64(104),
                        ObjectId::fromUint64(105)},
    };
    const auto result = DirectReferenceSource::build(input, visibility, scene);
    assert(result.hasValue());
    const ReferenceDrawList& list = result.value();
    assert(list.entries.size() == 9U);
    expectedRecords.reserve(9U);
    const ObjectKind kinds[] = {ObjectKind::kShape, ObjectKind::kImage,
                                ObjectKind::kVectorPath, ObjectKind::kRichText,
                                ObjectKind::kVectorStroke, ObjectKind::kDabStroke,
                                ObjectKind::kConnector, ObjectKind::kSticky,
                                ObjectKind::kGroup};
    for (std::uint64_t i = 0; i < 9U; ++i) {
        expectedRecords.push_back(fixtureRecord(100U + i, kinds[i], 20U - i));
    }
    assert(list.entries.front().record.objectId == ObjectId::fromUint64(108));
    assert(list.entries.front().record.kind == ObjectKind::kGroup);
    assert(!list.entries.front().contributesPixels);
    assert(list.entries.front().record.transform.tx == 108.0);
    assert(list.entries.back().record.objectId == ObjectId::fromUint64(105));
    assert(list.entries.back().record.kind == ObjectKind::kDabStroke);
    assert(list.entries.back().record.transform.tx == 105.0);
    assert(list.frame == input);
    assert(list.frame.sceneGeneration == SemanticGeneration{7});
    assert(list.frame.sceneReadToken == SceneRevision{7});
    assert(list.candidatesExamined == 9U);
    assert(list.visibleRecords == 9U);
    assert(list.queryWorldRect == input.worldViewport);
    assert(list.viewportClip == input.worldViewport);
    assert(list.worldToView == (canvas::render::WorldToViewAffine{
        1.9378248434212895, -0.4948079185090459,
        0.4948079185090459, 1.9378248434212895,
        396.1657571437723, 309.2357231292123}));
    assert(list.diagnostics.visibleIdsProcessed == 9U);
    assert(list.diagnostics.runtimeSceneFindLookups == 9U);
    assert(list.diagnostics.runtimeSceneRecordIterations == 0U);
    const ObjectKind expectedKinds[] = {
        ObjectKind::kGroup, ObjectKind::kShape, ObjectKind::kRichText,
        ObjectKind::kImage, ObjectKind::kSticky, ObjectKind::kVectorPath,
        ObjectKind::kConnector, ObjectKind::kVectorStroke, ObjectKind::kDabStroke};
    const std::size_t expectedCommands[] = {8U, 0U, 3U, 1U, 7U, 2U, 6U, 4U, 5U};
    for (std::size_t i = 0; i < list.entries.size(); ++i) {
        assert(list.entries[i].record.kind == expectedKinds[i]);
        assert(list.entries[i].command.index() == expectedCommands[i]);
        const auto expected = std::find_if(
            expectedRecords.begin(), expectedRecords.end(), [&](const ObjectRecord& record) {
                return record.id == list.entries[i].record.objectId;
            });
        assert(expected != expectedRecords.end());
        assert(list.entries[i].record.objectId == expected->id);
        assert(list.entries[i].record.kind == expected->kind);
        assert(list.entries[i].record.kindVersion == expected->kind_version);
        assert(list.entries[i].record.placement == expected->placement);
        assert(list.entries[i].record.transform == expected->transform);
        assert(list.entries[i].record.properties == expected->properties);
        assert(list.entries[i].record.content == expected->content);
        assert(list.entries[i].record.eraseMasks == expected->erase_masks);
        assert(list.entries[i].record.geometryBounds == WorldRect{});
        assert(list.entries[i].record.visualBounds == WorldRect{});
        assert(list.entries[i].record.worldBounds == WorldRect{});
        assert(list.entries[i].record.referenceGeometryDigest.empty());
        assert(list.entries[i].record.directDependencies.empty());
        switch (expectedKinds[i]) {
        case ObjectKind::kShape:
            assert(std::get<canvas::render::ShapeReferenceCommand>(list.entries[i].command).content ==
                   std::get<canvas::semantic::ShapeContent>(expected->content));
            break;
        case ObjectKind::kImage:
            assert(std::get<canvas::render::ImageReferenceCommand>(list.entries[i].command).content ==
                   std::get<canvas::semantic::ImageContent>(expected->content));
            break;
        case ObjectKind::kVectorPath:
            assert(std::get<canvas::render::VectorPathReferenceCommand>(list.entries[i].command).content ==
                   std::get<canvas::semantic::VectorPathContent>(expected->content));
            break;
        case ObjectKind::kRichText:
            assert(std::get<canvas::render::RichTextReferenceCommand>(list.entries[i].command).content ==
                   std::get<canvas::semantic::RichTextContent>(expected->content));
            break;
        case ObjectKind::kVectorStroke:
            assert(std::get<canvas::render::VectorStrokeReferenceCommand>(list.entries[i].command).content ==
                   std::get<canvas::semantic::VectorStrokeContent>(expected->content));
            break;
        case ObjectKind::kDabStroke:
            assert(std::get<canvas::render::DabStrokeReferenceCommand>(list.entries[i].command).content ==
                   std::get<canvas::semantic::DabStrokeContent>(expected->content));
            break;
        case ObjectKind::kConnector:
            assert(std::get<canvas::render::ConnectorReferenceCommand>(list.entries[i].command).content ==
                   std::get<canvas::semantic::ConnectorContent>(expected->content));
            break;
        case ObjectKind::kSticky:
            assert(std::get<canvas::render::StickyReferenceCommand>(list.entries[i].command).content ==
                   std::get<canvas::semantic::StickyContent>(expected->content));
            break;
        case ObjectKind::kGroup:
            assert(std::holds_alternative<canvas::render::GroupReferenceCommand>(list.entries[i].command));
            break;
        }
    }
    assert(!list.canonicalBytes.empty());
    assert(!list.digest.empty());
    std::cout << "{\"nine_kind_entries\":9,\"group_zero_pixels\":true,\"order_exact\":true,"
              << "\"canonical_bytes\":" << list.canonicalBytes.size() << ",\"digest\":\""
              << list.digest << "\",\"world_to_view\":[" << list.worldToView.a << ","
              << list.worldToView.b << "," << list.worldToView.c << "," << list.worldToView.d
              << "," << list.worldToView.tx << "," << list.worldToView.ty << "]}\n";
}

void rejectsIdentityMismatchAndMissingRecord() {
    ReferenceObjectStore store;
    RuntimeScene scene = makeRuntimeScene(store);
    const FrameState input = frame();
    VisibilityResult visibility{.sceneGeneration = SemanticGeneration{8},
                                .sceneReadToken = SceneRevision{7},
                                .queryWorldRect = input.worldViewport,
                                .visibleRecords = 1,
                                .backToFront = {ObjectId::fromUint64(100)}};
    assert(!DirectReferenceSource::build(input, visibility, scene));
    visibility.sceneGeneration = SemanticGeneration{7};
    visibility.backToFront = {ObjectId::fromUint64(999)};
    assert(!DirectReferenceSource::build(input, visibility, scene));
}

void replayIsByteAndDigestStable() {
    ReferenceObjectStore store;
    RuntimeScene scene = makeRuntimeScene(store);
    const FrameState input = frame();
    const VisibilityResult visibility{.sceneGeneration = SemanticGeneration{7},
                                      .sceneReadToken = SceneRevision{7},
                                      .queryWorldRect = input.worldViewport,
                                      .visibleRecords = 1,
                                      .backToFront = {ObjectId::fromUint64(100)}};
    const auto first = DirectReferenceSource::build(input, visibility, scene);
    const auto second = DirectReferenceSource::build(input, visibility, scene);
    assert(first.hasValue() && second.hasValue());
    assert(first.value().canonicalBytes == second.value().canonicalBytes);
    assert(first.value().digest == second.value().digest);

    VisibilityResult reordered = visibility;
    reordered.backToFront.front() = ObjectId::fromUint64(101);
    const auto orderMutation = DirectReferenceSource::build(input, reordered, scene);
    assert(orderMutation.hasValue());
    assert(orderMutation.value().canonicalBytes != first.value().canonicalBytes);
    const FrameState cameraMutation = frame(WorldPoint{8.0F, -4.0F});
    const auto cameraResult = DirectReferenceSource::build(cameraMutation, visibility, scene);
    assert(cameraResult.hasValue());
    assert(cameraResult.value().digest != first.value().digest);
    const FrameState clipMutation = frame(WorldPoint{3.0F, -4.0F},
                                          WorldRect{-11.0F, -20.0F, 100.0F, 200.0F});
    const auto clipResult = DirectReferenceSource::build(clipMutation, visibility, scene);
    assert(clipResult.hasValue());
    assert(clipResult.value().canonicalBytes != first.value().canonicalBytes);
    const auto identityResult = DirectReferenceSource::build(
        frame(WorldPoint{3.0F, -4.0F}, input.worldViewport, ViewId{10}), visibility, scene);
    assert(identityResult.hasValue());
    assert(identityResult.value().digest != first.value().digest);
    const auto changedPlan = [&](const ObjectRecord& record) {
        ReferenceObjectStore changedStore;
        assert(ObjectStoreMutator::insertFresh(changedStore, record));
        RuntimeScene changedScene;
        assert(changedScene.replace(SemanticReadView(changedStore, SemanticGeneration{7})));
        return DirectReferenceSource::build(input, visibility, changedScene);
    };
    auto contentMutation = fixtureRecord(100U, ObjectKind::kShape, 20U);
    contentMutation.content = canvas::semantic::ShapeContent{99U, 77.0, 88.0};
    const auto contentResult = changedPlan(contentMutation);
    assert(contentResult && contentResult.value().canonicalBytes != first.value().canonicalBytes);
    auto transformMutation = fixtureRecord(100U, ObjectKind::kShape, 20U);
    transformMutation.transform.tx = 777.0;
    const auto transformResult = changedPlan(transformMutation);
    assert(transformResult && transformResult.value().digest != first.value().digest);
    auto propertyMutation = fixtureRecord(100U, ObjectKind::kShape, 20U);
    propertyMutation.properties.entries.front().value = 999.0F;
    const auto propertyResult = changedPlan(propertyMutation);
    assert(propertyResult && propertyResult.value().digest != first.value().digest);
    auto eraseMutation = fixtureRecord(100U, ObjectKind::kShape, 20U);
    eraseMutation.erase_masks.front().id = ObjectId::fromUint64(12345U);
    const auto eraseResult = changedPlan(eraseMutation);
    assert(eraseResult && eraseResult.value().digest != first.value().digest);
    auto dependencyMutation = fixtureRecord(100U, ObjectKind::kShape, 20U);
    dependencyMutation.placement.parent_id = ObjectId::fromUint64(44U);
    const auto dependencyResult = changedPlan(dependencyMutation);
    assert(dependencyResult && dependencyResult.value().digest != first.value().digest);
    std::cout << "{\"digest_replay\":true,\"mutation_sensitivity\":true,"
              << "\"identity\":true,\"order\":true,\"transform\":true,"
              << "\"property\":true,\"content\":true,\"dependency\":true,"
              << "\"erase_mask\":true,\"camera\":true,\"clip\":true}\n";
}

void visibleDrivenLocality() {
    IndexedObjectStore store;
    for (std::uint64_t id = 1; id <= 100000U; ++id) {
        assert(ObjectStoreMutator::insertFresh(
            store, fixtureRecord(id, ObjectKind::kShape, id == 0 ? 1U : id)));
    }
    RuntimeScene scene;
    assert(scene.replace(SemanticReadView(store, SemanticGeneration{7})));
    const FrameState input = frame();
    VisibilityResult visibility{.sceneGeneration = SemanticGeneration{7},
                                .sceneReadToken = SceneRevision{7},
                                .queryWorldRect = input.worldViewport,
                                .candidatesExamined = 500,
                                .visibleRecords = 500,
                                .backToFront = {}};
    visibility.backToFront.reserve(500);
    for (std::uint64_t id = 1; id <= 500U; ++id) visibility.backToFront.push_back(ObjectId::fromUint64(id));
    const auto result = DirectReferenceSource::build(input, visibility, scene);
    assert(result.hasValue());
    assert(result.value().entries.size() == 500U);
    assert(result.value().diagnostics.visibleIdsProcessed == 500U);
    assert(result.value().diagnostics.runtimeSceneFindLookups == 500U);
    assert(result.value().diagnostics.runtimeSceneRecordIterations == 0U);
    assert(result.value().diagnostics.semanticObjectIterations == 0U);
    std::cout << "{\"total_scene_records\":100000,\"visible_ids\":500,\"find_lookups\":500,\"record_iterations\":0,\"semantic_iterations\":0}\n";
}

} // namespace

int main() {
    emitsNineKindsInVisibilityOrder();
    rejectsIdentityMismatchAndMissingRecord();
    replayIsByteAndDigestStable();
    visibleDrivenLocality();
    return 0;
}
