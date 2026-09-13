#include "canvas/scene/scene_delta.hpp"
#include "canvas/scene/scene_record_store.hpp"

#include <cstdlib>
#include <iostream>

namespace {

using namespace canvas;

SceneRecord record(std::uint64_t id, std::uint64_t order) {
    return SceneRecord{
        .objectId = ObjectId::fromUint64(id),
        .orderKey = SceneOrderKey(order),
        .kind = SceneObjectKind::kShape,
        .flags = SceneRecordFlags::kVisible,
        .worldBounds = WorldRect{0.0F, 0.0F, 1.0F, 1.0F},
        .contentRevision = ContentRevision(1),
        .renderPayload = RenderPayloadRef{1, 1},
        .hitGeometry = HitGeometryRef{1, 1},
    };
}

} // namespace

int main() {
    const SceneRecord before = record(1, 1);
    SceneRecord after = before;
    after.orderKey = SceneOrderKey(2);
    const CompiledSceneDelta input{
        .beforeRevision = SceneRevision(1),
        .afterRevision = SceneRevision(2),
        .mutations = {SceneMutation{
            .kind = SceneMutationKind::kUpdate,
            .objectId = before.objectId,
            .before = before,
            .after = after,
        }},
        .hints = std::nullopt,
    };
    const SceneDelta delta = makeSceneDelta(input);
    if (delta.generationFrom != SceneRevision(1) ||
        delta.generationTo != SceneRevision(2) || delta.updatedRecords.size() != 1 ||
        delta.orderChanges.size() != 1 || !delta.addedRecords.empty() ||
        !delta.removedHandles.empty()) {
        std::cerr << "SceneDelta mapping failed\n";
        return EXIT_FAILURE;
    }
    SceneRecordStore store;
    const std::vector<SceneRecord> initial{record(1, 1), record(2, 2)};
    auto replacement = store.prepareReplace(initial);
    if (!replacement) return EXIT_FAILURE;
    store.commit(std::move(replacement.value()));
    const RecordHandle stableHandle = store.handleFor(before.objectId);
    SceneRecordStore::LocalityDiagnostics beforeDiag = store.localityDiagnostics();
    auto local = store.prepareApply(input.mutations);
    if (!local) return EXIT_FAILURE;
    store.commit(std::move(local.value()));
    if (stableHandle == 0 || store.handleFor(before.objectId) != stableHandle ||
        store.localityDiagnostics().localizedMutationCount != beforeDiag.localizedMutationCount + 1 ||
        store.records().front().orderKey != SceneOrderKey(2)) {
        std::cerr << "Stable slot locality failed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
