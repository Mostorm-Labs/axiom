#include "canvas/runtime/runtime_composition.hpp"
#include <cassert>
#include <set>
#include <vector>

namespace {
using namespace canvas;
struct Recorder {
  std::vector<semantic::Operation> operations;
  static void applied(void* context, const semantic::Operation& operation,
                      const semantic::CanonicalCommitRecord&) noexcept {
    static_cast<Recorder*>(context)->operations.push_back(operation);
  }
};
void sticky(runtime::RuntimeComposition& owner, unsigned id) {
  runtime::StickyCreateRequest request{};
  request.objectId = foundation::ObjectId::fromUint64(id);
  request.content = semantic::StickyContent{80, 80};
  assert(owner.controlFacade().createSticky(request).state == runtime::ConnectorCommandState::kApplied);
}
void history(runtime::RuntimeComposition& owner, bool undo, unsigned serial) {
  auto& facade = owner.controlFacade();
  runtime::CanvasControlRequest request{};
  request.target = facade.mountedCanvasTargets().front(); request.clientId = 73;
  request.requestId = serial;
  request.payload.kind = undo ? runtime::CanvasControlPayloadKind::kUndo : runtime::CanvasControlPayloadKind::kRedo;
  assert(facade.submitCanvasControl(request).state == runtime::ProductControlState::kQueued);
  assert(owner.ownerPump());
  const auto receipt = facade.canvasControlReceipt({73, serial});
  assert(receipt && receipt->state == runtime::CanvasControlReceiptState::kApplied);
}
}

int main() {
  using namespace canvas;
  const auto document = foundation::ObjectId::fromUint64(0x4321);
  runtime::RuntimeComposition source;
  assert(source.openDocument(document) && source.bindSurface(256, 256));
  Recorder before;
  assert(source.setLocalOperationCallback(&Recorder::applied, &before));
  sticky(source, 101);
  runtime::DocumentSnapshotRequest request{};
  request.expectedSemanticGeneration = source.controlFacade().readCanvasControlSnapshot(
      source.controlFacade().mountedCanvasTargets().front()).semanticGeneration;
  request.documentRevision = 1; request.recoveryFrontier = {8,1,16,1,24,1};
  const auto saved = source.captureDocumentSnapshot(request);
  assert(saved.status == runtime::DocumentSnapshotStatus::kCaptured);
  const auto decoded = semantic::SnapshotCodec::decode(saved.bytes,
      semantic::SnapshotCapability::kShape2TriangleRichTextFormattingConnectorAndSectionV1);
  assert(decoded.ok());
  runtime::DocumentRestoreRequest restore{};
  restore.bytes = saved.bytes; restore.expectedDocumentId = document;
  restore.documentRevision = 1; restore.recoveryFrontier = request.recoveryFrontier;
  restore.checkpointDigest = decoded.snapshot->checkpoint_digest; restore.resourceBinding = 55;
  std::set<semantic::OperationId> identities{before.operations.front().id};
  for (unsigned repeat = 0; repeat < 3; ++repeat) {
    runtime::RuntimeComposition recovered;
    assert(recovered.openDocumentFromSnapshot(restore) == runtime::DocumentRestoreStatus::kRestored);
    assert(recovered.bindSurface(256, 256));
    Recorder after;
    assert(recovered.setLocalOperationCallback(&Recorder::applied, &after));
    sticky(recovered, 102);
    history(recovered, true, 1); history(recovered, false, 2);
    assert(after.operations.size() == 3);
    for (const auto& operation : after.operations) {
      assert(!operation.id.isZero() && identities.insert(operation.id).second);
      assert(operation.document_id.value() == document);
    }
    const auto count = after.operations.size();
    assert(recovered.applyReplayOperation(before.operations.front()).disposition == semantic::ApplyDisposition::kRejected);
    assert(after.operations.size() == count); // A checkpoint object cannot be inserted twice; no local echo.
    assert(recovered.close());
  }
  // Fresh public owners of the same document must also have independent low-lane drawing IDs.
  for (unsigned repeat = 0; repeat < 3; ++repeat) {
    runtime::RuntimeComposition fresh;
    assert(fresh.openDocument(document) && fresh.bindSurface(256, 256) && fresh.registerSource(1));
    Recorder drawing;
    assert(fresh.setLocalOperationCallback(&Recorder::applied, &drawing));
    input::PlatformPointerSample sample{};
    sample.source=1; sample.pointer=1; sample.sequence=1; sample.timestampNs=1;
    sample.x=20; sample.y=20; sample.phase=input::PointerPhase::kDown;
    input::PlatformPointerBatch batch; batch.samples.push_back(sample);
    const auto key=fresh.beginContact(batch); assert(key);
    sample.sequence=2; sample.timestampNs=2; sample.x=60; sample.y=60;
    sample.phase=input::PointerPhase::kUp; batch.samples[0]=sample;
    assert(fresh.submitContact(*key,batch));
    runtime::CanvasControlRequest control{};
    auto& facade=fresh.controlFacade();
    control.target=facade.mountedCanvasTargets().front(); control.clientId=75; control.requestId=1;
    control.payload.kind=runtime::CanvasControlPayloadKind::kSelectCreationPreset;
    control.payload.creationPreset=runtime::CanvasCreationPreset::kRectangle;
    assert(facade.submitCanvasControl(control).state==runtime::ProductControlState::kQueued && fresh.ownerPump());
    const auto draw=[&](float x, float y, float endX, float endY) {
      ++sample.pointer; ++sample.sequence; ++sample.timestampNs;
      sample.x=x; sample.y=y; sample.phase=input::PointerPhase::kDown; batch.samples[0]=sample;
      const auto contact=fresh.beginContact(batch); assert(contact);
      ++sample.sequence; ++sample.timestampNs; sample.x=endX; sample.y=endY;
      sample.phase=input::PointerPhase::kUp; batch.samples[0]=sample;
      assert(fresh.submitContact(*contact,batch));
    };
    draw(150,100,220,160);
    control.requestId=2; control.payload.kind=runtime::CanvasControlPayloadKind::kSelectTool;
    control.payload.tool=runtime::CanvasToolKind::kEraser;
    control.payload.eraser.mode=runtime::OptionPatch<std::uint32_t>::set(1U);
    assert(facade.submitCanvasControl(control).state==runtime::ProductControlState::kQueued && fresh.ownerPump());
    draw(20,20,60,60);
    assert(drawing.operations.size()==3);
    for (const auto& operation:drawing.operations)
      assert(identities.insert(operation.id).second);
    assert(fresh.close());
  }
  assert(source.close());
}
