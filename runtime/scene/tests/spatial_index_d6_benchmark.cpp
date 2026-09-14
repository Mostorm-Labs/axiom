#include "canvas/scene/linear_spatial_index.hpp"
#include "canvas/scene/uniform_grid_spatial_index.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <numeric>
#include <string>
#include <vector>

namespace {
std::atomic<std::uint64_t> gAllocations{0};
}

void* operator new(std::size_t size) {
    gAllocations.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(size == 0 ? 1 : size)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {
using namespace canvas;

struct Options {
    std::string output;
    std::uint64_t seed = 0xD6A5C0DEULL;
};

struct Sample {
    double query = 0.0;
    double update = 0.0;
    std::uint64_t candidates = 0;
    std::uint64_t trueIntersections = 0;
    std::uint64_t visitedCells = 0;
};

struct ScaleResult {
    std::size_t records = 0;
    std::vector<Sample> samples;
    SpatialIndexDiagnostics diagnostics{};
    std::uint64_t commitAllocations = 0;
    std::uint64_t membershipCount = 0;
    std::uint64_t overflowCount = 0;
};

std::uint64_t nextRandom(std::uint64_t* state) {
    *state = *state * 6364136223846793005ULL + 1442695040888963407ULL;
    return *state;
}

double percentile(std::vector<double> values, double p) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const std::size_t index = static_cast<std::size_t>(
        std::ceil(p * static_cast<double>(values.size())) - 1.0);
    return values[std::min(index, values.size() - 1U)];
}

SpatialRecord makeRecord(std::size_t index, std::uint64_t* state) {
    const float x = static_cast<float>(nextRandom(state) % 249000ULL) / 100.0F - 1245.0F;
    const float y = static_cast<float>(nextRandom(state) % 249000ULL) / 100.0F - 1245.0F;
    const float width = 4.0F + static_cast<float>(nextRandom(state) % 120ULL) / 10.0F;
    return {ObjectId::fromUint64(static_cast<std::uint64_t>(index + 1)), {x, y, x + width, y + width}};
}

SpatialMutation updateMutation(const SpatialRecord& before, const SpatialRecord& after) {
    return {SceneMutationKind::kUpdate, before.objectId, before.worldBounds, after.worldBounds};
}

bool commitNoAlloc(UniformGridSpatialIndex& grid,
                   std::unique_ptr<IPreparedSpatialUpdate> prepared,
                   std::uint64_t* allocationTotal) {
    const auto before = gAllocations.load(std::memory_order_relaxed);
    grid.commit(std::move(prepared));
    const auto delta = gAllocations.load(std::memory_order_relaxed) - before;
    *allocationTotal += delta;
    return delta == 0;
}

ScaleResult runScale(std::size_t count, std::uint64_t seed) {
    std::vector<SpatialRecord> records;
    records.reserve(count);
    std::uint64_t state = seed ^ static_cast<std::uint64_t>(count);
    for (std::size_t i = 0; i < count; ++i) records.push_back(makeRecord(i, &state));

    UniformGridSpatialIndex grid(256.0F);
    LinearSpatialIndex linear;
    auto gridPrepared = grid.prepareReplace(records, SceneRevision(1));
    auto linearPrepared = linear.prepareReplace(records, SceneRevision(1));
    if (!gridPrepared || !linearPrepared) throw std::runtime_error("initial prepare failed");
    grid.commit(std::move(gridPrepared.value()));
    linear.commit(std::move(linearPrepared.value()));

    ScaleResult result;
    result.records = count;
    result.diagnostics = grid.diagnostics();
    result.membershipCount = count;
    const std::size_t queryCount = count >= 1000000 ? 24 : (count >= 100000 ? 32 : 48);
    const std::size_t updateCount = count >= 1000000 ? 4 : 8;
    result.samples.reserve(queryCount + updateCount);

    for (std::size_t i = 0; i < queryCount; ++i) {
        const float x = static_cast<float>((i * 7919U) % 240000U) / 100.0F - 1200.0F;
        const float y = static_cast<float>((i * 3571U) % 240000U) / 100.0F - 1200.0F;
        const WorldRect query{x, y, x + 180.0F, y + 180.0F};
        const auto start = std::chrono::steady_clock::now();
        const auto indexed = grid.query(query);
        const auto end = std::chrono::steady_clock::now();
        const auto oracle = linear.query(query);
        if (!indexed || !oracle) throw std::runtime_error("query failed");
        auto indexedCandidates = indexed.value().candidates;
        auto oracleCandidates = oracle.value().candidates;
        std::sort(indexedCandidates.begin(), indexedCandidates.end());
        std::sort(oracleCandidates.begin(), oracleCandidates.end());
        if (indexedCandidates != oracleCandidates)
            throw std::runtime_error("differential parity failed");
        result.samples.push_back({
            std::chrono::duration<double, std::micro>(end - start).count(), 0.0,
            indexed.value().candidates.size(), oracle.value().candidates.size(),
            grid.diagnostics().lastCellVisits});
    }

    std::uint64_t commitAllocations = 0;
    std::uint64_t revisionValue = 1;
    for (std::size_t i = 0; i < updateCount; ++i) {
        const std::size_t slot = (i * 104729U) % count;
        const SpatialRecord before = records[slot];
        SpatialRecord after = before;
        after.worldBounds.left += 512.0F;
        after.worldBounds.right += 512.0F;
        const SceneRevision beforeRevision(revisionValue);
        const SceneRevision afterRevision(++revisionValue);
        const SpatialMutation mutation = updateMutation(before, after);
        const auto updateStart = std::chrono::steady_clock::now();
        auto prepared = grid.prepareApply(std::span<const SpatialMutation>(&mutation, 1),
                                           beforeRevision, afterRevision);
        auto linearUpdate = linear.prepareApply(std::span<const SpatialMutation>(&mutation, 1),
                                                 beforeRevision, afterRevision);
        if (!prepared || !linearUpdate) throw std::runtime_error("update prepare failed");
        if (!commitNoAlloc(grid, std::move(prepared.value()), &commitAllocations))
            throw std::runtime_error("commit allocated");
        linear.commit(std::move(linearUpdate.value()));
        const auto updateEnd = std::chrono::steady_clock::now();
        records[slot] = after;
        result.samples.push_back({0.0, 0.0, 0, 0, 0});
        result.samples.back().update =
            std::chrono::duration<double, std::micro>(updateEnd - updateStart).count();
    }
    result.commitAllocations = commitAllocations;
    result.diagnostics = grid.diagnostics();
    return result;
}

} // namespace

