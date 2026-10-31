#include "canvas/runtime/runtime_composition.hpp"

#include <gtest/gtest.h>
#include <set>
#include <string>
#include <vector>

namespace {
using namespace canvas;
constexpr auto kSnapshotCapability =
    semantic::SnapshotCapability::kShape2TriangleRichTextFormattingConnectorAndSectionV1;

struct Recorder {
  std::vector<semantic::Operation> operations;
  static void applied(void* context, const semantic::Operation& operation,
                      const semantic::CanonicalCommitRecord&) noexcept {
    static_cast<Recorder*>(context)->operations.push_back(operation);
  }
};

const semantic::ObjectRecord& createdObject(const semantic::Operation& operation) {
  if (const auto* stroke = std::get_if<semantic::AddStrokeOp>(&operation.payload))
    return stroke->object;
  return std::get<semantic::InsertObjectsOp>(operation.payload).objects.front();
}

struct Host {
  Recorder recorder;
  runtime::RuntimeComposition owner;
  std::uint64_t request = 0;
  std::uint64_t sequence = 0;

  void bind() {
    ASSERT_TRUE(owner.bindSurface(512, 512));
    ASSERT_TRUE(owner.registerSource(1));
    ASSERT_TRUE(owner.setLocalOperationCallback(&Recorder::applied, &recorder));
  }
  void control(runtime::CanvasControlPayload payload) {
    auto& facade = owner.controlFacade();
    runtime::CanvasControlRequest command{};
    command.target = facade.mountedCanvasTargets().front();
    command.clientId = 91; command.requestId = ++request; command.payload = payload;
    ASSERT_EQ(facade.submitCanvasControl(command).state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(owner.ownerPump());
    const auto receipt = facade.canvasControlReceipt({91, request});
    ASSERT_TRUE(receipt);
    ASSERT_EQ(receipt->state, runtime::CanvasControlReceiptState::kApplied);
  }
  void tool(int preset) {
    runtime::CanvasControlPayload payload{};
    if (preset < 0) {
      payload.kind = runtime::CanvasControlPayloadKind::kSelectTool;
      payload.tool = runtime::CanvasToolKind::kInk;
    } else {
      payload.kind = runtime::CanvasControlPayloadKind::kSelectCreationPreset;
      payload.creationPreset = static_cast<runtime::CanvasCreationPreset>(preset);
    }
    control(payload);
  }
  bool contact(float x, float y, float endX, float endY) {
    input::PlatformPointerSample sample{};
    sample.source = 1; sample.pointer = 1;
    sample.sequence = ++sequence; sample.timestampNs = sequence;
    sample.x = x; sample.y = y; sample.phase = input::PointerPhase::kDown;
    input::PlatformPointerBatch batch; batch.samples.push_back(sample);
    const auto contact = owner.beginContact(batch);
    if (!contact) return false;
    sample.sequence = ++sequence; sample.timestampNs = sequence;
    sample.x = endX; sample.y = endY; sample.phase = input::PointerPhase::kUp;
    batch.samples[0] = sample;
    return owner.submitContact(*contact, batch);
  }
  bool draw(float x) { return contact(x, 40, x + 60, 100); }
  void history(bool undo) {
    runtime::CanvasControlPayload payload{};
    payload.kind = undo ? runtime::CanvasControlPayloadKind::kUndo
                        : runtime::CanvasControlPayloadKind::kRedo;
    control(payload);
  }
  void font() {
    semantic::ResourceManifest manifest;
    manifest.entries.push_back({semantic::ResourceId{foundation::ObjectId::fromUint64(90)},
        semantic::ResourceKind::kFont, 1,
        "sha256:4bf5122f344554c53bde2ebb8cd2b7e3d1600ad631c385a5d7cce23c7785459a",
        1, 1, 1, {8, 1, 16, 0, 24, 1}});
    ASSERT_TRUE(owner.bindInitialResourceManifest(manifest, 55));
  }
  void text(runtime::TextIntentKind kind, foundation::ObjectId object = {}) {
    auto& facade = owner.controlFacade();
    runtime::TextIntent intent{};
    intent.target = facade.mountedCanvasTargets().front();
    intent.clientId = 92; intent.requestId = ++request; intent.kind = kind;
    intent.token = facade.readTextEditor().token;
    intent.object = object; intent.viewX = 40; intent.viewY = 150;
    ASSERT_EQ(facade.submitTextIntent(intent).status, runtime::TextStatus::kQueued);
    ASSERT_TRUE(owner.ownerPump());
    const auto receipt = facade.textIntentReceipt(92, request);
    ASSERT_TRUE(receipt);
    ASSERT_EQ(receipt->status, runtime::TextStatus::kApplied);
  }
  void replaceText() {
    auto& facade = owner.controlFacade();
    runtime::TextInputEvent event{};
    event.token = facade.readTextEditor().token;
    event.sequence = 1; event.kind = runtime::TextInputKind::kReplace;
    event.text = u"writer";
    ASSERT_EQ(facade.submitTextInput(event).status, runtime::TextStatus::kQueued);
    ASSERT_TRUE(owner.ownerPump());
    const auto receipt = facade.textInputReceipt(event.token, event.sequence);
    ASSERT_TRUE(receipt);
    ASSERT_EQ(receipt->status, runtime::TextStatus::kApplied);
  }
  runtime::DocumentRestoreRequest checkpoint() {
    auto& facade = owner.controlFacade();
    runtime::DocumentSnapshotRequest checkpointRequest{};
    checkpointRequest.expectedSemanticGeneration = facade.readCanvasControlSnapshot(
        facade.mountedCanvasTargets().front()).semanticGeneration;
    checkpointRequest.documentRevision = 1; checkpointRequest.recoveryFrontier = {8, 1, 16, 1, 24, 1};
    const auto saved = owner.captureDocumentSnapshot(checkpointRequest);
    EXPECT_EQ(saved.status, runtime::DocumentSnapshotStatus::kCaptured);
    const auto decoded = semantic::SnapshotCodec::decode(saved.bytes, kSnapshotCapability);
    EXPECT_TRUE(decoded.ok());
    runtime::DocumentRestoreRequest restore{};
    restore.bytes = saved.bytes; restore.expectedDocumentId = owner.documentId().value();
    restore.documentRevision = checkpointRequest.documentRevision;
    restore.recoveryFrontier = checkpointRequest.recoveryFrontier; restore.resourceBinding = 55;
    if (decoded.ok()) restore.checkpointDigest = decoded.snapshot->checkpoint_digest;
    return restore;
  }
};

// -1 is Ink; 0..5 are the public Rectangle/Ellipse/Triangle/Line/Arrow/Sticky presets.
class GeneratedObjectIdentity : public testing::TestWithParam<int> {};

TEST_P(GeneratedObjectIdentity, SnapshotRestoreThenPointerCreationAndHistory) {
  Host source;
  ASSERT_TRUE(source.owner.openDocument(foundation::ObjectId::fromUint64(0x4321)));
  source.bind(); source.tool(GetParam());
  ASSERT_TRUE(source.draw(40));
  ASSERT_EQ(source.recorder.operations.size(), 1U);
  const auto first = createdObject(source.recorder.operations.front());
  const auto restore = source.checkpoint();
  std::set<foundation::ObjectId> ids{first.id};
  for (int repeat = 0; repeat < 3; ++repeat) {
    Host recovered;
    ASSERT_EQ(recovered.owner.openDocumentFromSnapshot(restore), runtime::DocumentRestoreStatus::kRestored);
    recovered.bind(); recovered.tool(GetParam());
    ASSERT_TRUE(recovered.draw(220));
    ASSERT_EQ(recovered.recorder.operations.size(), 1U);
    const auto second = createdObject(recovered.recorder.operations.front());
    EXPECT_FALSE(second.id.isZero());
    EXPECT_TRUE(ids.insert(second.id).second);
    ASSERT_EQ(recovered.owner.hostObservation()->objectCount, 2U);
    recovered.history(true);
    EXPECT_EQ(recovered.owner.hostObservation()->objectCount, 1U);
    recovered.history(false);
    EXPECT_EQ(recovered.owner.hostObservation()->objectCount, 2U);
    const auto roundTrip = recovered.checkpoint();
    const auto decoded = semantic::SnapshotCodec::decode(roundTrip.bytes, kSnapshotCapability);
    ASSERT_TRUE(decoded.ok());
    ASSERT_EQ(decoded.snapshot->objects.size(), 2U);
    bool originalRetained = false, newRetained = false;
    for (const auto& object : decoded.snapshot->objects) {
      originalRetained |= object.id == first.id;
      newRetained |= object.id == second.id;
    }
    EXPECT_TRUE(originalRetained && newRetained);
  }
}

TEST_P(GeneratedObjectIdentity, IndependentWritersMergeWithoutRewritingIdentity) {
  Host first, second;
  const auto document = foundation::ObjectId::fromUint64(0x4321);
  ASSERT_TRUE(first.owner.openDocument(document));
  ASSERT_TRUE(second.owner.openDocument(document));
  first.bind(); second.bind(); first.tool(GetParam()); second.tool(GetParam());
  ASSERT_TRUE(first.draw(40)); ASSERT_TRUE(second.draw(220));
  ASSERT_EQ(first.recorder.operations.size(), 1U);
  ASSERT_EQ(second.recorder.operations.size(), 1U);
  const auto a = first.recorder.operations.front();
  const auto b = second.recorder.operations.front();
  std::string identity;
  constexpr char hex[] = "0123456789abcdef";
  for (const auto byte : createdObject(a).id.bytes) {
    identity += hex[byte >> 4U]; identity += hex[byte & 15U];
  }
  RecordProperty("first_object_id", identity);
  EXPECT_NE(createdObject(a).id, createdObject(b).id);
  ASSERT_EQ(second.owner.applyRemoteOperation(a).disposition, semantic::ApplyDisposition::kApplied);
  ASSERT_EQ(first.owner.applyRemoteOperation(b).disposition, semantic::ApplyDisposition::kApplied);
  EXPECT_EQ(first.owner.hostObservation()->objectCount, 2U);
  EXPECT_EQ(second.owner.hostObservation()->objectCount, 2U);
  EXPECT_EQ(first.recorder.operations.size(), 1U); // Remote input never becomes local echo.
  EXPECT_EQ(second.recorder.operations.size(), 1U);
  second.history(true);
  ASSERT_EQ(first.owner.applyRemoteOperation(second.recorder.operations.back()).disposition,
            semantic::ApplyDisposition::kApplied);
  EXPECT_EQ(first.owner.hostObservation()->objectCount, 1U);
  second.history(false);
  ASSERT_EQ(first.owner.applyRemoteOperation(second.recorder.operations.back()).disposition,
            semantic::ApplyDisposition::kApplied);
  EXPECT_EQ(first.owner.hostObservation()->objectCount, 2U);

  auto duplicate = a;
  duplicate.id = semantic::OperationId{foundation::ObjectId::fromUint64(0xDEADBEEF)};
  EXPECT_EQ(first.owner.applyRemoteOperation(duplicate).disposition, semantic::ApplyDisposition::kRejected);
  EXPECT_EQ(first.owner.hostObservation()->objectCount, 2U);
}

INSTANTIATE_TEST_SUITE_P(PointerTools, GeneratedObjectIdentity, testing::Range(-1, 6));

TEST(GeneratedObjectIdentity, LegacyNumericSnapshotRetainsIdentityAndAllowsNewRectangle) {
  Host source, restored;
  const auto document = foundation::ObjectId::fromUint64(0x4321);
  ASSERT_TRUE(source.owner.openDocument(document)); source.bind();
  semantic::ObjectRecord legacy{};
  // Identity observed in the released SDK's first pointer-created rectangle.
  legacy.id = foundation::ObjectId::fromUint64(0x1000000000001ULL);
  legacy.kind = semantic::ObjectKind::kShape; legacy.kind_version = 1;
  legacy.content = semantic::ShapeContent{1, 60, 60};
  legacy.placement.order_key = semantic::OrderKey{{0x80}};
  const semantic::Operation operation{semantic::OperationId{foundation::ObjectId::fromUint64(98)},
      semantic::DocumentId{document}, 1, 1, semantic::InsertObjectsOp{{legacy}}};
  ASSERT_EQ(source.owner.applyRemoteOperation(operation).disposition, semantic::ApplyDisposition::kApplied);
  ASSERT_EQ(restored.owner.openDocumentFromSnapshot(source.checkpoint()), runtime::DocumentRestoreStatus::kRestored);
  restored.bind(); restored.tool(0);
  ASSERT_TRUE(restored.draw(220));
  ASSERT_EQ(restored.recorder.operations.size(), 1U);
  EXPECT_NE(createdObject(restored.recorder.operations.front()).id, legacy.id);
  const auto saved = restored.checkpoint();
  const auto decoded = semantic::SnapshotCodec::decode(saved.bytes, kSnapshotCapability);
  ASSERT_TRUE(decoded.ok());
  ASSERT_EQ(decoded.snapshot->objects.size(), 2U);
  bool unchanged = false;
  for (const auto& object : decoded.snapshot->objects) unchanged |= object == legacy;
  EXPECT_TRUE(unchanged);
}

TEST(GeneratedObjectIdentity, AutomaticConnectorTargetsAreIndependentAcrossWriters) {
  Host first, second;
  const auto document = foundation::ObjectId::fromUint64(0x4321);
  semantic::ObjectRecord source{};
  source.id = foundation::ObjectId::fromUint64(99);
  source.kind = semantic::ObjectKind::kShape; source.kind_version = 1;
  source.transform.tx = 40; source.transform.ty = 40;
  source.content = semantic::ShapeContent{1, 60, 60};
  source.placement.order_key = semantic::OrderKey{{0x80}};
  const semantic::Operation base{semantic::OperationId{foundation::ObjectId::fromUint64(98)},
      semantic::DocumentId{document}, 1, 1, semantic::InsertObjectsOp{{source}}};
  std::set<foundation::ObjectId> ids{source.id};
  for (auto* host : {&first, &second}) {
    ASSERT_TRUE(host->owner.openDocument(document)); host->bind();
    ASSERT_EQ(host->owner.applyRemoteOperation(base).disposition, semantic::ApplyDisposition::kApplied);
    runtime::CanvasControlPayload select{};
    select.kind = runtime::CanvasControlPayloadKind::kSelectTool;
    select.tool = runtime::CanvasToolKind::kSelect;
    host->control(select);
    ASSERT_TRUE(host->contact(70, 70, 70, 70));
    ASSERT_EQ(host->owner.controlFacade().readSelectionSummary().primary, source.id);
    // The public right-side connector affordance is 28 view pixels beyond the edge.
    ASSERT_TRUE(host->contact(128, 70, 128, 70));
    ASSERT_EQ(host->recorder.operations.size(), 1U);
    const auto& inserted = std::get<semantic::InsertObjectsOp>(host->recorder.operations.front().payload).objects;
    ASSERT_EQ(inserted.size(), 2U);
    for (const auto& object : inserted) EXPECT_TRUE(ids.insert(object.id).second);
    const auto& connector = std::get<semantic::ConnectorContent>(inserted.front().content);
    EXPECT_EQ(std::get<semantic::AttachedEndpoint>(connector.end.value).target_object_id, inserted.back().id);
  }
  ASSERT_EQ(first.owner.applyRemoteOperation(second.recorder.operations.front()).disposition,
            semantic::ApplyDisposition::kApplied);
  EXPECT_EQ(first.owner.hostObservation()->objectCount, 5U);
}

TEST(GeneratedObjectIdentity, RichTextObjectsAndParagraphsAreIndependentAcrossWriters) {
  Host first, second;
  const auto document = foundation::ObjectId::fromUint64(0x4321);
  std::set<foundation::ObjectId> ids;
  for (auto* host : {&first, &second}) {
    ASSERT_TRUE(host->owner.openDocument(document)); host->bind(); host->font();
    host->text(runtime::TextIntentKind::kCreate);
    host->replaceText(); host->text(runtime::TextIntentKind::kFinish);
    ASSERT_EQ(host->recorder.operations.size(), 1U);
    const auto& object = createdObject(host->recorder.operations.front());
    EXPECT_TRUE(ids.insert(object.id).second);
    const auto& rich = std::get<semantic::RichTextContent>(object.content);
    ASSERT_EQ(rich.document.paragraphs.size(), 1U);
    EXPECT_TRUE(ids.insert(rich.document.paragraphs.front().id).second);
  }
  ASSERT_EQ(first.owner.applyRemoteOperation(second.recorder.operations.front()).disposition,
            semantic::ApplyDisposition::kApplied);
  EXPECT_EQ(first.owner.hostObservation()->objectCount, 2U);
}

TEST(GeneratedObjectIdentity, StickyRichTextChildrenAreIndependentAcrossWriters) {
  Host first, second;
  const auto document = foundation::ObjectId::fromUint64(0x4321);
  std::set<foundation::ObjectId> ids;
  unsigned ordinal = 100;
  for (auto* host : {&first, &second}) {
    ASSERT_TRUE(host->owner.openDocument(document)); host->bind(); host->font();
    runtime::StickyCreateRequest sticky{};
    sticky.objectId = foundation::ObjectId::fromUint64(++ordinal);
    sticky.content = semantic::StickyContent{120, 120};
    ASSERT_EQ(host->owner.controlFacade().createSticky(sticky).state,
              runtime::ConnectorCommandState::kApplied);
    host->text(runtime::TextIntentKind::kEdit, sticky.objectId);
    ASSERT_EQ(host->recorder.operations.size(), 2U);
    const auto& child = createdObject(host->recorder.operations.back());
    EXPECT_EQ(child.placement.parent_id, sticky.objectId);
    EXPECT_TRUE(ids.insert(child.id).second);
    host->replaceText(); host->text(runtime::TextIntentKind::kFinish);
  }
  ASSERT_EQ(first.owner.applyRemoteOperation(second.recorder.operations.front()).disposition,
            semantic::ApplyDisposition::kApplied);
  ASSERT_EQ(first.owner.applyRemoteOperation(second.recorder.operations[1]).disposition,
            semantic::ApplyDisposition::kApplied);
  EXPECT_EQ(first.owner.hostObservation()->objectCount, 4U);
}
} // namespace
