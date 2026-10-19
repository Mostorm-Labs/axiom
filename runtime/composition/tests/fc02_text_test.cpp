#include "auditoryworks/axiom/v1/operation.pb.h"
#include "auditoryworks/axiom/v1/snapshot.pb.h"
#include "canvas/interaction/editor_history.hpp"
#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/rich_text_state.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "canvas/semantic/snapshot_bootstrap.hpp"
#include "protobuf_codec_internal.hpp"
#include "sha256_internal.hpp"

#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>

namespace {
using namespace canvas::semantic;
namespace p = auditoryworks::axiom::v1;
ObjectId id(std::uint64_t n) {
    return ObjectId::fromUint64(n);
}
std::vector<std::uint8_t> caps(std::initializer_list<std::string> names) {
    std::vector<std::uint8_t> out;
    for (const auto& name : names) {
        out.push_back(10);
        out.push_back(static_cast<std::uint8_t>(name.size()));
        out.insert(out.end(), name.begin(), name.end());
    }
    return out;
}
SemanticSnapshot emptySnapshot() {
    SemanticSnapshot s;
    s.document_id = DocumentId{id(100)};
    s.schema_version = 3;
    s.manifest_present = true;
    s.recovery_frontier = {8, 1, 16, 1, 24, 0};
    s.digest_algorithm = 1;
    s.capability_requirements = caps({"resource-manifest-v1"});
    s.document_digest.resize(32);
    s.checkpoint_digest.resize(32);
    return s;
}
TEST(FC02Snapshot, UnknownRequiredCapabilityRejectsBeforeTypedPublication) {
    auto s = emptySnapshot();
    s.capability_requirements = caps({"resource-manifest-v1", "unknown-feature"});
    ReferenceObjectStore objects;
    ResourceManifest manifest;
    SemanticGenerationState gen;
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;
    EXPECT_FALSE(SnapshotBootstrapper::restore(
                     s, SnapshotCapability::kLegacyV1, state, objects, manifest, gen)
                     .restored);
    EXPECT_EQ(objects.size(), 0U);
}
TEST(FC02Snapshot, TypedBootstrapMustVerifyDigestValue) {
    const auto s = emptySnapshot();
    ReferenceObjectStore objects;
    ResourceManifest manifest;
    SemanticGenerationState gen;
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;
    EXPECT_FALSE(SnapshotBootstrapper::restore(
                     s, SnapshotCapability::kLegacyV1, state, objects, manifest, gen)
                     .restored);
}
TEST(FC02Snapshot, AcceptedSemanticProjectionForExplicitEmptyManifest) {
    auto s = emptySnapshot();
    std::vector<std::uint8_t> projection;
    const auto number = [&](std::uint64_t n, unsigned width) {
        for (unsigned i = 0; i < width; ++i)
            projection.push_back(static_cast<std::uint8_t>(n >> (8 * i)));
    };
    const std::string domain = "axiom.semantic.projection.v1";
    number(domain.size(), 4);
    projection.insert(projection.end(), domain.begin(), domain.end());
    projection.insert(
        projection.end(), s.document_id.value().bytes.begin(), s.document_id.value().bytes.end());
    number(3, 4);
    number(s.capability_requirements.size(), 4);
    projection.insert(
        projection.end(), s.capability_requirements.begin(), s.capability_requirements.end());
    number(1, 1);
    number(0, 8);
    number(1, 4);
    number(0, 8);
    number(0, 8);
    number(0, 8);
    const auto digest = internal::sha256(projection);
    s.document_digest.assign(digest.begin(), digest.end());
    p::DocumentSnapshot wire;
    wire.mutable_document_id()->set_value(
        std::string(reinterpret_cast<const char*>(id(100).bytes.data()), 16));
    wire.set_schema_version(3);
    wire.mutable_manifest()->set_schema_version(1);
    wire.mutable_manifest()->set_manifest_revision(0);
    wire.set_document_revision(0);
    wire.set_recovery_frontier(std::string(s.recovery_frontier.begin(), s.recovery_frontier.end()));
    wire.set_digest_algorithm(1);
    wire.set_document_digest(std::string(digest.begin(), digest.end()));
    wire.set_capability_requirements(
        std::string(s.capability_requirements.begin(), s.capability_requirements.end()));
    const auto encoded = internal::canonicalEncodeDocumentSnapshot(wire);
    ASSERT_TRUE(encoded.ok());
    std::vector<std::uint8_t> framed;
    const std::string checkpointDomain = "axiom.checkpoint.v1";
    const auto append = [&](std::uint64_t n, unsigned width) {
        for (unsigned i = 0; i < width; ++i)
            framed.push_back(static_cast<std::uint8_t>(n >> (8 * i)));
    };
    append(checkpointDomain.size(), 4);
    framed.insert(framed.end(), checkpointDomain.begin(), checkpointDomain.end());
    append(encoded.bytes.size(), 8);
    framed.insert(framed.end(), encoded.bytes.begin(), encoded.bytes.end());
    const auto cp = internal::sha256(framed);
    s.checkpoint_digest.assign(cp.begin(), cp.end());
    EXPECT_TRUE(SnapshotCodec::encode(s).ok());
}
struct HistoryPort final : canvas::interaction::HistorySubmitPort {
    OperationId allocateOperationId() override {
        return OperationId{id(99)};
    }
    canvas::interaction::SubmitResult submit(std::span<const Operation>, ApplySource) override {
        return {true};
    }
};
TEST(FC02History, TextCompensationIsSupported) {
    HistoryPort port;
    canvas::interaction::EditorHistory history(port);
    Operation forward{OperationId{id(1)},
                      DocumentId{id(100)},
                      1,
                      1,
                      EditRichTextOp{id(2), {1, {DeleteTextStep{id(3), 0, 1}}}}};
    auto inverse = forward;
    inverse.id = OperationId{id(4)};
    EXPECT_TRUE(history.record(forward, {inverse}));
}

ResourceManifest fontManifest() {
    ResourceManifest m;
    m.entries.push_back({ResourceId{id(90)},
                         ResourceKind::kFont,
                         1,
                         "sha256:" + std::string(64, '0'),
                         1,
                         1,
                         1,
                         {8, 1, 16, 0, 24, 1}});
    return m;
}
ObjectRecord textObject(bool fixed = true) {
    TextStyle style;
    style.font_resource_id = ResourceId{id(90)};
    style.font_size = 16;
    style.weight = 400;
    style.color = {0, 0, 0, 1};
    RichTextContent content;
    content.document.paragraphs = {
        {id(31), {ParagraphAlignment::kLeft, 1, 0, 0}, {{"A", style}, {"B", style}}}};
    if (fixed)
        content.layout = RichTextLayout{320};
    ObjectRecord object;
    object.id = id(30);
    object.id.bytes[15] = 0x81;
    object.kind = ObjectKind::kRichText;
    object.kind_version = fixed ? 2U : 1U;
    object.placement.order_key = OrderKey({1});
    object.content = content;
    return object;
}
struct Engine {
    ReferenceObjectStore objects;
    ResourceManifest manifest = fontManifest();
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock clock{RuntimeEpoch{7}};
    OperationEngine engine;
    std::uint64_t next = 1000;
    Operation op(OperationPayload payload) {
        auto value = id(next++);
        value.bytes[14] = 0x80;
        return {OperationId{value}, DocumentId{id(100)}, 1, 1, std::move(payload)};
    }
    ApplyResult apply(const Operation& operation) {
        return engine.apply(operation,
                            ApplySource::kLocalInteraction,
                            objects,
                            manifest,
                            ledger,
                            generation,
                            clock);
    }
    Operation delta(ObjectId object, std::vector<RichTextStep> steps) {
        return op(
            EditRichTextOp{object,
                           {2,
                            std::move(steps),
                            richTextStateDigest(DocumentId{id(100)}, *objects.find(object))}});
    }
};
TEST(FC02Semantic, WidthCodecAndStrictPresenceRoundTrip) {
    Engine e;
    auto object = textObject();
    const auto operation = e.op(InsertObjectsOp{{object}});
    const auto bytes = SemanticCodec::encodeProtobufOperation(operation);
    ASSERT_TRUE(bytes.ok());
    const auto decoded = SemanticCodec::decodeProtobufOperation(bytes.bytes);
    ASSERT_TRUE(decoded.ok());
    EXPECT_EQ(std::get<InsertObjectsOp>(decoded.operation.payload).objects.front(), object);
    auto invalid = object;
    std::get<RichTextContent>(invalid.content).layout->wrap_width = 0;
    EXPECT_EQ(e.apply(e.op(InsertObjectsOp{{invalid}})).disposition, ApplyDisposition::kRejected);
    std::get<RichTextContent>(invalid.content).layout->wrap_width =
        std::numeric_limits<double>::infinity();
    EXPECT_EQ(e.apply(e.op(InsertObjectsOp{{invalid}})).disposition, ApplyDisposition::kRejected);
    invalid = object;
    invalid.kind_version = 1;
    EXPECT_EQ(e.apply(e.op(InsertObjectsOp{{invalid}})).disposition, ApplyDisposition::kRejected);
    EXPECT_EQ(e.objects.size(), 0U);
}
TEST(FC02Semantic, ExactRestorePreservesRunBoundariesAndUnrelatedTransform) {
    Engine e;
    const auto before = textObject(false);
    ASSERT_EQ(e.apply(e.op(InsertObjectsOp{{before}})).disposition, ApplyDisposition::kApplied);
    const auto style =
        std::get<RichTextContent>(before.content).document.paragraphs[0].runs[0].style;
    const auto forward = e.delta(before.id, {InsertTextStep{id(31), 1, "X", style}});
    ASSERT_EQ(e.apply(forward).disposition, ApplyDisposition::kApplied);
    EXPECT_EQ(e.apply(forward).disposition, ApplyDisposition::kAlreadyApplied);
    EXPECT_EQ(std::get<RichTextContent>(e.objects.find(before.id)->content)
                  .document.paragraphs[0]
                  .runs.size(),
              1U);
    auto transform = before.transform;
    transform.tx = 77;
    ASSERT_EQ(e.apply(e.op(SetTransformsOp{{{before.id, transform}}})).disposition,
              ApplyDisposition::kApplied);
    const auto undo = e.delta(
        before.id, {RestoreRichTextStateStep{{1, std::get<RichTextContent>(before.content)}}});
    const auto bytes = SemanticCodec::encodeProtobufOperation(undo);
    ASSERT_TRUE(bytes.ok());
    const auto decoded = SemanticCodec::decodeProtobufOperation(bytes.bytes);
    ASSERT_TRUE(decoded.ok());
    ASSERT_EQ(e.apply(decoded.operation).disposition, ApplyDisposition::kApplied);
    EXPECT_EQ(e.objects.find(before.id)->content, before.content);
    EXPECT_EQ(e.objects.find(before.id)->transform, transform);
}
TEST(FC02Semantic, GuardAndInvalidSecondStepRejectAtomically) {
    Engine e;
    auto object = textObject();
    ASSERT_EQ(e.apply(e.op(InsertObjectsOp{{object}})).disposition, ApplyDisposition::kApplied);
    const auto style =
        std::get<RichTextContent>(object.content).document.paragraphs[0].runs[0].style;
    auto bad =
        e.delta(object.id, {InsertTextStep{id(31), 1, "X", style}, DeleteTextStep{id(999), 0, 1}});
    const auto gen = e.generation.current();
    EXPECT_EQ(e.apply(bad).disposition, ApplyDisposition::kRejected);
    EXPECT_EQ(e.generation.current(), gen);
    EXPECT_EQ(*e.objects.find(object.id), object);
    bad = e.delta(object.id, {SetRichTextLayoutStep{2, RichTextLayout{200}}});
    std::get<EditRichTextOp>(bad.payload).delta.expected_state_digest[0] ^= 1;
    EXPECT_EQ(e.apply(bad).disposition, ApplyDisposition::kRejected);
    bad = e.delta(
        object.id,
        {SetRichTextLayoutStep{2, RichTextLayout{200}}, InsertTextStep{id(31), 0, "X", style}});
    EXPECT_EQ(e.apply(bad).disposition, ApplyDisposition::kRejected);
    auto wrong = e.delta(object.id, {SetRichTextLayoutStep{2, RichTextLayout{200}}});
    wrong.document_id = DocumentId{id(101)};
    EXPECT_EQ(e.apply(wrong).disposition, ApplyDisposition::kRejected);
    EXPECT_EQ(e.generation.current(), gen);
}
TEST(FC02Semantic, ExplicitMigrationAndInverseRestoreLegacyAbsence) {
    Engine e;
    auto object = textObject(false);
    ASSERT_EQ(e.apply(e.op(InsertObjectsOp{{object}})).disposition, ApplyDisposition::kApplied);
    ASSERT_EQ(
        e.apply(e.delta(object.id, {SetRichTextLayoutStep{2, RichTextLayout{123}}})).disposition,
        ApplyDisposition::kApplied);
    EXPECT_EQ(e.objects.find(object.id)->kind_version, 2U);
    ASSERT_EQ(e.apply(e.delta(object.id, {SetRichTextLayoutStep{1, std::nullopt}})).disposition,
              ApplyDisposition::kApplied);
    EXPECT_EQ(*e.objects.find(object.id), object);
    e.manifest.entries.clear();
    EXPECT_EQ(
        e.apply(e.delta(object.id, {SetRichTextLayoutStep{2, RichTextLayout{123}}})).disposition,
        ApplyDisposition::kRejected);
}
TEST(FC02Snapshot, MixedTriangleTextAndManifestRoundTripAndBoundRestore) {
    auto s = emptySnapshot();
    s.manifest = fontManifest();
    s.objects = {textObject(), textObject(false)};
    s.objects.back().id = id(32);
    s.objects.back().placement.order_key = OrderKey({2});
    ObjectRecord triangle;
    triangle.id = id(40);
    triangle.kind = ObjectKind::kShape;
    triangle.kind_version = 2;
    triangle.placement.order_key = OrderKey({3});
    triangle.content = ShapeContent{3, 100, 80};
    s.objects.push_back(triangle);
    StrokeRecord stroke;
    stroke.brush.brush_family_id = 1;
    stroke.brush.brush_version = 1;
    stroke.brush.color = {0, 0, 0, 1};
    stroke.brush.nominal_size = 2;
    stroke.brush.opacity = 1;
    stroke.data =
        VectorStrokeData{{StrokeSample{{10, 20}, 1, {0, 0}}, StrokeSample{{30, 40}, 1, {0, 0}}}};
    ObjectRecord ink;
    ink.id = id(41);
    ink.kind = ObjectKind::kVectorStroke;
    ink.kind_version = 1;
    ink.placement.order_key = OrderKey({4});
    ink.content = VectorStrokeContent{stroke};
    s.objects.push_back(ink);

    s.capability_requirements =
        caps({"resource-manifest-v1", "richtext-fixed-width-v2", "shape2-triangle-v1"});
    const auto both = SnapshotCapability::kShape2TriangleAndRichTextFixedWidthV2;
    ASSERT_TRUE(SnapshotCodec::bindDigests(s, both));
    EXPECT_FALSE(SnapshotCodec::encode(s).ok());
    EXPECT_FALSE(SnapshotCodec::encode(s, SnapshotCapability::kShape2Triangle).ok());
    const auto encoded = SnapshotCodec::encode(s, both);
    ASSERT_TRUE(encoded.ok());
    const auto decoded = SnapshotCodec::decode(encoded.bytes, both);
    ASSERT_TRUE(decoded.ok());
    SnapshotBootstrapper::CheckpointBinding expected{
        s.document_id, s.document_revision, s.recovery_frontier, s.checkpoint_digest};
    ReferenceObjectStore objects;
    ResourceManifest manifest;
    SemanticGenerationState gen;
    auto state = DocumentRuntimeState::kLoading;
    EXPECT_FALSE(
        SnapshotBootstrapper::restore(*decoded.snapshot, both, state, objects, manifest, gen)
            .restored);
    state = DocumentRuntimeState::kLoading;
    ASSERT_TRUE(SnapshotBootstrapper::restore(
                    *decoded.snapshot, both, state, objects, manifest, gen, expected)
                    .restored);
    EXPECT_EQ(objects.size(), 4U);
    EXPECT_EQ(manifest, s.manifest);
    EXPECT_EQ(gen.current().value(), 0U);
    IndexedObjectStore indexed;
    ResourceManifest indexedManifest;
    SemanticGenerationState indexedGen;
    auto indexedState = DocumentRuntimeState::kLoading;
    EXPECT_TRUE(
        SnapshotBootstrapper::restore(
            *decoded.snapshot, both, indexedState, indexed, indexedManifest, indexedGen, expected)
            .restored);
    auto bad = *decoded.snapshot;
    bad.document_digest[0] ^= 1;
    ReferenceObjectStore rejected;
    ResourceManifest rm;
    SemanticGenerationState rg;
    auto rs = DocumentRuntimeState::kLoading;
    EXPECT_FALSE(SnapshotBootstrapper::restore(bad, both, rs, rejected, rm, rg, expected).restored);
    EXPECT_EQ(rejected.size(), 0U);
    bad = *decoded.snapshot;
    bad.capability_requirements = caps({"resource-manifest-v1", "shape2-triangle-v1"});
    EXPECT_FALSE(SnapshotCodec::bindDigests(bad, both));
}

struct RealHost {
    canvas::runtime::RuntimeComposition composition;
    canvas::runtime::RuntimeFacade& facade = composition.controlFacade();
    std::uint64_t request = 0, sequence = 0;
    RealHost() {
        if (!composition.open() || !composition.bindSurface(256, 256) ||
            !composition.bindManifest(fontManifest(), 1))
            throw std::runtime_error("host setup");
    }
    canvas::runtime::TextReceipt intent(canvas::runtime::TextIntentKind kind, double width = 320) {
        canvas::runtime::TextIntent in;
        in.target = facade.mountedCanvasTargets().front();
        in.clientId = 7;
        in.requestId = ++request;
        in.kind = kind;
        in.token = facade.readTextEditor().token;
        in.width = width;
        in.viewX = 32;
        in.viewY = 48;
        const auto queued = facade.submitTextIntent(in);
        if (queued.status != canvas::runtime::TextStatus::kQueued)
            return queued;
        if (!composition.ownerPump())
            throw std::runtime_error("owner pump");
        return *facade.textIntentReceipt(7, request);
    }
    canvas::runtime::TextInputEvent event(canvas::runtime::TextInputKind kind,
                                          std::u16string text = {},
                                          canvas::runtime::TextRange range = {},
                                          std::uint64_t compositionId = 0) {
        auto snapshot = facade.readTextEditor();
        canvas::runtime::TextInputEvent in;
        in.token = snapshot.token;
        in.sequence = ++sequence;
        in.kind = kind;
        in.text = std::move(text);
        in.range = range;
        in.composition = (kind == canvas::runtime::TextInputKind::kReplace ||
                          kind == canvas::runtime::TextInputKind::kSelection)
                             ? 0
                         : compositionId ? compositionId
                                         : 1;
        in.finishEffect =
            kind == canvas::runtime::TextInputKind::kCommit ? snapshot.finishEffect : 0;
        return in;
    }
    canvas::runtime::TextReceipt send(canvas::runtime::TextInputEvent in) {
        const auto result = facade.submitTextInput(in);
        if (result.status != canvas::runtime::TextStatus::kQueued)
            return result;
        if (!composition.ownerPump())
            throw std::runtime_error("owner input pump");
        return *facade.textInputReceipt(in.token, in.sequence);
    }
    std::size_t operations() {
        return composition.qualificationRuntime().submittedOperationCount();
    }
};
TEST(FC02Facade, FakeImeCommitsThroughRealHistoryAndCanonicalStore) {
    using namespace canvas::runtime;
    RealHost h;
    ASSERT_EQ(h.intent(TextIntentKind::kCreate).status, TextStatus::kApplied);
    EXPECT_EQ(h.composition.semanticObjectCount(), 0U);
    EXPECT_EQ(h.operations(), 0U);
    EXPECT_NE(h.facade.readTextEditor().token.object.bytes[15], 0);
    ASSERT_EQ(h.send(h.event(TextInputKind::kBegin)).status, TextStatus::kObserved);
    for (const auto* value : {u"n", u"ni", u"\u4F60"})
        EXPECT_EQ(h.send(h.event(TextInputKind::kUpdate, value)).status, TextStatus::kObserved);
    EXPECT_EQ(h.operations(), 0U);
    const auto terminal = h.event(TextInputKind::kCommit, u"\u4F60");
    ASSERT_EQ(h.facade.submitTextInput(terminal).status, TextStatus::kQueued);
    EXPECT_EQ(h.operations(), 0U);
    ASSERT_TRUE(h.composition.ownerPump());
    const auto receipt = h.facade.textInputReceipt(terminal.token, terminal.sequence);
    ASSERT_TRUE(receipt);
    ASSERT_EQ(receipt->status, TextStatus::kApplied);
    EXPECT_FALSE(receipt->operation.isZero());
    EXPECT_EQ(h.operations(), 1U);
    EXPECT_EQ(h.composition.semanticObjectCount(), 1U);
    EXPECT_EQ(h.facade.submitTextInput(terminal).status, TextStatus::kApplied);
    EXPECT_EQ(h.operations(), 1U);
    EXPECT_EQ(h.facade.readTextEditor().projection, u"\u4F60");
    ASSERT_EQ(h.intent(TextIntentKind::kFinish).status, TextStatus::kApplied);
    const auto target = h.facade.mountedCanvasTargets().front();
    ASSERT_EQ(h.facade.undo(1, target.runtimeEpoch, 20).state, ProductControlState::kQueued);
    ASSERT_TRUE(h.composition.ownerPump());
    EXPECT_EQ(h.composition.semanticObjectCount(), 0U);
    ASSERT_EQ(h.facade.redo(2, target.runtimeEpoch, 20).state, ProductControlState::kQueued);
    ASSERT_TRUE(h.composition.ownerPump());
    EXPECT_EQ(h.composition.semanticObjectCount(), 1U);
}
TEST(FC02Facade, ToolSwitchWaitsForRealImeCommitAndResizeKeepsSession) {
    using namespace canvas::runtime;
    RealHost h;
    ASSERT_EQ(h.intent(TextIntentKind::kCreate).status, TextStatus::kApplied);
    ASSERT_EQ(h.send(h.event(TextInputKind::kBegin)).status, TextStatus::kObserved);
    const auto token = h.facade.readTextEditor().token;
    ASSERT_TRUE(h.composition.resizeSurface(300, 220));
    EXPECT_EQ(h.facade.readTextEditor().token, token);
    const auto target = h.facade.mountedCanvasTargets().front();
    ASSERT_EQ(h.facade.setTool(4102, 1, target.runtimeEpoch, 30).state,
              ProductControlState::kQueued);
    ASSERT_TRUE(h.composition.ownerPump());
    EXPECT_NE(h.facade.readTextEditor().finishEffect, 0U);
    EXPECT_FALSE(h.composition.qualificationRuntime().selectionMode());
    EXPECT_EQ(h.operations(), 0U);
    ASSERT_EQ(h.send(h.event(TextInputKind::kCommit, u"X")).status, TextStatus::kApplied);
    EXPECT_TRUE(h.composition.qualificationRuntime().selectionMode());
    EXPECT_EQ(h.operations(), 1U);
    EXPECT_EQ(h.facade.readTextEditor().phase, TextPhase::kClosed);
}
TEST(FC02Facade, CrossParagraphReplacementAndWidthUndoAreCanonical) {
    using namespace canvas::runtime;
    RealHost h;
    ASSERT_EQ(h.intent(TextIntentKind::kCreate).status, TextStatus::kApplied);
    ASSERT_EQ(h.send(h.event(TextInputKind::kReplace, u"A\U0001F600B\nCD")).status,
              TextStatus::kApplied);
    ASSERT_EQ(h.send(h.event(TextInputKind::kReplace, u"X\nY", {3, 3})).status,
              TextStatus::kApplied);
    EXPECT_EQ(h.facade.readTextEditor().projection, u"A\U0001F600X\nYD");
    ASSERT_EQ(h.intent(TextIntentKind::kSetWidth, 150).status, TextStatus::kApplied);
    EXPECT_DOUBLE_EQ(h.facade.readTextEditor().width, 150);
    const auto target = h.facade.mountedCanvasTargets().front();
    ASSERT_EQ(h.facade.undo(1, target.runtimeEpoch, 31).state, ProductControlState::kQueued);
    ASSERT_TRUE(h.composition.ownerPump());
    EXPECT_DOUBLE_EQ(h.facade.readTextEditor().width, 320);
    ASSERT_EQ(h.facade.undo(2, target.runtimeEpoch, 31).state, ProductControlState::kQueued);
    ASSERT_TRUE(h.composition.ownerPump());
    EXPECT_EQ(h.facade.readTextEditor().projection, u"A\U0001F600B\nCD");
}
TEST(FC02Facade, InvalidOffsetsCancellationAndEmptyDraftLeaveNoObjects) {
    using namespace canvas::runtime;
    RealHost h;
    ASSERT_EQ(h.intent(TextIntentKind::kCreate).status, TextStatus::kApplied);
    ASSERT_EQ(h.send(h.event(TextInputKind::kBegin)).status, TextStatus::kObserved);
    ASSERT_EQ(h.send(h.event(TextInputKind::kUpdate, u"X")).status, TextStatus::kObserved);
    ASSERT_EQ(h.send(h.event(TextInputKind::kCancel)).status, TextStatus::kObserved);
    EXPECT_EQ(h.intent(TextIntentKind::kFinish).status, TextStatus::kApplied);
    EXPECT_EQ(h.composition.semanticObjectCount(), 0U);
    h.sequence = 0;
    ASSERT_EQ(h.intent(TextIntentKind::kCreate).status, TextStatus::kApplied);
    auto wrong = h.event(TextInputKind::kReplace, u"X");
    wrong.token.object.bytes[15] ^= 1;
    EXPECT_EQ(h.send(wrong).status, TextStatus::kStale);
    h.sequence = 0;
    auto invalid = h.event(TextInputKind::kReplace, std::u16string(1, 0xD800));
    EXPECT_EQ(h.send(invalid).status, TextStatus::kRejected);
    EXPECT_EQ(h.operations(), 0U);
    ASSERT_EQ(h.send(h.event(TextInputKind::kReplace, u"\U0001F600")).status, TextStatus::kApplied);
    EXPECT_EQ(h.send(h.event(TextInputKind::kReplace, u"X", {1, 0})).status, TextStatus::kRejected);
    EXPECT_EQ(h.operations(), 1U);
}
TEST(FC02Facade, PublicationReadCannotInvalidateOwnFinishingComposition) {
    using namespace canvas::runtime;
    RealHost h;
    ASSERT_EQ(h.intent(TextIntentKind::kCreate).status, TextStatus::kApplied);
    ASSERT_EQ(h.send(h.event(TextInputKind::kReplace, u"A")).status, TextStatus::kApplied);
    ASSERT_EQ(h.send(h.event(TextInputKind::kBegin, {}, {1, 0})).status, TextStatus::kObserved);
    ASSERT_EQ(h.intent(TextIntentKind::kFinish).status, TextStatus::kQueued);
    struct Sink final : canvas::interaction::LocalOperationSink {
        RuntimeFacade& facade;
        TextEditorSnapshot observed{};
        explicit Sink(RuntimeFacade& value) : facade(value) {}
        void applied(const Operation&, const CanonicalCommitRecord&) noexcept override {
            observed = facade.readTextEditor();
        }
        canvas::interaction::AxisOperationStatus
        status(const OperationId&, const CanonicalCommitStamp&) const noexcept override {
            return {};
        }
    } sink(h.facade);
    h.composition.qualificationRuntime().setLocalOperationSink(&sink);
    EXPECT_EQ(h.send(h.event(TextInputKind::kCommit, u"B", {1, 0})).status, TextStatus::kApplied);
    h.composition.qualificationRuntime().setLocalOperationSink(nullptr);
    EXPECT_EQ(sink.observed.phase, TextPhase::kFinishing);
    EXPECT_EQ(sink.observed.projection, u"A");
    EXPECT_EQ(h.facade.readTextEditor().phase, TextPhase::kClosed);
    EXPECT_EQ(h.facade.textIntentReceipt(7, h.request)->status, TextStatus::kApplied);
    EXPECT_FALSE(h.facade.textIntentReceipt(7, h.request)->operation.isZero());
    EXPECT_EQ(h.operations(), 2U);
}
TEST(FC02Facade, TextOwnershipConsumesPointerLifecycleWithoutStartingInk) {
    using namespace canvas::runtime;
    RealHost h;
    ASSERT_EQ(h.intent(TextIntentKind::kCreate).status, TextStatus::kApplied);
    const auto pointer = [](std::uint64_t sequence, canvas::input::PointerPhase phase) {
        canvas::input::PlatformPointerBatch b;
        b.samples.push_back({17,
                             9,
                             sequence,
                             sequence * 1000,
                             static_cast<float>(sequence * 20),
                             20,
                             0,
                             0,
                             0,
                             {},
                             {},
                             canvas::input::SampleProvenance::kConfirmedCurrent,
                             phase});
        return b;
    };
    ASSERT_TRUE(h.composition.submit(pointer(1, canvas::input::PointerPhase::kDown)));
    const auto old = h.composition.platformKey(17, 9);
    ASSERT_TRUE(old);
    ASSERT_TRUE(h.composition.submit(pointer(2, canvas::input::PointerPhase::kUp)));
    EXPECT_EQ(h.operations(), 0U);
    EXPECT_EQ(h.composition.semanticObjectCount(), 0U);
    ASSERT_EQ(h.intent(TextIntentKind::kFinish).status, TextStatus::kApplied);
    ASSERT_TRUE(h.composition.submit(pointer(3, canvas::input::PointerPhase::kDown)));
    const auto next = h.composition.platformKey(17, 9);
    ASSERT_TRUE(next);
    EXPECT_NE(next->generation, old->generation);
    EXPECT_TRUE(h.composition.cancelAllPointers());
}
TEST(FC02Facade, FailedTerminalKeepsPreimageAndRetryDoesNotRewriteRejection) {
    using namespace canvas::runtime;
    RealHost h;
    ASSERT_EQ(h.intent(TextIntentKind::kCreate).status, TextStatus::kApplied);
    ASSERT_EQ(h.send(h.event(TextInputKind::kBegin)).status, TextStatus::kObserved);
    ASSERT_EQ(h.intent(TextIntentKind::kFinish).status, TextStatus::kQueued);
    ASSERT_TRUE(h.composition.loseSurface());
    const auto terminal = h.event(TextInputKind::kCommit, u"X");
    ASSERT_EQ(h.send(terminal).status, TextStatus::kRejected);
    const auto originalFinish = h.request;
    EXPECT_EQ(h.facade.textIntentReceipt(7, originalFinish)->status, TextStatus::kRejected);
    EXPECT_EQ(h.facade.readTextEditor().projection, u"");
    EXPECT_EQ(h.operations(), 0U);
    auto repeat = terminal;
    repeat.sequence = ++h.sequence;
    EXPECT_EQ(h.send(repeat).status, TextStatus::kRejected);
    EXPECT_EQ(h.intent(TextIntentKind::kFinish).status, TextStatus::kRejected);
    ASSERT_TRUE(h.composition.rebindSurface());
    EXPECT_EQ(h.intent(TextIntentKind::kRetry).status, TextStatus::kApplied);
    EXPECT_EQ(h.operations(), 1U);
    EXPECT_EQ(h.facade.readTextEditor().phase, TextPhase::kClosed);
    EXPECT_EQ(h.facade.textInputReceipt(terminal.token, terminal.sequence)->status,
              TextStatus::kRejected);
    EXPECT_EQ(h.facade.textIntentReceipt(7, originalFinish)->status, TextStatus::kRejected);
}

TEST(FC02Semantic, WirePresenceAndDecodedSemanticRejectionsLeaveStateUntouched) {
    Engine e;
    const auto object = textObject();
    ASSERT_EQ(e.apply(e.op(InsertObjectsOp{{object}})).disposition, ApplyDisposition::kApplied);
    const auto encoded = SemanticCodec::encodeProtobufOperation(
        e.delta(object.id, {SetRichTextLayoutStep{2, RichTextLayout{100}}}));
    ASSERT_TRUE(encoded.ok());
    p::Operation wire;
    ASSERT_TRUE(wire.ParseFromArray(encoded.bytes.data(), static_cast<int>(encoded.bytes.size())));
    const auto rejected = [&](const p::Operation& value) {
        const auto bytes = value.SerializeAsString();
        const auto decoded = SemanticCodec::decodeProtobufOperation({bytes.begin(), bytes.end()});
        return !decoded.ok() ||
               e.apply(decoded.operation).disposition == ApplyDisposition::kRejected;
    };
    auto bad = wire;
    bad.mutable_payload()->mutable_edit_rich_text()->mutable_delta()->clear_expected_state_digest();
    EXPECT_TRUE(rejected(bad));
    bad = wire;
    bad.mutable_payload()
        ->mutable_edit_rich_text()
        ->mutable_delta()
        ->mutable_steps(0)
        ->mutable_set_layout()
        ->mutable_layout()
        ->clear_wrap_width();
    EXPECT_TRUE(rejected(bad));
    bad = wire;
    bad.mutable_payload()->mutable_edit_rich_text()->mutable_delta()->set_delta_version(99);
    EXPECT_TRUE(rejected(bad));
    bad = wire;
    auto* step = bad.mutable_payload()->mutable_edit_rich_text()->mutable_delta()->mutable_steps(0);
    step->clear_step();
    step->GetReflection()->MutableUnknownFields(step)->AddLengthDelimited(9, "");
    EXPECT_TRUE(rejected(bad));
    auto legacy = e.op(EditRichTextOp{object.id, {1, {DeleteTextStep{id(31), 0, 1}}}});
    EXPECT_EQ(e.apply(legacy).disposition, ApplyDisposition::kRejected);
    auto other = object;
    other.id.bytes[15] ^= 1;
    EXPECT_NE(richTextStateDigest(DocumentId{id(100)}, object),
              richTextStateDigest(DocumentId{id(100)}, other));
}
TEST(FC02Snapshot, LegacyTypedRestoreCannotBypassTextCapability) {
    SemanticSnapshot s;
    s.document_id = DocumentId{id(100)};
    s.schema_version = 1;
    s.objects = {textObject()};
    ReferenceObjectStore store;
    SemanticGenerationState gen;
    auto state = DocumentRuntimeState::kLoading;
    EXPECT_FALSE(SnapshotBootstrapper::restore(s, SnapshotCapability::kLegacyV1, state, store, gen)
                     .restored);
    EXPECT_EQ(store.size(), 0U);
    auto valid = emptySnapshot();
    valid.manifest = fontManifest();
    valid.objects = {textObject()};
    valid.capability_requirements = caps({"resource-manifest-v1", "richtext-fixed-width-v2"});
    ASSERT_TRUE(SnapshotCodec::bindDigests(valid, SnapshotCapability::kRichTextFixedWidthV2));
    for (const auto& invalidCaps :
         {caps({"richtext-fixed-width-v2", "resource-manifest-v1"}),
          caps({"resource-manifest-v1", "richtext-fixed-width-v2", "richtext-fixed-width-v2"}),
          caps({"resource-manifest-v1", "richtext-fixed-width-v2", "unknown"})}) {
        auto invalid = valid;
        invalid.capability_requirements = invalidCaps;
        EXPECT_FALSE(
            SnapshotCodec::bindDigests(invalid, SnapshotCapability::kRichTextFixedWidthV2));
    }
}
TEST(FC02Facade, AcknowledgedAndOldSessionEventsCannotReapplyAndFailedWidthPreservesRedo) {
    using namespace canvas::runtime;
    RealHost h;
    ASSERT_EQ(h.intent(TextIntentKind::kCreate).status, TextStatus::kApplied);
    const auto first = h.event(TextInputKind::kReplace, u"A");
    ASSERT_EQ(h.send(first).status, TextStatus::kApplied);
    ASSERT_TRUE(h.facade.acknowledgeTextInput(first.token, first.sequence));
    EXPECT_EQ(h.send(first).status, TextStatus::kExpired);
    EXPECT_EQ(h.operations(), 1U);
    ASSERT_EQ(h.intent(TextIntentKind::kSetWidth, 120).status, TextStatus::kApplied);
    const auto target = h.facade.mountedCanvasTargets().front();
    ASSERT_EQ(h.facade.undo(1, target.runtimeEpoch, 90).state, ProductControlState::kQueued);
    ASSERT_TRUE(h.composition.ownerPump());
    const auto before = h.operations();
    EXPECT_EQ(h.intent(TextIntentKind::kSetWidth, 0).status, TextStatus::kRejected);
    EXPECT_EQ(h.operations(), before);
    ASSERT_EQ(h.facade.redo(2, target.runtimeEpoch, 90).state, ProductControlState::kQueued);
    ASSERT_TRUE(h.composition.ownerPump());
    EXPECT_EQ(h.facade.readTextEditor().width, 120);
    ASSERT_EQ(h.intent(TextIntentKind::kFinish).status, TextStatus::kApplied);
    h.sequence = 0;
    ASSERT_EQ(h.intent(TextIntentKind::kCreate).status, TextStatus::kApplied);
    auto old = first;
    old.sequence = 99;
    EXPECT_EQ(h.send(old).status, TextStatus::kStale);
    EXPECT_EQ(h.send(h.event(TextInputKind::kReplace, u"B")).status, TextStatus::kApplied);
    EXPECT_EQ(h.composition.semanticObjectCount(), 2U);
}

TEST(FC02Facade, ForcedCloseRejectsQueuedInputAndLateCallbacks) {
    using namespace canvas::runtime;
    RealHost h;
    ASSERT_EQ(h.intent(TextIntentKind::kCreate).status, TextStatus::kApplied);
    auto pending = h.event(TextInputKind::kReplace, u"unsent");
    ASSERT_EQ(h.facade.submitTextInput(pending).status, TextStatus::kQueued);
    ASSERT_TRUE(h.composition.close());
    EXPECT_EQ(h.facade.readTextEditor().phase, TextPhase::kClosed);
    EXPECT_EQ(h.facade.textInputReceipt(pending.token, pending.sequence)->status,
              TextStatus::kRejected);
    pending.sequence++;
    EXPECT_EQ(h.facade.submitTextInput(pending).status, TextStatus::kStale);
    EXPECT_EQ(h.operations(), 0U);
}
} // namespace
