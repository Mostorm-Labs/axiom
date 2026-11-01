#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"

#include <cassert>
#include <algorithm>
#include <limits>
#include <thread>

namespace {
using namespace canvas;
template <typename Store> void checkCanonicalStore() {
  Store store;
  semantic::AppliedOperationLedger ledger;
  semantic::SemanticGenerationState generation;
  semantic::CanonicalCommitClock clock{semantic::RuntimeEpoch{1}};
  semantic::OperationEngine engine;
  foundation::DocumentBackground value;
  value.mode=foundation::BackgroundMode::kGrid;value.color={0,0,1,1};
  semantic::Operation op{semantic::OperationId{foundation::ObjectId::fromUint64(1)},
      semantic::DocumentId{foundation::ObjectId::fromUint64(2)},1,1,semantic::SetDocumentBackgroundOp{value}};
  const auto result=engine.apply(op,semantic::ApplySource::kRemoteSync,store,ledger,generation,clock);
  assert(result.disposition==semantic::ApplyDisposition::kApplied && result.commit_record);
  assert(result.commit_record->change_set.documentBackgroundChanged());
  assert(result.commit_record->change_set.objects().empty() && store.size()==0);
  assert(store.documentBackground()==value && generation.current().value()==1 && clock.lastCommittedOrdinal().value()==1);
  assert(engine.apply(op,semantic::ApplySource::kRemoteSync,store,ledger,generation,clock).disposition==semantic::ApplyDisposition::kAlreadyApplied);
  auto invalid=op;invalid.id=semantic::OperationId{foundation::ObjectId::fromUint64(3)};
  value.patternColor[0]=2;invalid.payload=semantic::SetDocumentBackgroundOp{value};
  assert(engine.apply(invalid,semantic::ApplySource::kRemoteSync,store,ledger,generation,clock).disposition==semantic::ApplyDisposition::kRejected);
  assert(generation.current().value()==1 && clock.lastCommittedOrdinal().value()==1 && store.documentBackground().color[2]==1);
}
struct Recorder {
  std::vector<semantic::Operation> operations;
  bool allow = true;
  static bool prepare(void* opaque, std::span<const semantic::Operation>) noexcept {
    return static_cast<Recorder*>(opaque)->allow;
  }
  static void applied(void* opaque, const semantic::Operation& op,
                      const semantic::CanonicalCommitRecord&) noexcept {
    static_cast<Recorder*>(opaque)->operations.push_back(op);
  }
};
bool set(runtime::RuntimeComposition& owner, foundation::DocumentBackground background,
         std::uint64_t request) {
  auto& facade = owner.controlFacade();
  runtime::CanvasControlRequest command{};
  command.target = facade.mountedCanvasTargets().front();
  command.clientId = 99; command.requestId = request;
  command.payload.kind = runtime::CanvasControlPayloadKind::kDocumentBackground;
  command.payload.background = background;
  if (facade.submitCanvasControl(command).state != runtime::ProductControlState::kQueued) return false;
  assert(owner.ownerPump());
  const auto receipt = facade.canvasControlReceipt({99, request});
  return receipt && receipt->state == runtime::CanvasControlReceiptState::kApplied;
}
bool history(runtime::RuntimeComposition& owner, bool undo, std::uint64_t request) {
  auto& facade = owner.controlFacade();
  runtime::CanvasControlRequest command{};
  command.target = facade.mountedCanvasTargets().front();
  command.clientId = 99; command.requestId = request;
  command.payload.kind = undo ? runtime::CanvasControlPayloadKind::kUndo : runtime::CanvasControlPayloadKind::kRedo;
  if (facade.submitCanvasControl(command).state != runtime::ProductControlState::kQueued) return false;
  assert(owner.ownerPump());
  const auto receipt = facade.canvasControlReceipt({99, request});
  return receipt && receipt->state == runtime::CanvasControlReceiptState::kApplied;
}
}

