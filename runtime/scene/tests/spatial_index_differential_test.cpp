#include "canvas/scene/linear_spatial_index.hpp"
#include "canvas/scene/uniform_grid_spatial_index.hpp"
#include "canvas/scene/spatial_delta.hpp"

#include <cstdlib>
#include <iostream>

namespace {
using namespace canvas;

SpatialRecord record(std::uint64_t id, WorldRect bounds) {
    return {ObjectId::fromUint64(id), bounds};
}

SceneRecord sceneRecord(const SpatialRecord& value) {
    return SceneRecord{value.objectId, SceneOrderKey(1), SceneObjectKind::kShape,
                       SceneRecordFlags::kVisible, value.worldBounds, ContentRevision(1),
                       RenderPayloadRef{1, 1}, HitGeometryRef{1, 1}};
}

SpatialMutation insert(const SpatialRecord& value) {
    return {SceneMutationKind::kInsert, value.objectId, std::nullopt, value.worldBounds};
}

SpatialMutation update(const SpatialRecord& before, const SpatialRecord& after) {
    return {SceneMutationKind::kUpdate, before.objectId, before.worldBounds, after.worldBounds};
}
} // namespace

int main() {
    UniformGridSpatialIndex grid(1.0F);
    LinearSpatialIndex linear;
    const SpatialRecord first = record(1, {0, 0, 2, 2});
    const SpatialRecord second = record(2, {10, 10, 12, 12});
    const std::vector<SpatialRecord> initial{first, second};
    auto gridSeed = grid.prepareReplace(initial, SceneRevision(1));
    auto linearSeed = linear.prepareReplace(initial, SceneRevision(1));
    if (!gridSeed || !linearSeed) return EXIT_FAILURE;
    grid.commit(std::move(gridSeed.value()));
    linear.commit(std::move(linearSeed.value()));
    SpatialRecord moved = first;
    moved.worldBounds = {1, 0, 3, 2};
    const SceneRecord firstScene = sceneRecord(first);
    const SceneRecord movedScene = sceneRecord(moved);
    SceneDelta delta{SceneRevision(1), SceneRevision(2), {}, {}, {}, {}, {},
                     {SceneMutation{SceneMutationKind::kUpdate, first.objectId, firstScene,
                                    movedScene}}};
    auto gridPrepared = grid.prepareDelta(delta, SceneRevision(1), SceneRevision(2));
    auto linearPrepared = linear.prepareDelta(delta, SceneRevision(1), SceneRevision(2));
    if (!gridPrepared || !linearPrepared) return EXIT_FAILURE;
    grid.commit(std::move(gridPrepared.value()));
    linear.commit(std::move(linearPrepared.value()));
    const auto gridQuery = grid.query({0, 0, 4, 4});
    const auto linearQuery = linear.query({0, 0, 4, 4});
    if (!gridQuery || !linearQuery || gridQuery.value().candidates != linearQuery.value().candidates) {
        std::cerr << "uniform grid and linear oracle diverged\n";
        return EXIT_FAILURE;
    }
    const auto diagnostics = grid.diagnostics();
    if (diagnostics.fullSpatialRebuildCount != 0 || diagnostics.fullSpatialRecordCloneCount != 0 ||
        diagnostics.fullCellScanCount != 0 || diagnostics.membershipRetainedCount == 0) {
        std::cerr << "localized diagnostics violated\n";
        return EXIT_FAILURE;
    }
    const ObjectId hugeId = ObjectId::fromUint64(0xA500000000000001ULL);
    const WorldRect hugeBounds{-262144.0F, -262144.0F, 262144.0F, 262144.0F};
    SpatialMutation hugeInsert{SceneMutationKind::kInsert, hugeId, std::nullopt, hugeBounds};
    SceneDelta hugeDelta{SceneRevision(2), SceneRevision(3), {}, {}, {}, {}, {},
                         {SceneMutation{SceneMutationKind::kInsert, hugeId, std::nullopt,
                                         sceneRecord(record(0xA500000000000001ULL, hugeBounds))}}};
    auto hugePrepared = grid.prepareSpatialDelta(makeSpatialDelta(hugeDelta), SceneRevision(2), SceneRevision(3));
    if (!hugePrepared) {
        std::cerr << "huge object prepare failed\n";
        return EXIT_FAILURE;
    }
    grid.commit(std::move(hugePrepared.value()));
    if (!grid.query({-128, -128, 128, 128})) return EXIT_FAILURE;
    const auto beforeFailure = grid.diagnostics();
    SceneDelta invalid{SceneRevision(2), SceneRevision(3), {}, {}, {}, {}, {},
                       {SceneMutation{SceneMutationKind::kUpdate, second.objectId,
                                       sceneRecord(first), sceneRecord(moved)}}};
    if (grid.prepareDelta(invalid, SceneRevision(2), SceneRevision(3)) ||
        grid.diagnostics().affectedEntryCount != beforeFailure.affectedEntryCount ||
        grid.diagnostics().membershipAddCount != beforeFailure.membershipAddCount) {
        std::cerr << "failed prepare contaminated diagnostics\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
