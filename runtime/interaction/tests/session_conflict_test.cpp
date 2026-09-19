#include "canvas/interaction/interaction_dependency_footprint.hpp"
#include "canvas/interaction/session_manager.hpp"
#include <cassert>
using namespace canvas;
namespace {
foundation::ObjectId id(std::uint64_t n) { return foundation::ObjectId::fromUint64(n); }
struct T final : interaction::TransientPresentationPort { int cancels=0; void cancel(std::uint64_t) noexcept override {++cancels;} };
void decisions_use_footprint_not_generation_alone() {
    interaction::InteractionDependencyFootprint f;
    assert(f.track(id(1)));
    auto generationOnly = semantic::ChangeSet::fromChanges(semantic::SemanticGeneration(1), semantic::SemanticGeneration(2), {});
    assert(f.evaluate(generationOnly) == interaction::ConflictDecision::kContinue);
    auto unrelated = semantic::ChangeSet::fromChanges(semantic::SemanticGeneration(2), semantic::SemanticGeneration(3), {{id(2), semantic::SemanticChangeFlags::kTransform, {}}});
    assert(f.evaluate(unrelated) == interaction::ConflictDecision::kContinue);
    auto target = semantic::ChangeSet::fromChanges(semantic::SemanticGeneration(3), semantic::SemanticGeneration(4), {{id(1), semantic::SemanticChangeFlags::kTransform, {}}});
    assert(f.evaluate(target) == interaction::ConflictDecision::kReResolve);
    interaction::CancellationReason reason=interaction::CancellationReason::kNone;
    auto deleted = semantic::ChangeSet::fromChanges(semantic::SemanticGeneration(4), semantic::SemanticGeneration(5), {{id(1), semantic::SemanticChangeFlags::kDeleted, {}}});
    assert(f.evaluate(deleted,&reason) == interaction::ConflictDecision::kCancel);
    assert(reason == interaction::CancellationReason::kTargetDeleted);
}
void manager_rejects_stale_callbacks_after_lifecycle_cancel() {
    T t; interaction::InteractionSessionManager m(t); assert(m.start(9)); assert(m.trackDependency(9,id(1))); assert(m.sourceLost(9)); assert(m.activeCount()==0); assert(t.cancels==1);
    interaction::ConflictDecision d=interaction::ConflictDecision::kContinue;
    auto change=semantic::ChangeSet::fromChanges(semantic::SemanticGeneration(1), semantic::SemanticGeneration(2), {{id(1),semantic::SemanticChangeFlags::kTransform,{}}});
    assert(!m.onChangeSet(9,change,&d)); assert(!m.finish(9)); assert(!m.trackDependency(9,id(2)));
}
}
int main(){decisions_use_footprint_not_generation_alone(); manager_rejects_stale_callbacks_after_lifecycle_cancel();}
