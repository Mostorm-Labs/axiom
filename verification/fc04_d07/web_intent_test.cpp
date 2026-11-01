// Browser-executed WASM check of the real Runtime, Semantic, Scene and History owners.
#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include <array>
#include <cstdio>

using namespace canvas;
namespace {
foundation::ObjectId id(std::uint64_t value) { return foundation::ObjectId::fromUint64(value); }
semantic::ObjectRecord shape(std::uint64_t value) {
    semantic::ObjectRecord r;
    r.id = id(value); r.kind = semantic::ObjectKind::kShape; r.kind_version = 1;
    r.placement.order_key = semantic::OrderKey({static_cast<std::uint8_t>(value)});
    r.content = semantic::ShapeContent{1, 20, 20}; return r;
}
semantic::Operation op(std::uint64_t value, semantic::OperationPayload payload) {
    return {semantic::OperationId{id(value)}, semantic::DocumentId{id(1)}, 1, 1, std::move(payload)};
}
bool run() {
    runtime::RuntimeCompositionCore composition;
    if (!composition.open() || !composition.bindSurface(64, 64)) return false;
    auto& runtime = composition.qualificationRuntime();
    const auto seed = op(100, semantic::InsertObjectsOp{{shape(1)}});
    if (composition.applyExternalOperation(seed, semantic::ApplySource::kRemoteSync).disposition !=
        semantic::ApplyDisposition::kApplied) return false;
    const auto inserted = shape(2);
    const std::array operations{
        op(101, semantic::InsertObjectsOp{{inserted}}),
        op(102, semantic::SetTransformsOp{{{id(2), {1, 0, 0, 1, 200, 30}}}})};
    if (!runtime.beginExplicitIntent() || !runtime.prepareExplicitIntent(operations) ||
        !runtime.commitExplicitIntent()) return false;
    auto* record = runtime.qualificationObjectRecord(id(2));
    if (!record || record->transform.tx != 200 || runtime.semanticObjectCount() != 2) return false;
    if (!runtime.undo() || runtime.qualificationObjectRecord(id(2))) return false;
    if (!runtime.redo()) return false;
    record = runtime.qualificationObjectRecord(id(2));
    if (!record || record->transform.tx != 200) return false;
    return composition.close();
}
}
int main() {
    const bool passed = run();
    std::printf("FC04_D07_REAL_WASM_INTENT=%s\n", passed ? "PASS" : "FAIL");
    return passed ? 0 : 1;
}