int main() {
  using namespace canvas;
  checkCanonicalStore<semantic::ReferenceObjectStore>();
  checkCanonicalStore<semantic::IndexedObjectStore>();
  const auto document = foundation::ObjectId::fromUint64(0xBA01);
  runtime::RuntimeComposition owner;
  assert(owner.openDocument(document) && owner.bindSurface(256,256));
  auto& facade = owner.controlFacade();
  const foundation::DocumentBackground initial{};
  assert(facade.readBackground() == initial);
  Recorder recorder;
  assert(owner.setLocalOperationCallbacks(&Recorder::applied, &recorder, &Recorder::prepare, nullptr, nullptr));
  assert(set(owner, initial, 1) && recorder.operations.empty());

  auto dots = initial;
  dots.mode = foundation::BackgroundMode::kDots;
  dots.color = {0.2F,0.3F,0.4F,1};
  dots.patternColor = {1,0,0,0.5F};
  assert(set(owner, dots, 2) && facade.readBackground() == dots);
  assert(owner.hostObservation()->objectCount == 0 && recorder.operations.size() == 1);
  semantic::ResourceManifest lateManifest;
  assert(!owner.bindInitialResourceManifest(lateManifest,19));
  const auto encoded = semantic::SemanticCodec::encodeProtobufOperation(recorder.operations.front());
  assert(encoded.ok());
  const auto decodedOp = semantic::SemanticCodec::decodeProtobufOperation(encoded.bytes);
  assert(decodedOp.ok());
  // Hand-authored fixed32 color bytes exercise untrusted wire ingress, rather
  // than relying on the model writer to reject a non-finite value first.
  const std::uint8_t colorWire[]={0x0d,0xcd,0xcc,0x4c,0x3e,0x15,0x9a,0x99,0x99,0x3e,
      0x1d,0xcd,0xcc,0xcc,0x3e,0x25,0,0,0x80,0x3f};
  auto malformed=encoded.bytes;
  const auto colorPosition=std::search(malformed.begin(),malformed.end(),std::begin(colorWire),std::end(colorWire));
  assert(colorPosition!=malformed.end());
  const std::uint8_t notFinite[]={0,0,0xc0,0x7f};
  std::copy(std::begin(notFinite),std::end(notFinite),colorPosition+1);
  assert(!semantic::SemanticCodec::decodeProtobufOperation(malformed).ok());
  runtime::RuntimeComposition remote;
  assert(remote.openDocument(document) && remote.bindSurface(256,256));
  assert(remote.applyRemoteOperation(decodedOp.operation).disposition == semantic::ApplyDisposition::kApplied);
  assert(remote.controlFacade().readBackground() == dots);
  assert(remote.applyRemoteOperation(decodedOp.operation).disposition == semantic::ApplyDisposition::kAlreadyApplied);
  const auto generation = facade.readCanvasControlSnapshot(facade.mountedCanvasTargets().front()).semanticGeneration;

  auto invalid = dots; invalid.color[0] = std::numeric_limits<float>::quiet_NaN();
  assert(!set(owner, invalid, 3));
  invalid = dots; invalid.color[3] = 0.5F;
  assert(!set(owner, invalid, 4));
  invalid = dots; invalid.mode = static_cast<foundation::BackgroundMode>(255);
  assert(!set(owner, invalid, 5));
  assert(facade.readBackground() == dots && recorder.operations.size() == 1);
  assert(facade.readCanvasControlSnapshot(facade.mountedCanvasTargets().front()).semanticGeneration == generation);
  recorder.allow = false;
  auto grid = dots; grid.mode = foundation::BackgroundMode::kGrid;
  assert(!set(owner, grid, 6) && facade.readBackground() == dots);
  recorder.allow = true;
  assert(history(owner,true,7) && facade.readBackground() == initial);
  assert(history(owner,false,8) && facade.readBackground() == dots);
  assert(recorder.operations.size() == 3 && recorder.operations[0].id != recorder.operations[1].id);
  assert(set(owner,grid,9));

  runtime::DocumentSnapshotRequest request{};
  request.expectedSemanticGeneration = facade.readCanvasControlSnapshot(facade.mountedCanvasTargets().front()).semanticGeneration;
  request.documentRevision = 7;
  request.recoveryFrontier = {0x08,1,0x10,1,0x18,7};
  const auto saved = owner.captureDocumentSnapshot(request);
  assert(saved.status == runtime::DocumentSnapshotStatus::kCaptured);
  assert(!semantic::SnapshotCodec::decode(saved.bytes, semantic::SnapshotCapability::kShape2TriangleRichTextFormattingConnectorAndSectionV1).ok());
  const auto snapshot = semantic::SnapshotCodec::decode(saved.bytes, semantic::SnapshotCapability::kAllWithBackgroundV1);
  assert(snapshot.ok() && snapshot.snapshot->background == grid);
  runtime::DocumentRestoreRequest restore{};
  restore.bytes=saved.bytes; restore.expectedDocumentId=document; restore.documentRevision=7;
  restore.recoveryFrontier=request.recoveryFrontier; restore.checkpointDigest=snapshot.snapshot->checkpoint_digest;
  restore.resourceBinding=2;
  runtime::RuntimeComposition reopened;
  assert(reopened.openDocumentFromSnapshot(restore) == runtime::DocumentRestoreStatus::kRestored);
  assert(reopened.bindSurface(256,256) && reopened.controlFacade().readBackground() == grid);
  assert(set(reopened,dots,1) && history(reopened,true,2) && reopened.controlFacade().readBackground() == grid);
  const auto published = recorder.operations.size();
  auto conflicting = recorder.operations.back();
  conflicting.id = semantic::OperationId{foundation::ObjectId::fromUint64(0xBA02)};
  auto foreign = grid; foreign.color = {0,1,0,1};
  conflicting.payload = semantic::SetDocumentBackgroundOp{foreign};
  assert(owner.applyRemoteOperation(conflicting).disposition == semantic::ApplyDisposition::kApplied);
  assert(!history(owner,true,10) && facade.readBackground() == foreign);
  assert(recorder.operations.size() == published);
  auto collision = conflicting; collision.payload = semantic::SetDocumentBackgroundOp{dots};
  assert(owner.applyRemoteOperation(collision).disposition == semantic::ApplyDisposition::kRejected);
  assert(facade.readBackground() == foreign);
  bool wrongThreadRejected = false;
  std::thread other([&] { wrongThreadRejected = !facade.readBackground().has_value(); });
  other.join(); assert(wrongThreadRejected);
  auto tampered = *snapshot.snapshot;
  tampered.background.color = {1,0,0,1};
  assert(!semantic::SnapshotCodec::encode(tampered, semantic::SnapshotCapability::kAllWithBackgroundV1).ok());
  assert(facade.readCanvasControlSnapshot(facade.mountedCanvasTargets().front()).semanticGeneration > generation);
  assert(remote.close() && reopened.close() && owner.close());
  assert(!facade.readBackground());
}
