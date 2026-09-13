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
    const auto afterDiag = store.localityDiagnostics();
    if (stableHandle == 0 || store.handleFor(before.objectId) != stableHandle ||
        store.localityDiagnostics().localizedMutationCount != beforeDiag.localizedMutationCount + 1 ||
        afterDiag.fullRecordCloneCount != beforeDiag.fullRecordCloneCount ||
        afterDiag.fullSortCount != beforeDiag.fullSortCount ||
        afterDiag.fullReindexCount != beforeDiag.fullReindexCount ||
        afterDiag.fullRebuildCount != beforeDiag.fullRebuildCount ||
        store.records().front().orderKey != SceneOrderKey(2)) {
        std::cerr << "Stable slot locality failed\n";
        return EXIT_FAILURE;
    }

    auto runSequence = [](std::size_t population, bool readBetween) {
        SceneRecordStore local;
        std::vector<SceneRecord> seed;
        seed.reserve(population);
        for (std::size_t i = 0; i < population; ++i) {
            seed.push_back(record(static_cast<std::uint64_t>(i + 10), i + 10));
        }
        auto initial = local.prepareReplace(seed);
        if (!initial) return std::pair<std::uint64_t, std::size_t>{0, 0};
        local.commit(std::move(initial.value()));
        const auto baseline = local.localityDiagnostics();
        const SceneRecord inserted = record(1, population + 100);
        CompiledSceneDelta add{SceneRevision(1), SceneRevision(2),
                               {SceneMutation{SceneMutationKind::kInsert, inserted.objectId,
                                               std::nullopt, inserted}}, std::nullopt};
        auto addPrepared = local.prepareApply(add.mutations);
        if (!addPrepared) return std::pair<std::uint64_t, std::size_t>{0, 0};
        local.commit(std::move(addPrepared.value()));
        if (readBetween) static_cast<void>(local.materializeSnapshot());
        SceneRecord changed = inserted;
        changed.contentRevision = ContentRevision(2);
        CompiledSceneDelta update{SceneRevision(2), SceneRevision(3),
                                  {SceneMutation{SceneMutationKind::kUpdate, inserted.objectId,
                                                  inserted, changed}}, std::nullopt};
        auto updatePrepared = local.prepareApply(update.mutations);
        if (!updatePrepared) return std::pair<std::uint64_t, std::size_t>{0, 0};
        local.commit(std::move(updatePrepared.value()));
        if (readBetween) static_cast<void>(local.materializeSnapshot());
        CompiledSceneDelta remove{SceneRevision(3), SceneRevision(4),
                                  {SceneMutation{SceneMutationKind::kRemove, inserted.objectId,
                                                  changed, std::nullopt}}, std::nullopt};
        auto removePrepared = local.prepareApply(remove.mutations);
        if (!removePrepared) return std::pair<std::uint64_t, std::size_t>{0, 0};
        local.commit(std::move(removePrepared.value()));
        const auto diagnostics = local.localityDiagnostics();
        if (diagnostics.localizedMutationCount != baseline.localizedMutationCount + 3 ||
            diagnostics.fullRecordCloneCount != baseline.fullRecordCloneCount ||
            diagnostics.fullSortCount != baseline.fullSortCount ||
            diagnostics.fullReindexCount != baseline.fullReindexCount ||
            diagnostics.fullRebuildCount != baseline.fullRebuildCount) {
            return std::pair<std::uint64_t, std::size_t>{0, 0};
        }
        return std::pair<std::uint64_t, std::size_t>{local.handleFor(inserted.objectId),
                                                     local.records().size()};
    };
    const auto noRead = runSequence(1000, false);
    const auto withRead = runSequence(1000, true);
    if (noRead != withRead || noRead.first != 0) {
        std::cerr << "snapshot-independent mutation regression failed\n";
        return EXIT_FAILURE;
    }
    for (std::size_t population : {1000U, 10000U, 100000U}) {
        const auto result = runSequence(population, false);
        if (result.first != 0 || result.second != population) {
            std::cerr << "locality matrix failed at " << population << "\n";
            return EXIT_FAILURE;
        }
        std::cout << "locality population=" << population << " localized_mutations=3\n";
    }
    return EXIT_SUCCESS;
}
