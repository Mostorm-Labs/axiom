#include "canvas/runtime/runtime_composition.hpp"
#include <cassert>
#include <string>
#include <thread>

int main() {
  using namespace canvas;
  runtime::RuntimeComposition owner;
  const auto document = foundation::ObjectId::fromUint64(0x4321);
  assert(owner.openDocument(document) && owner.bindSurface(256, 256));
  auto& facade = owner.controlFacade();
  const auto targets = facade.mountedCanvasTargets(); assert(!targets.empty());
  runtime::SectionIntent section{};
  section.action = runtime::SectionAction::kCreate;
  section.objectId = foundation::ObjectId::fromUint64(100);
  assert(facade.submitSectionIntent(section, 1, 800).state == runtime::ProductControlState::kQueued);
  assert(owner.ownerPump());
  const auto receipt = facade.canvasControlReceipt({800, 1});
  assert(receipt && receipt->state == runtime::CanvasControlReceiptState::kApplied);
  semantic::ObjectRecord child{};
  child.id = foundation::ObjectId::fromUint64(77); child.kind = semantic::ObjectKind::kShape;
  child.kind_version = 1; child.content = semantic::ShapeContent{1,20,20};
  child.placement.parent_id = section.objectId; child.placement.order_key = semantic::OrderKey({0x80});
  child.transform.tx = 43; child.transform.ty = 27;
  const semantic::Operation remote{semantic::OperationId{foundation::ObjectId::fromUint64(200)},
      semantic::DocumentId{document},1,1,semantic::InsertObjectsOp{{child}}};
  assert(owner.applyRemoteOperation(remote).disposition == semantic::ApplyDisposition::kApplied);
  const auto generation = facade.readCanvasControlSnapshot(targets.front()).semanticGeneration;
  runtime::DocumentSnapshotRequest request{};
  request.expectedSemanticGeneration = generation; request.documentRevision = 7;
  request.recoveryFrontier = {0x08,1,0x10,1,0x18,7};
  const auto addRequirement=[&](const char* name) {
    const std::string text(name); request.capabilityRequirements.push_back(0x0a);
    request.capabilityRequirements.push_back(static_cast<std::uint8_t>(text.size()));
    request.capabilityRequirements.insert(request.capabilityRequirements.end(), text.begin(), text.end());
  };
  addRequirement("resource-manifest-v1"); addRequirement("section-v1");
  const auto saved = owner.captureDocumentSnapshot(request);
  assert(saved.status == runtime::DocumentSnapshotStatus::kCaptured && !saved.bytes.empty());
  auto automatic = request; automatic.capabilityRequirements.clear();
  const auto automaticSnapshot = owner.captureDocumentSnapshot(automatic);
  assert(automaticSnapshot.status == runtime::DocumentSnapshotStatus::kCaptured);
  assert(automaticSnapshot.bytes == saved.bytes);
  assert(saved.semanticGeneration == generation);
  const auto decoded = semantic::SnapshotCodec::decode(saved.bytes,
      semantic::SnapshotCapability::kShape2TriangleRichTextFormattingConnectorAndSectionV1);
  assert(decoded.ok() && decoded.snapshot->document_id.value() == document);
  assert(decoded.snapshot->objects.size() == 2);
  assert(!semantic::SnapshotCodec::decode(saved.bytes,
      semantic::SnapshotCapability::kShape2TriangleRichTextFormattingAndConnectorAttachmentV1).ok());
  runtime::DocumentRestoreRequest restore{};
  restore.bytes = saved.bytes; restore.expectedDocumentId = document;
  restore.documentRevision = decoded.snapshot->document_revision;
  restore.recoveryFrontier = decoded.snapshot->recovery_frontier;
  restore.checkpointDigest = decoded.snapshot->checkpoint_digest;
  restore.resourceBinding = 33;
  runtime::RuntimeComposition borrowed;
  auto* borrowedFacade = &borrowed.controlFacade();
  assert(borrowed.openDocumentFromSnapshot(restore) == runtime::DocumentRestoreStatus::kNotReady);
  assert(&borrowed.controlFacade() == borrowedFacade);
  assert(borrowed.openDocument(document) && borrowed.close());
  runtime::RuntimeComposition reopened;
  auto invalid = restore; invalid.expectedDocumentId = foundation::ObjectId::fromUint64(999);
  assert(reopened.openDocumentFromSnapshot(invalid) == runtime::DocumentRestoreStatus::kInvalidSnapshot);
  assert(!reopened.documentId());
  invalid = restore; invalid.checkpointDigest.front() ^= 1;
  assert(reopened.openDocumentFromSnapshot(invalid) == runtime::DocumentRestoreStatus::kInvalidSnapshot);
  assert(!reopened.documentId());
  assert(reopened.openDocumentFromSnapshot(restore) == runtime::DocumentRestoreStatus::kRestored);
  assert(reopened.documentId() == document && reopened.bindSurface(256,256));
  assert(reopened.hostObservation()->objectCount == 2);
  const auto restoredSection = reopened.controlFacade().readSection(section.objectId);
  assert(restoredSection.available && restoredSection.projectionReady);
  assert(restoredSection.members.size() == 1 && restoredSection.members.front().objectId == child.id);
  assert(restoredSection.members.front().localTransform.tx == 43 && restoredSection.members.front().localTransform.ty == 27);
  assert(reopened.openDocumentFromSnapshot(restore) == runtime::DocumentRestoreStatus::kNotReady);
  assert(reopened.registerSource(2));
  input::PlatformPointerSample restoredDown{}; restoredDown.source=2; restoredDown.pointer=1;
  restoredDown.sequence=1; restoredDown.timestampNs=1; restoredDown.phase=input::PointerPhase::kDown;
  restoredDown.x=200; restoredDown.y=200;
  input::PlatformPointerBatch restoredBatch; restoredBatch.samples.push_back(restoredDown);
  const auto restoredKey=reopened.beginContact(restoredBatch); assert(restoredKey);
  restoredDown.sequence=2; restoredDown.timestampNs=2; restoredDown.phase=input::PointerPhase::kUp;
  restoredDown.x=220; restoredDown.y=220; restoredBatch.samples[0]=restoredDown;
  assert(reopened.submitContact(*restoredKey,restoredBatch));
  assert(reopened.hostObservation()->objectCount == 3);
  assert(reopened.close());
  assert(owner.registerSource(1));
  input::PlatformPointerSample down{}; down.source=1; down.pointer=1; down.sequence=1;
  down.timestampNs=1; down.phase=input::PointerPhase::kDown; down.x=180; down.y=180;
  input::PlatformPointerBatch batch; batch.samples.push_back(down);
  const auto key=owner.beginContact(batch); assert(key);
  assert(owner.captureDocumentSnapshot(request).status == runtime::DocumentSnapshotStatus::kBusy);
  assert(owner.cancelContact(*key, 2, 2));
  ++request.expectedSemanticGeneration;
  assert(owner.captureDocumentSnapshot(request).status == runtime::DocumentSnapshotStatus::kStale);
  request.expectedSemanticGeneration = generation; request.recoveryFrontier.clear();
  assert(owner.captureDocumentSnapshot(request).status == runtime::DocumentSnapshotStatus::kInvalidCheckpoint);
  runtime::DocumentSnapshotStatus otherStatus = runtime::DocumentSnapshotStatus::kCaptured;
  std::thread other([&] { otherStatus = owner.captureDocumentSnapshot(request).status; });
  other.join(); assert(otherStatus == runtime::DocumentSnapshotStatus::kWrongThread);
  assert(owner.close());
}
