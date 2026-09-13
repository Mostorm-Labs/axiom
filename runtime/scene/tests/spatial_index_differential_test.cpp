#include "canvas/scene/linear_spatial_index.hpp"
#include "canvas/scene/spatial_delta.hpp"
#include "canvas/scene/uniform_grid_spatial_index.hpp"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#include <malloc.h>
#endif

namespace {
std::atomic<std::uint64_t> gOrdinaryAllocations{0};

void* allocateAligned(std::size_t size, std::size_t alignment) noexcept {
#if defined(_WIN32)
    return _aligned_malloc(size == 0U ? 1U : size, alignment);
#else
    void* memory = nullptr;
    if (posix_memalign(&memory, alignment, size == 0U ? 1U : size) != 0) {
        return nullptr;
    }
    return memory;
#endif
}

void freeAligned(void* memory) noexcept {
#if defined(_WIN32)
    _aligned_free(memory);
#else
    std::free(memory);
#endif
}
} // namespace

void* operator new(std::size_t size) {
    gOrdinaryAllocations.fetch_add(1U, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0U ? 1U : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    gOrdinaryAllocations.fetch_add(1U, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0U ? 1U : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

void* operator new(std::size_t size, std::align_val_t alignment) {
    gOrdinaryAllocations.fetch_add(1U, std::memory_order_relaxed);
    if (void* memory = allocateAligned(size, static_cast<std::size_t>(alignment))) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    return ::operator new(size, alignment);
}

void operator delete(void* memory, std::align_val_t) noexcept { freeAligned(memory); }
void operator delete[](void* memory, std::align_val_t) noexcept { freeAligned(memory); }
void operator delete(void* memory, std::size_t, std::align_val_t) noexcept { freeAligned(memory); }
void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept {
    freeAligned(memory);
}

namespace {
using namespace canvas;

SpatialRecord record(std::uint64_t id, WorldRect bounds) {
    return {ObjectId::fromUint64(id), bounds};
}

SpatialMutation insert(const SpatialRecord& value) {
    return {SceneMutationKind::kInsert, value.objectId, std::nullopt, value.worldBounds};
}

SpatialMutation update(const SpatialRecord& before, const SpatialRecord& after) {
    return {SceneMutationKind::kUpdate, before.objectId, before.worldBounds, after.worldBounds};
}

SpatialMutation remove(const SpatialRecord& value) {
    return {SceneMutationKind::kRemove, value.objectId, value.worldBounds, std::nullopt};
}

SpatialDelta delta(SceneRevision before,
                   SceneRevision after,
                   std::initializer_list<SpatialMutation> mutations) {
    return SpatialDelta{before, after, std::vector<SpatialMutation>(mutations)};
}

bool seed(UniformGridSpatialIndex& grid,
          LinearSpatialIndex& linear,
          std::span<const SpatialRecord> records,
          SceneRevision revision) {
    auto gridPrepared = grid.prepareReplace(records, revision);
    auto linearPrepared = linear.prepareReplace(records, revision);
    if (!gridPrepared || !linearPrepared) {
        std::cerr << "seed prepare failed\n";
        return false;
    }
    grid.commit(std::move(gridPrepared.value()));
    linear.commit(std::move(linearPrepared.value()));
    return true;
}

bool commitWithoutOrdinaryAllocation(UniformGridSpatialIndex& grid,
                                     std::unique_ptr<IPreparedSpatialUpdate> prepared,
                                     std::string_view label) {
    const std::uint64_t before = gOrdinaryAllocations.load(std::memory_order_relaxed);
    grid.commit(std::move(prepared));
    const std::uint64_t after = gOrdinaryAllocations.load(std::memory_order_relaxed);
    if (after != before) {
        std::cerr << label << " allocated during commit: " << (after - before) << "\n";
        return false;
    }
    return true;
}

bool applyBoth(UniformGridSpatialIndex& grid,
               LinearSpatialIndex& linear,
               const SpatialDelta& change,
               std::string_view label) {
    auto gridPrepared =
        grid.prepareSpatialDelta(change, change.generationFrom, change.generationTo);
    auto linearPrepared =
        linear.prepareSpatialDelta(change, change.generationFrom, change.generationTo);
    if (!gridPrepared || !linearPrepared) {
        std::cerr << label << " prepare failed\n";
        return false;
    }
    if (!commitWithoutOrdinaryAllocation(grid, std::move(gridPrepared.value()), label)) {
        return false;
    }
    linear.commit(std::move(linearPrepared.value()));
    return true;
}

std::vector<ObjectId> normalized(std::vector<ObjectId> values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    return values;
}

bool expectParity(const UniformGridSpatialIndex& grid,
                  const LinearSpatialIndex& linear,
                  const WorldRect& query,
                  std::initializer_list<ObjectId> expected,
                  std::string_view label) {
    const auto gridResult = grid.query(query);
    const auto linearResult = linear.query(query);
    if (!gridResult || !linearResult) {
        std::cerr << label << " query failed\n";
        return false;
    }
    const auto gridSet = normalized(gridResult.value().candidates);
    const auto linearSet = normalized(linearResult.value().candidates);
    const auto expectedSet = normalized(std::vector<ObjectId>(expected));
    if (gridSet != linearSet || gridSet != expectedSet) {
        std::cerr << label << " parity mismatch\n";
        return false;
    }
    return true;
}

bool d1LocalizedPreparedPublication() {
    UniformGridSpatialIndex grid(1.0F);
    LinearSpatialIndex linear;

    SpatialRecord first = record(1, {0, 0, 1, 1});
    const SpatialRecord sentinel = record(2, {100, 100, 101, 101});
    const std::vector<SpatialRecord> initial{first, sentinel};
    if (!seed(grid, linear, initial, SceneRevision(1))) {
        return false;
    }

    const SpatialRecord inserted = record(3, {10, 10, 11, 11});
    if (!applyBoth(grid, linear,
                   delta(SceneRevision(1), SceneRevision(2), {insert(inserted)}),
                   "localized insert / absent cell")) {
        return false;
    }
    if (!expectParity(grid, linear, {9, 9, 12, 12}, {inserted.objectId},
                      "localized insert lookup publication")) {
        return false;
    }

    SpatialRecord moved = first;
    moved.worldBounds = {2, 0, 3, 1};
    if (!applyBoth(grid, linear,
                   delta(SceneRevision(2), SceneRevision(3), {update(first, moved)}),
                   "cross-cell update")) {
        return false;
    }
    first = moved;
    if (!expectParity(grid, linear, {0, 0, 4, 2}, {first.objectId},
                      "cross-cell update parity")) {
        return false;
    }

    const auto beforeSameCoverage = grid.diagnostics();
    SpatialRecord sameCoverage = first;
    sameCoverage.worldBounds = {2.1F, 0.1F, 2.9F, 0.9F};
    if (!applyBoth(grid, linear,
                   delta(SceneRevision(3), SceneRevision(4), {update(first, sameCoverage)}),
                   "same coverage bounds update")) {
        return false;
    }
    first = sameCoverage;
    const auto afterSameCoverage = grid.diagnostics();
    if (afterSameCoverage.membershipRemovalCount != beforeSameCoverage.membershipRemovalCount ||
        afterSameCoverage.membershipAddCount != beforeSameCoverage.membershipAddCount) {
        std::cerr << "same coverage update churned membership\n";
        return false;
    }

    const auto beforeUnchanged = grid.diagnostics();
    if (!applyBoth(grid, linear,
                   delta(SceneRevision(4), SceneRevision(5), {update(first, first)}),
                   "unchanged bounds update")) {
        return false;
    }
    const auto afterUnchanged = grid.diagnostics();
    if (afterUnchanged.membershipRemovalCount != beforeUnchanged.membershipRemovalCount ||
        afterUnchanged.membershipAddCount != beforeUnchanged.membershipAddCount) {
        std::cerr << "unchanged bounds update churned membership\n";
        return false;
    }

    if (!applyBoth(grid, linear,
                   delta(SceneRevision(5), SceneRevision(6), {remove(inserted)}),
                   "localized remove")) {
        return false;
    }
    if (!expectParity(grid, linear, {9, 9, 12, 12}, {}, "localized remove parity")) {
        return false;
    }

    const SpatialRecord overflow =
        record(4, {-262144.0F, -262144.0F, 262144.0F, 262144.0F});
    if (!applyBoth(grid, linear,
                   delta(SceneRevision(6), SceneRevision(7), {insert(overflow)}),
                   "overflow insert")) {
        return false;
    }
    if (!expectParity(grid, linear, {-10, -10, 10, 10}, {first.objectId, overflow.objectId},
                      "overflow insert parity")) {
        return false;
    }
    if (!applyBoth(grid, linear,
                   delta(SceneRevision(7), SceneRevision(8), {remove(overflow)}),
                   "overflow remove")) {
        return false;
    }

    const auto beforeFailure = grid.diagnostics();
    const auto sentinelBefore = grid.query({99, 99, 102, 102});
    SpatialRecord wrongBefore = first;
    wrongBefore.worldBounds = {50, 50, 51, 51};
    const SpatialRecord rejectedAfter = record(1, {4, 0, 5, 1});
    const auto rejected = grid.prepareSpatialDelta(
        delta(SceneRevision(8), SceneRevision(9), {update(wrongBefore, rejectedAfter)}),
        SceneRevision(8), SceneRevision(9));
    const auto afterFailure = grid.diagnostics();
    const auto sentinelAfter = grid.query({99, 99, 102, 102});
    if (rejected || !sentinelBefore || !sentinelAfter ||
        beforeFailure.revision != afterFailure.revision ||
        beforeFailure.localizedMutationCount != afterFailure.localizedMutationCount ||
        beforeFailure.affectedEntryCount != afterFailure.affectedEntryCount ||
        beforeFailure.membershipRemovalCount != afterFailure.membershipRemovalCount ||
        beforeFailure.membershipAddCount != afterFailure.membershipAddCount ||
        normalized(sentinelBefore.value().candidates) != normalized(sentinelAfter.value().candidates)) {
        std::cerr << "failed prepare changed published state or committed diagnostics\n";
        return false;
    }

    const auto diagnostics = grid.diagnostics();
    if (diagnostics.fullSpatialRebuildCount != 0U ||
        diagnostics.fullSpatialRecordCloneCount != 0U ||
        diagnostics.fullCellScanCount != 0U) {
        std::cerr << "localized D1 path performed full work\n";
        return false;
    }
    return true;
}

bool d5HugeFixture() {
    UniformGridSpatialIndex grid;
    LinearSpatialIndex linear;
    const std::vector<SpatialRecord> initial;
    if (!seed(grid, linear, initial, SceneRevision(100))) {
        return false;
    }

    const ObjectId hugeId = ObjectId::fromUint64(0xA500000000000001ULL);
    const SpatialRecord b0 = record(0xA500000000000001ULL,
                                    {-262144.0F, -262144.0F, 262144.0F, 262144.0F});
    const SpatialRecord b1 = record(0xA500000000000001ULL,
                                    {-196608.0F, -262144.0F, 327680.0F, 262144.0F});
    const WorldRect qCenter{-128, -128, 128, 128};
    const WorldRect qOldOnly{-262000, -64, -261744, 64};
    const WorldRect qNewOnly{327424, -64, 327600, 64};
    const WorldRect qOutside{393216, -128, 393472, 128};

    if (!applyBoth(grid, linear,
                   delta(SceneRevision(100), SceneRevision(101), {insert(b0)}),
                   "HUGE-01 insert")) {
        return false;
    }
    if (!expectParity(grid, linear, qCenter, {hugeId}, "HUGE-01 insert center") ||
        !expectParity(grid, linear, qOldOnly, {hugeId}, "HUGE-01 insert old-only") ||
        !expectParity(grid, linear, qNewOnly, {}, "HUGE-01 insert new-only") ||
        !expectParity(grid, linear, qOutside, {}, "HUGE-01 insert outside")) {
        return false;
    }

    if (!applyBoth(grid, linear,
                   delta(SceneRevision(101), SceneRevision(102), {update(b0, b1)}),
                   "HUGE-01 update")) {
        return false;
    }
    if (!expectParity(grid, linear, qCenter, {hugeId}, "HUGE-01 update center") ||
        !expectParity(grid, linear, qOldOnly, {}, "HUGE-01 update old-only") ||
        !expectParity(grid, linear, qNewOnly, {hugeId}, "HUGE-01 update new-only") ||
        !expectParity(grid, linear, qOutside, {}, "HUGE-01 update outside")) {
        return false;
    }

    if (!applyBoth(grid, linear,
                   delta(SceneRevision(102), SceneRevision(103), {remove(b1)}),
                   "HUGE-01 remove")) {
        return false;
    }
    return expectParity(grid, linear, qCenter, {}, "HUGE-01 remove center") &&
           expectParity(grid, linear, qOldOnly, {}, "HUGE-01 remove old-only") &&
           expectParity(grid, linear, qNewOnly, {}, "HUGE-01 remove new-only") &&
           expectParity(grid, linear, qOutside, {}, "HUGE-01 remove outside");
}

bool d5ExtremeFailClosed() {
    UniformGridSpatialIndex grid;
    LinearSpatialIndex linear;
    const SpatialRecord sentinel =
        record(0xA5000000000000F2ULL, {-64.0F, -64.0F, 64.0F, 64.0F});
    const std::vector<SpatialRecord> initial{sentinel};
    if (!seed(grid, linear, initial, SceneRevision(200))) {
        return false;
    }

    const SpatialRecord probe = record(0xA5000000000000F1ULL,
                                       {549755813888.0F, 0.0F,
                                        549755879424.0F, 65536.0F});
    const SpatialDelta probeDelta =
        delta(SceneRevision(200), SceneRevision(201), {insert(probe)});
    const auto before = grid.diagnostics();
    const auto sentinelBefore = grid.query({-128, -128, 128, 128});
    const auto prepared =
        grid.prepareSpatialDelta(probeDelta, SceneRevision(200), SceneRevision(201));
    const auto after = grid.diagnostics();
    const auto sentinelAfter = grid.query({-128, -128, 128, 128});
    const auto repeated =
        grid.prepareSpatialDelta(probeDelta, SceneRevision(200), SceneRevision(201));
    if (prepared || repeated ||
        prepared.error().code != foundation::ErrorCode::kInvalidArgument ||
        repeated.error().code != foundation::ErrorCode::kInvalidArgument ||
        !sentinelBefore || !sentinelAfter ||
        before.revision != SceneRevision(200) || after.revision != SceneRevision(200) ||
        before.localizedMutationCount != after.localizedMutationCount ||
        before.affectedEntryCount != after.affectedEntryCount ||
        before.membershipRemovalCount != after.membershipRemovalCount ||
        before.membershipAddCount != after.membershipAddCount ||
        normalized(sentinelBefore.value().candidates) != std::vector<ObjectId>{sentinel.objectId} ||
        normalized(sentinelBefore.value().candidates) != normalized(sentinelAfter.value().candidates)) {
        std::cerr << "EXTREME-01 did not reject atomically\n";
        return false;
    }
    return true;
}

} // namespace

int main() {
    if (!d1LocalizedPreparedPublication()) {
        return EXIT_FAILURE;
    }
    if (!d5HugeFixture()) {
        return EXIT_FAILURE;
    }
    if (!d5ExtremeFailClosed()) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
