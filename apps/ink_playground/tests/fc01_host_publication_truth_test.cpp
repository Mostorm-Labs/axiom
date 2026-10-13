#include "../common/ink_playground_host.hpp"
#include "ink_playground_history_test_access.hpp"
#include "canvas/semantic/codec.hpp"
#include "../common/canvas_control_binding.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <vector>
#include <thread>

namespace {
using namespace canvas;
using Host = ink_playground::InkPlaygroundHost;
using Access = ink_playground::InkPlaygroundHistoryTestAccess;
using Progress = ink_playground::AxisProgress;

// Deterministic external provider adapter: no filesystem or cloud claim.
// It records every delivery, including accidental duplicate/remote delivery.
struct AxisRecorder final : ink_playground::LocalOperationSink {
  struct Event { semantic::Operation operation; semantic::CanonicalCommitRecord commit; };
  std::vector<Event> events;
  std::map<semantic::OperationId, ink_playground::AxisOperationStatus> states;
  semantic::CanonicalCommitStamp localFrontier{}, cloudFrontier{};
  bool failDelivery = false;
  void applied(const semantic::Operation& op, const semantic::CanonicalCommitRecord& record) noexcept override {
    events.push_back({op,record});
    states[record.operation_id] = {failDelivery ? Progress::kFailed : Progress::kPending,Progress::kPending};
  }
  ink_playground::AxisOperationStatus status(const semantic::OperationId& id,
      const semantic::CanonicalCommitStamp& stamp) const noexcept override {
    const auto event=std::find_if(events.begin(),events.end(),[&](const auto& item){
      return item.commit.operation_id==id && item.commit.commit_stamp==stamp;
    });
    if(event==events.end())return {};
    const auto item=states.find(id);
    return item==states.end()?ink_playground::AxisOperationStatus{}:item->second;
  }
  void localReady(std::size_t event) {
    states[events[event].commit.operation_id].localDurable=Progress::kConfirmed;
    localFrontier=events[event].commit.commit_stamp;
  }
  void cloudReady(std::size_t event) {
    assert(states[events[event].commit.operation_id].localDurable==Progress::kConfirmed);
    states[events[event].commit.operation_id].cloudSynced=Progress::kConfirmed;
    cloudFrontier=events[event].commit.commit_stamp;
  }
};

void create(Host& host, ink_playground::CreationPreset preset,std::uint64_t keyId=42) {
  assert(host.selectCreationPreset(preset));
  const input::PointerKey key{11,keyId,1};
  assert(host.creationPointer(key,input::PointerPhase::kDown,20,20));
  assert(host.creationPointer(key,input::PointerPhase::kMove,100,80));
  assert(host.creationPointer(key,input::PointerPhase::kUp,100,80));
}

semantic::ObjectRecord shape(std::uint64_t id) {
  semantic::ObjectRecord r{};r.id=foundation::ObjectId::fromUint64(id);
  r.kind=semantic::ObjectKind::kShape;r.kind_version=1;
  r.placement.order_key=semantic::OrderKey{{1,static_cast<std::uint8_t>(id%254+1)}};
  r.content=semantic::ShapeContent{1,20,20};return r;
}

void five_presets_publish_exactly_once_and_four_domains_remain_separate() {
  for(const auto preset:{ink_playground::CreationPreset::kRectangle,ink_playground::CreationPreset::kEllipse,
      ink_playground::CreationPreset::kTriangle,ink_playground::CreationPreset::kLine,ink_playground::CreationPreset::kArrow}) {
    Host host;assert(host.bindSurface(256,256));host.setPlatformPresentationDeferred(true);
    AxisRecorder axis;host.setLocalOperationSink(&axis);
    assert(!host.operationOutcome().applied);
    create(host,preset);
    assert(axis.events.size()==1);
    const auto& event=axis.events.front();
    assert(event.commit.operation_id==event.operation.id);
    assert(event.commit.source==semantic::ApplySource::kLocalInteraction);
    assert(event.commit.change_set.objects().size()==1);
    assert(event.commit.commit_stamp.ordinal.value()==1);
    assert(host.sceneRevision()==host.semanticGeneration().value());
    auto outcome=host.operationOutcome();
    assert(outcome.applied && outcome.applied->operation_id==event.operation.id);
    assert(outcome.presentedGeneration.value()==0);
    assert(outcome.axis.localDurable==Progress::kPending && outcome.axis.cloudSynced==Progress::kPending);
    assert(axis.localFrontier.ordinal.value()==0 && axis.cloudFrontier.ordinal.value()==0);
    const auto gen=host.semanticGeneration();
    assert(host.presentCanonicalFrame(1,0,false));
    outcome=host.operationOutcome();
    assert(outcome.presentedGeneration==gen);
    assert(outcome.axis.localDurable==Progress::kPending);
    axis.states[event.operation.id].localDurable=Progress::kFailed;
    assert(host.operationOutcome().axis.localDurable==Progress::kFailed);
    assert(host.semanticGeneration()==gen && host.semanticObjectCount()==1);
    axis.localReady(0);
    assert(host.operationOutcome().axis.localDurable==Progress::kConfirmed);
    assert(host.operationOutcome().axis.cloudSynced==Progress::kPending);
    axis.cloudReady(0);
    assert(host.operationOutcome().axis.cloudSynced==Progress::kConfirmed);
    assert(axis.cloudFrontier==event.commit.commit_stamp);
    assert(axis.events.size()==1);
    // Duplicate Up and rendering never republish the accepted operation.
    assert(host.creationPointer({11,42,1},input::PointerPhase::kUp,100,80));
    assert(host.presentCanonicalFrame(2,0,false));
    assert(axis.events.size()==1 && host.semanticGeneration()==gen);
    ink_playground::CanvasControlBinding binding(host,{1,1,1,1});
    const auto snapshot=binding.snapshot({1,1,1,1});
    assert(snapshot.lastAppliedOperation==event.operation.id.value());
    assert(snapshot.appliedCommitOrdinal==event.commit.commit_stamp.ordinal.value());
    assert(snapshot.presentedSemanticGeneration==gen.value());
    assert(snapshot.axisLocalDurable==3 && snapshot.axisCloudSynced==3);
  }
  std::cout<<"PASS E09 five presets; Applied/Presented/LocalDurable/CloudSynced, delayed/failed ACKs\n";
}

void remote_replay_reject_duplicate_and_cancel_have_no_local_outbox() {
  Host host;assert(host.bindSurface(256,256));AxisRecorder axis;host.setLocalOperationSink(&axis);
  const semantic::DocumentId document{foundation::ObjectId::fromUint64(1)};
  semantic::Operation remote{semantic::OperationId{foundation::ObjectId::fromUint64(700)},document,
      1,1,semantic::InsertObjectsOp{{shape(700)}}};
  assert(host.applyExternalOperation(remote,semantic::ApplySource::kRemoteSync).disposition==semantic::ApplyDisposition::kApplied);
  assert(axis.events.empty());
  assert(host.operationOutcome().axis.localDurable==Progress::kUnavailable);
  const auto records=Access::objects(host);const auto gen=host.semanticGeneration();
  assert(host.applyExternalOperation(remote,semantic::ApplySource::kRemoteSync).disposition==semantic::ApplyDisposition::kAlreadyApplied);
  assert(host.semanticGeneration()==gen && axis.events.empty());
  auto collision=remote;std::get<semantic::InsertObjectsOp>(collision.payload).objects.front().transform.tx=1;
  assert(host.applyExternalOperation(collision,semantic::ApplySource::kRemoteSync).disposition==semantic::ApplyDisposition::kRejected);
  auto invalid=remote;invalid.id=semantic::OperationId{foundation::ObjectId::fromUint64(701)};
  invalid.payload=semantic::InsertObjectsOp{{shape(701),shape(702)}};
  std::get<semantic::InsertObjectsOp>(invalid.payload).objects.back().kind_version=2;
  assert(host.applyExternalOperation(invalid,semantic::ApplySource::kRestoreReplay).disposition==semantic::ApplyDisposition::kRejected);
  assert(Access::objects(host)==records && host.semanticGeneration()==gen && axis.events.empty());
  remote.id=semantic::OperationId{foundation::ObjectId::fromUint64(703)};
  remote.payload=semantic::InsertObjectsOp{{shape(703)}};
  assert(host.applyExternalOperation(remote,semantic::ApplySource::kRestoreReplay).disposition==semantic::ApplyDisposition::kApplied);
  assert(axis.events.empty());
  assert(host.selectCreationPreset(ink_playground::CreationPreset::kTriangle));
  const auto cancelGen=host.semanticGeneration();
  assert(host.creationPointer({11,44,1},input::PointerPhase::kDown,20,20));
  for(unsigned i=0;i<100;++i)assert(host.creationPointer({11,44,1},input::PointerPhase::kMove,60+i,80));
  assert(host.creationPointer({11,44,1},input::PointerPhase::kCancel,160,80));
  assert(host.semanticGeneration()==cancelGen && axis.events.empty());
  std::cout<<"PASS E09 real host Remote/Replay/AlreadyApplied/Rejected/100 Move/Cancel no-echo\n";
}

void off_owner_external_apply_is_rejected() {
  Host host;assert(host.bindSurface(256,256));
  const semantic::Operation operation{semantic::OperationId{foundation::ObjectId::fromUint64(800)},
      semantic::DocumentId{foundation::ObjectId::fromUint64(1)},1,1,semantic::InsertObjectsOp{{shape(800)}}};
  semantic::ApplyResult result;
  std::thread worker([&]{result=host.applyExternalOperation(operation,semantic::ApplySource::kRemoteSync);});
  worker.join();
  assert(result.disposition==semantic::ApplyDisposition::kRejected && host.semanticObjectCount()==0);
}

struct ReentrantSink final : ink_playground::LocalOperationSink {
  Host& host; semantic::ApplyResult nested; bool nestedFixture=true; bool nestedPointer=true;
  explicit ReentrantSink(Host& owner):host(owner){}
  void applied(const semantic::Operation&,const semantic::CanonicalCommitRecord&) noexcept override {
    const semantic::Operation operation{semantic::OperationId{foundation::ObjectId::fromUint64(850)},
        semantic::DocumentId{foundation::ObjectId::fromUint64(1)},1,1,semantic::InsertObjectsOp{{shape(850)}}};
    nested=host.applyExternalOperation(operation,semantic::ApplySource::kRemoteSync);
    nestedFixture=host.seedQualificationFixture(1);
    nestedPointer=host.creationPointer({11,99,1},input::PointerPhase::kDown,20,20);
  }
  ink_playground::AxisOperationStatus status(const semantic::OperationId&,const semantic::CanonicalCommitStamp&) const noexcept override {return {};}
};
void publication_callback_cannot_reenter_apply_and_old_surface_is_not_presented() {
  Host host;assert(host.bindSurface(256,256));ReentrantSink sink(host);host.setLocalOperationSink(&sink);
  create(host,ink_playground::CreationPreset::kRectangle);
  assert(sink.nested.disposition==semantic::ApplyDisposition::kRejected && host.semanticObjectCount()==1);
  assert(!sink.nestedFixture && !sink.nestedPointer && !host.creationActive());
  assert(host.presentCanonicalFrame(1,0,false));
  assert(host.operationOutcome().presentedGeneration==host.semanticGeneration());
  assert(host.resizeSurface(300,256));
  assert(host.operationOutcome().presentedGeneration.value()==0);
}

void edits_history_and_failures_preserve_committed_truth() {
  Host host;assert(host.bindSurface(256,256));host.setPlatformPresentationDeferred(true);
  AxisRecorder axis;host.setLocalOperationSink(&axis);create(host,ink_playground::CreationPreset::kTriangle);
  assert(host.setSelectionMode(true));assert(host.selectAtViewPoint(60,60));
  const auto verifyIntent=[&](const auto& action,semantic::OperationKind kind) {
    const auto events=axis.events.size();const auto gen=host.semanticGeneration().value();
    assert(action());assert(axis.events.size()==events+1);
    assert(axis.events.back().operation.kind()==kind);
    assert(host.semanticGeneration().value()==gen+1);
  };
  verifyIntent([&]{return host.setSelectedObjectSize(120,90);},semantic::OperationKind::kSetObjectSize);
  verifyIntent([&]{return host.setSelectedAppearance({});},semantic::OperationKind::kPatchProperties);
  semantic::Transform2D transform{};transform.tx=30;transform.ty=25;
  verifyIntent([&]{return host.setSelectedTransform(transform);},semantic::OperationKind::kSetTransforms);
  const auto noOpEvents=axis.events.size();
  assert(host.setSelectedOrder(true));
  assert(axis.events.size()==noOpEvents);
  auto sibling=shape(900);sibling.transform.tx=180;
  assert(Access::applyRemote(host,semantic::InsertObjectsOp{{sibling}}));
  verifyIntent([&]{return host.setSelectedOrder(true);},semantic::OperationKind::kSetPlacements);
  verifyIntent([&]{return host.patchSelectedProperty(2,semantic::PropertyValue{true});},semantic::OperationKind::kPatchProperties);
  verifyIntent([&]{return host.unlockSelectedObjects();},semantic::OperationKind::kPatchProperties);
  const auto beforeDelete=Access::objects(host);
  verifyIntent([&]{return host.deleteSelectedObjects();},semantic::OperationKind::kDeleteObjects);
  verifyIntent([&]{return host.undo();},semantic::OperationKind::kRestoreObjects);
  assert(axis.events.back().commit.source==semantic::ApplySource::kUndoRedo);
  assert(Access::objects(host)==beforeDelete);
  verifyIntent([&]{return host.redo();},semantic::OperationKind::kDeleteObjects);
  // Lost render target cannot make the semantic compensation disappear.
  assert(host.loseSurface());axis.failDelivery=true;
  verifyIntent([&]{return host.undo();},semantic::OperationKind::kRestoreObjects);
  assert(!host.presentCanonicalFrame(1,0,false));
  const auto committed=host.semanticGeneration();
  assert(host.operationOutcome().axis.localDurable==Progress::kFailed);
  assert(Access::objects(host)==beforeDelete && host.semanticGeneration()==committed);
  assert(host.operationOutcome().presentedGeneration.value()==0);
  std::cout<<"PASS E09 selected operation family and Undo/Redo source stamps; Render/Axis failure retains Applied\n";
}
}  // namespace
int main(int argc,char** argv) {
  if(argc>1 && std::string_view(argv[1])=="reentrant"){publication_callback_cannot_reenter_apply_and_old_surface_is_not_presented();return 0;}
  if(argc>1){off_owner_external_apply_is_rejected();return 0;}
  off_owner_external_apply_is_rejected();
  publication_callback_cannot_reenter_apply_and_old_surface_is_not_presented();
  five_presets_publish_exactly_once_and_four_domains_remain_separate();
  remote_replay_reject_duplicate_and_cancel_have_no_local_outbox();
  edits_history_and_failures_preserve_committed_truth();
}
