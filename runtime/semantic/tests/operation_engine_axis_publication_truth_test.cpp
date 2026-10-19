#include "canvas/render/presentation_tracker.hpp"
#include "canvas/render/surface_lifecycle.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/commit_publication_projection.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/reference_object_store.hpp"

#include "object_store_mutator.hpp"

#include <cassert>
#include <cstdint>
#include <optional>
#include <vector>

namespace {
using namespace canvas;
using namespace canvas::semantic;

struct AxisRecorder final {
  enum class Durability : std::uint8_t { kPending, kFailed, kDurable };
  struct Event final { ApplySource source; OperationId operation; };
  std::vector<Event> events;
  Durability durability = Durability::kPending;
  std::uint64_t frontier = 0;

  void publish(const CanonicalCommitRecord& record) {
    if (localBridgePublicationDisposition(record) != LocalBridgePublicationDisposition::kEligible) return;
    events.push_back({record.source, record.operation_id});
    durability = Durability::kPending;
  }
  void fail() { durability = Durability::kFailed; }
  void acknowledge(std::uint64_t ordinal) { frontier = ordinal; durability = Durability::kDurable; }
};

ObjectRecord shape(std::uint64_t idValue) {
  ObjectRecord record{};
  record.id = ObjectId::fromUint64(idValue);
  record.kind = ObjectKind::kShape;
  record.kind_version = 1U;
  record.placement.order_key = OrderKey({1U, static_cast<std::uint8_t>(idValue)});
  record.content = ShapeContent{1U, 20.0, 20.0};
  return record;
}

Operation insert(std::uint64_t operationValue, std::uint64_t objectValue) {
  return Operation{OperationId(ObjectId::fromUint64(operationValue)), DocumentId(ObjectId::fromUint64(10U)),
                   1U, 1U, InsertObjectsOp{{shape(objectValue)}}};
}

template <typename Store>
void local_remote_and_duplicate_paths_are_recorded_once() {
  Store objects;
  AppliedOperationLedger ledger;
  SemanticGenerationState generation;
  CanonicalCommitClock clock(RuntimeEpoch(9U));
  AxisRecorder axis;
  const auto local = OperationEngine{}.apply(insert(1U, 1U), ApplySource::kLocalInteraction,
                                              objects, ledger, generation, clock);
  assert(local.disposition == ApplyDisposition::kApplied && local.commit_record.has_value());
  axis.publish(*local.commit_record);
  assert(axis.events.size() == 1U && axis.events.front().source == ApplySource::kLocalInteraction);

  const auto duplicate = OperationEngine{}.apply(insert(1U, 1U), ApplySource::kRemoteSync,
                                                  objects, ledger, generation, clock);
  assert(duplicate.disposition == ApplyDisposition::kAlreadyApplied && !duplicate.commit_record.has_value());
  assert(axis.events.size() == 1U);

  const auto remote = OperationEngine{}.apply(insert(2U, 2U), ApplySource::kRemoteSync,
                                               objects, ledger, generation, clock);
  assert(remote.disposition == ApplyDisposition::kApplied && remote.commit_record.has_value());
  axis.publish(*remote.commit_record);
  assert(axis.events.size() == 1U);

  axis.fail();
  assert(axis.durability == AxisRecorder::Durability::kFailed);
  axis.acknowledge(local.commit_record->after_generation.value());
  assert(axis.durability == AxisRecorder::Durability::kDurable);
  assert(axis.frontier == local.commit_record->after_generation.value());
  if constexpr (std::is_same_v<Store, IndexedObjectStore>) assert(internal::ObjectStoreMutator::indexMatchesRebuild(objects));
}

void presented_feedback_is_qualified_and_generation_fenced() {
  using namespace render;
  const SurfaceMetrics metrics{128.0F, 128.0F, 128U, 128U, 1.0F, 1.0F};
  SurfaceLifecycle lifecycle{SurfaceSnapshot{ViewId{1U}, SurfaceGeneration{1U}, MetricsGeneration{1U}, metrics}};
  PresentationTracker tracker{lifecycle};
  const FrameState frame{ViewId{1U}, CameraState{foundation::WorldPoint{0, 0}, 1.0F, 0.37F, CameraGeneration{1U}},
                         foundation::WorldRect{-10, -10, 10, 10}, metrics, SemanticGeneration{1U},
                         foundation::SceneRevision{1U}, SurfaceGeneration{1U}, MetricsGeneration{1U}, FrameId{1U}};
  assert(tracker.submit(frame) == PresentFeedbackDisposition::kSubmitted);
  assert(tracker.receive({ViewId{1U}, FrameId{1U}, SurfaceGeneration{1U}, MetricsGeneration{1U},
                          PresentOutcome::kPresented, PresentEvidenceKind::kApproximate, 1U}) ==
         PresentFeedbackDisposition::kUnqualifiedEvidence);
  assert(tracker.receive({ViewId{1U}, FrameId{1U}, SurfaceGeneration{1U}, MetricsGeneration{1U},
                          PresentOutcome::kPresented, PresentEvidenceKind::kPlatformQualified, 2U}) ==
         PresentFeedbackDisposition::kPresented);
  assert(tracker.receive({ViewId{1U}, FrameId{1U}, SurfaceGeneration{1U}, MetricsGeneration{1U},
                          PresentOutcome::kPresented, PresentEvidenceKind::kPlatformQualified, 2U}) ==
         PresentFeedbackDisposition::kDuplicate);
  assert(lifecycle.replace({ViewId{1U}, SurfaceGeneration{2U}, MetricsGeneration{2U}, metrics}) ==
         SurfaceLifecycleDisposition::kReplaced);
  assert(tracker.receive({ViewId{1U}, FrameId{1U}, SurfaceGeneration{1U}, MetricsGeneration{1U},
                          PresentOutcome::kPresented, PresentEvidenceKind::kPlatformQualified, 3U}) ==
         PresentFeedbackDisposition::kStaleGeneration);
}
}  // namespace

int main() {
  local_remote_and_duplicate_paths_are_recorded_once<ReferenceObjectStore>();
  local_remote_and_duplicate_paths_are_recorded_once<IndexedObjectStore>();
  presented_feedback_is_qualified_and_generation_fenced();
  return 0;
}