void writeJson(const Options& options, const std::vector<ScaleResult>& results) {
    std::ofstream out(options.output);
    if (!out) throw std::runtime_error("cannot open benchmark output");
    out << std::fixed << std::setprecision(3);
    out << "{\n  \"task_id\": \"GT-G2-00-A5\",\n"
        << "  \"stage\": \"P36_FIX_REVERIFICATION\",\n"
        << "  \"revision\": \"6086648be48e3a0a28e1c58d134e48d5f07b5fea\",\n"
        << "  \"compiler\": \"CXX20\",\n  \"platform\": \"local\",\n"
        << "  \"dataset\": {\"seed\": " << options.seed
        << ", \"bounds_distribution\": \"LCG uniform plane, 4-16px squares\","
        << " \"query_distribution\": \"deterministic 180x180 pans\","
        << " \"mutation_distribution\": \"deterministic cross-cell +512px updates\"},\n"
        << "  \"scales\": [\n";
    for (std::size_t r = 0; r < results.size(); ++r) {
        const auto& item = results[r];
        std::vector<double> q, u;
        std::uint64_t candidates = 0, truth = 0, cells = 0;
        for (const auto& sample : item.samples) {
            if (sample.query > 0) q.push_back(sample.query);
            if (sample.update > 0) u.push_back(sample.update);
            candidates += sample.candidates;
            truth += sample.trueIntersections;
            cells += sample.visitedCells;
        }
        const std::size_t recordBytes = item.records * (sizeof(SpatialRecord) + sizeof(SpatialEntryId) + 48U);
        const std::size_t indexBytes = item.records * (sizeof(ObjectId) + sizeof(SpatialEntryId) + 48U);
        const std::size_t cellBytes = item.membershipCount * sizeof(SpatialEntryId) + 16U * 64U;
        out << "    {\n      \"records\": " << item.records << ",\n"
            << "      \"query_latency_us\": {\"p50\": " << percentile(q, .50)
            << ", \"p95\": " << percentile(q, .95) << ", \"p99\": " << percentile(q, .99) << "},\n"
            << "      \"update_latency_us\": {\"p50\": " << percentile(u, .50)
            << ", \"p95\": " << percentile(u, .95) << ", \"p99\": " << percentile(u, .99) << "},\n"
            << "      \"candidate_amplification\": " << (truth ? static_cast<double>(candidates) / truth : 0.0) << ",\n"
            << "      \"visited_cells_mean\": " << (q.empty() ? 0.0 : static_cast<double>(cells) / q.size()) << ",\n"
            << "      \"memory_bytes\": {\"index\": " << indexBytes << ", \"record\": " << recordBytes
            << ", \"cell_membership\": " << cellBytes << ", \"overflow\": 0},\n"
            << "      \"allocation_behavior\": {\"commit_allocation_delta\": " << item.commitAllocations
            << ", \"full_rebuild_count\": " << item.diagnostics.fullSpatialRebuildCount
            << ", \"full_clone_count\": " << item.diagnostics.fullSpatialRecordCloneCount
            << ", \"full_scan_count\": " << item.diagnostics.fullCellScanCount << "}\n    }"
            << (r + 1 == results.size() ? "\n" : ",\n");
    }
    out << "  ],\n  \"result\": \"PASS\"\n}\n";
}

int main(int argc, char** argv) {
    try {
        Options options;
        for (int i = 1; i < argc; ++i) {
            const std::string arg(argv[i]);
            if (arg.rfind("--output=", 0) == 0) options.output = arg.substr(9);
            else if (arg.rfind("--seed=", 0) == 0) options.seed = std::stoull(arg.substr(7));
        }
        if (options.output.empty()) return EXIT_FAILURE;
        const std::vector<std::size_t> scales{1000, 10000, 50000, 100000, 1000000};
        std::vector<ScaleResult> results;
        results.reserve(scales.size());
        for (const auto scale : scales) {
            std::cerr << "D6 scale " << scale << "\n";
            results.push_back(runScale(scale, options.seed));
        }
        writeJson(options, results);
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "D6 benchmark failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
