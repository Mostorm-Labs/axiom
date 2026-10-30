#include "canvas/runtime/runtime_composition.hpp"
#include <cassert>
#include <vector>

namespace {
struct Recorder {
  canvas::runtime::RuntimeComposition* owner = nullptr;
  std::vector<canvas::semantic::Operation> operations;
  std::vector<canvas::semantic::ApplySource> sources;
  bool reentrantCloseRejected = false;
  static void applied(void* opaque, const canvas::semantic::Operation& operation,
                      const canvas::semantic::CanonicalCommitRecord& record) noexcept {
    auto& self = *static_cast<Recorder*>(opaque);
    self.operations.push_back(operation);
    self.sources.push_back(record.source);
    self.reentrantCloseRejected = !self.owner->close();
  }
};
}

int main() {
  using namespace canvas;
  runtime::RuntimeComposition owner;
  const auto document = foundation::ObjectId::fromUint64(0x4321);
  assert(owner.openDocument(document) && owner.bindSurface(256, 256));
  Recorder recorder{&owner, {}, {}, false};
  assert(owner.setLocalOperationCallback(&Recorder::applied, &recorder));
  assert(owner.registerSource(1));
  input::PlatformPointerSample sample{};
  sample.source=1; sample.pointer=1; sample.sequence=1; sample.timestampNs=1;
  sample.x=20; sample.y=20; sample.phase=input::PointerPhase::kDown;
  input::PlatformPointerBatch batch; batch.samples.push_back(sample);
  const auto key=owner.beginContact(batch); assert(key);
  sample.sequence=2; sample.timestampNs=2; sample.x=60; sample.y=60;
  sample.phase=input::PointerPhase::kUp; batch.samples[0]=sample;
  assert(owner.submitContact(*key,batch));
  assert(recorder.operations.size()==1 && recorder.operations.front().document_id.value()==document);
  assert(recorder.reentrantCloseRejected);
  auto& facade=owner.controlFacade(); const auto targets=facade.mountedCanvasTargets(); assert(!targets.empty());
  runtime::CanvasControlRequest history{}; history.target=targets.front(); history.clientId=71;
  history.requestId=1; history.payload.kind=runtime::CanvasControlPayloadKind::kUndo;
  assert(facade.submitCanvasControl(history).state==runtime::ProductControlState::kQueued && owner.ownerPump());
  assert(recorder.operations.size()==2 && recorder.sources.back()==semantic::ApplySource::kUndoRedo);
  history.requestId=2; history.payload.kind=runtime::CanvasControlPayloadKind::kRedo;
  assert(facade.submitCanvasControl(history).state==runtime::ProductControlState::kQueued && owner.ownerPump());
  assert(recorder.operations.size()==3 && recorder.sources.back()==semantic::ApplySource::kUndoRedo);
  assert(recorder.operations[0].id != recorder.operations[1].id && recorder.operations[1].id != recorder.operations[2].id);

  semantic::ObjectRecord shape{};
  shape.id=foundation::ObjectId::fromUint64(77); shape.kind=semantic::ObjectKind::kShape;
  shape.kind_version=1; shape.placement.order_key=semantic::OrderKey({0x80});
  shape.content=semantic::ShapeContent{1,20,20};
  semantic::Operation remote{semantic::OperationId{foundation::ObjectId::fromUint64(42)},
      semantic::DocumentId{document},1,1,semantic::InsertObjectsOp{{shape}}};
  const auto applied=owner.applyRemoteOperation(remote);
  assert(applied.disposition==semantic::ApplyDisposition::kApplied);
  assert(owner.hostObservation()->objectCount==2 && recorder.operations.size()==3);
  assert(owner.applyRemoteOperation(remote).disposition==semantic::ApplyDisposition::kAlreadyApplied);
  remote.document_id=semantic::DocumentId{foundation::ObjectId::fromUint64(999)};
  assert(owner.applyRemoteOperation(remote).disposition==semantic::ApplyDisposition::kRejected);
  assert(recorder.operations.size()==3);
  assert(owner.setLocalOperationCallback(nullptr,nullptr));
  assert(owner.close());
}
