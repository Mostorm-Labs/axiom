#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/normalizer.hpp"
#include "canvas/semantic/snapshot.hpp"
#include <gtest/gtest.h>
namespace {
using namespace canvas::semantic;
ObjectRecord sketchObject() {
    ObjectRecord o;
    o.id = ObjectId::fromUint64(10);
    o.kind = ObjectKind::kShape;
    o.kind_version = 1;
    o.placement.order_key = OrderKey({128});
    o.content = ShapeContent{1, 120, 80};
    o.properties.entries = {{0x102U, SketchStyleV1{1, 9223372036854775809ULL, 0, true}}};
    return o;
}
Operation op(ObjectRecord o) {
    return {OperationId{ObjectId::fromUint64(2)}, DocumentId{ObjectId::fromUint64(1)}, 1, 1,
            InsertObjectsOp{{o}}};
}
} // namespace
TEST(SketchSemantic, ZeroRoughnessRetainsSeedAndRoundTrips) {
    auto input = op(sketchObject());
    auto normalized = normalizeOperation(input);
    ASSERT_TRUE(normalized.ok());
    EXPECT_EQ(normalized.value.payload, input.payload);
    auto wire = SemanticCodec::encodeProtobufOperation(input);
    ASSERT_TRUE(wire.ok());
    auto decoded = SemanticCodec::decodeProtobufOperation(wire.bytes);
    ASSERT_TRUE(decoded.ok());
    EXPECT_EQ(decoded.operation.payload, input.payload);
}
TEST(SketchSemantic, RejectsUnknownVersionAndNonFiniteOrOutOfRange) {
    for (int i = 0; i < 5; ++i) {
        auto o = sketchObject();
        auto &s = std::get<SketchStyleV1>(o.properties.entries[0].value);
        if (i == 0)
            s.version = 2;
        if (i == 1)
            s.seed = 0;
        if (i == 2)
            s.roughness = -.1;
        if (i == 3)
            s.roughness = 3.01;
        if (i == 4)
            s.roughness = std::numeric_limits<double>::quiet_NaN();
        EXPECT_FALSE(SemanticCodec::encodeProtobufOperation(op(o)).ok()) << i;
    }
}
TEST(SketchSemantic, PatternRoundtripAndApplicability) {
    auto o = sketchObject();
    o.properties.entries.insert(
        o.properties.entries.begin(),
        {0x100U,
         FillStyleValue{PatternFillV1{1, {1, 0, 0, 1}, PatternKind::kCrosshatch, -45, 8, 1}}});
    auto wire = SemanticCodec::encodeProtobufOperation(op(o));
    ASSERT_TRUE(wire.ok());
    auto decoded = SemanticCodec::decodeProtobufOperation(wire.bytes);
    ASSERT_TRUE(decoded.ok());
    EXPECT_EQ(decoded.operation.payload, op(o).payload);
    o.kind = ObjectKind::kVectorPath;
    o.content = VectorPathContent{VectorPathGeometry{
        FillRule::kNonZero, {MoveTo{{0, 0}}, LineTo{{120, 0}}, LineTo{{0, 80}}, ClosePath{}}}};
    o.properties.entries.erase(o.properties.entries.begin() + 1);
    const auto pattern = o.properties.entries[0].value;
    o.properties.entries[0].value = FillStyleValue{SolidFill{{1, 0, 0, 1}}};
    ASSERT_TRUE(SemanticCodec::encodeProtobufOperation(op(o)).ok());
    o.properties.entries[0].value = pattern;
    EXPECT_TRUE(SemanticCodec::encodeProtobufOperation(op(o)).ok());
    o.kind=ObjectKind::kConnector;o.kind_version=1;o.content=ConnectorContent{{FreePointEndpoint{{0,0}}},{FreePointEndpoint{{100,100}}},ConnectorRouting::kStraight};
    EXPECT_FALSE(SemanticCodec::encodeProtobufOperation(op(o)).ok());
}
#include "canvas/runtime/runtime_composition.hpp"
TEST(SketchRuntime, PreviewIsTransientAndCommitIsOneHistoryEntry) {
    using namespace canvas;
    runtime::RuntimeComposition owner;
    ASSERT_TRUE(owner.openDocument(ObjectId::fromUint64(1)));
    ASSERT_TRUE(owner.bindSurface(800, 600));
    auto &api = owner.controlFacade();
    ASSERT_TRUE(
        api.setMultiSelectionPermission(runtime::MultiSelectionPermissionMode::kLocalSingleUser));
    runtime::StickyCreateRequest sticky;
    sticky.objectId = ObjectId::fromUint64(100);
    sticky.content.width = 160;
    sticky.content.height = 100;
    ASSERT_EQ(api.createSticky(sticky).state, runtime::ConnectorCommandState::kApplied);
    runtime::CanvasControlRequest select;
    select.target = api.mountedCanvasTargets().front();
    select.clientId = 99;
    select.requestId = 1;
    select.payload.kind = runtime::CanvasControlPayloadKind::kSelectTool;
    select.payload.tool = runtime::CanvasToolKind::kSelect;
    ASSERT_EQ(api.submitCanvasControl(select).state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(owner.ownerPump());
    ASSERT_TRUE(owner.registerSource(1));
    input::PlatformPointerSample sample;
    sample.source = 1;
    sample.pointer = 1;
    sample.sequence = 1;
    sample.timestampNs = 1;
    sample.x = 80;
    sample.y = 50;
    sample.phase = input::PointerPhase::kDown;
    input::PlatformPointerBatch batch;
    batch.samples.push_back(sample);
    auto key = owner.beginContact(batch);
    ASSERT_TRUE(key);
    sample.phase = input::PointerPhase::kUp;
    sample.sequence = 2;
    sample.timestampNs = 2;
    batch.samples[0] = sample;
    ASSERT_TRUE(owner.submitContact(*key, batch));
    ASSERT_EQ(api.readSelectionSummary().primary, sticky.objectId);
    auto before = api.readAppearance();
    ASSERT_TRUE(before.writable);
    runtime::AppearanceRequest r;
    r.action = runtime::AppearanceAction::kBegin;
    r.stamp = before.stamp;
    r.eligible = before.eligible;
    auto begin = api.appearanceCommand(r);
    ASSERT_TRUE(begin.applied);
    r.action = runtime::AppearanceAction::kPreview;
    r.token = begin.token;
    r.patch.enabled = true;
    r.patch.roughness = 1.5;
    ASSERT_TRUE(api.appearanceCommand(r).applied);
    EXPECT_EQ(api.readAppearance().stamp.semanticGeneration, before.stamp.semanticGeneration);
    EXPECT_FALSE(api.readAppearance().values.enabled);
    r.action = runtime::AppearanceAction::kCommit;
    r.patch = {};
    auto commit = api.appearanceCommand(r);
    ASSERT_TRUE(commit.applied);
    ASSERT_TRUE(commit.changed);
    auto after = api.readAppearance();
    EXPECT_TRUE(after.values.enabled);
    EXPECT_DOUBLE_EQ(after.values.roughness, 1.5);
    EXPECT_NE(after.values.seed, 0);
    EXPECT_EQ(after.stamp.semanticGeneration, before.stamp.semanticGeneration + 1);
    runtime::DocumentSnapshotRequest capture;
    capture.expectedSemanticGeneration = after.stamp.semanticGeneration;
    capture.documentRevision = 7;
    capture.recoveryFrontier = {8, 1, 16, 1, 24, 7};
    const auto saved = owner.captureDocumentSnapshot(capture);
    ASSERT_EQ(saved.status, runtime::DocumentSnapshotStatus::kCaptured);
    EXPECT_FALSE(SnapshotCodec::decode(saved.bytes, SnapshotCapability::kAllWithTableV1).ok());
    const auto decoded = SnapshotCodec::decode(saved.bytes, SnapshotCapability::kAllWithSketchV1);
    ASSERT_TRUE(decoded.ok());
    const auto &restored = decoded.snapshot->objects.front();
    const auto entry =
        std::find_if(restored.properties.entries.begin(), restored.properties.entries.end(),
                     [](const auto &e) { return e.field_id == 0x102U; });
    ASSERT_NE(entry, restored.properties.entries.end());
    EXPECT_EQ(std::get<SketchStyleV1>(entry->value).seed, after.values.seed);

    auto history = [&](bool undo, int seq) {
        runtime::CanvasControlRequest h;
        h.target = api.mountedCanvasTargets().front();
        h.clientId = 98;
        h.requestId = seq;
        h.payload.kind = undo ? runtime::CanvasControlPayloadKind::kUndo
                              : runtime::CanvasControlPayloadKind::kRedo;
        ASSERT_EQ(api.submitCanvasControl(h).state, runtime::ProductControlState::kQueued);
        ASSERT_TRUE(owner.ownerPump());
        ASSERT_EQ(api.canvasControlReceipt({98, static_cast<std::uint64_t>(seq)})->state,
                  runtime::CanvasControlReceiptState::kApplied);
    };
    history(true, 1);
    EXPECT_FALSE(api.readAppearance().values.enabled);
    history(false, 2);
    EXPECT_EQ(api.readAppearance().values.seed, after.values.seed);
    r = {};
    r.action = runtime::AppearanceAction::kPatch;
    r.stamp = api.readAppearance().stamp;
    r.eligible = after.eligible;
    r.patch.enabled = false;
    ASSERT_TRUE(api.appearanceCommand(r).applied);
    EXPECT_EQ(api.readAppearance().values.seed, after.values.seed);
    r.stamp = api.readAppearance().stamp;
    r.patch.enabled = true;
    ASSERT_TRUE(api.appearanceCommand(r).applied);
    EXPECT_EQ(api.readAppearance().values.seed, after.values.seed);
    const Operation remoteStyle{
        OperationId{ObjectId::fromUint64(999)}, DocumentId{ObjectId::fromUint64(1)}, 1, 1,
        PatchPropertiesOp{{{sticky.objectId, 0x102U, PropertyPatchAction::kSet,
                            PropertyValue{SketchStyleV1{1, after.values.seed, 2.75, true}}}}}};
    ASSERT_EQ(owner.applyRemoteOperation(remoteStyle).disposition, ApplyDisposition::kApplied);
    const auto remoteGeneration = api.readAppearance().stamp.semanticGeneration;
    runtime::CanvasControlRequest guardedUndo;
    guardedUndo.target = api.mountedCanvasTargets().front();
    guardedUndo.clientId = 97;
    guardedUndo.requestId = 1;
    guardedUndo.payload.kind = runtime::CanvasControlPayloadKind::kUndo;
    ASSERT_EQ(api.submitCanvasControl(guardedUndo).state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(owner.ownerPump());
    EXPECT_EQ(api.canvasControlReceipt({97, 1})->state,
              runtime::CanvasControlReceiptState::kFailed);
    EXPECT_EQ(api.readAppearance().stamp.semanticGeneration, remoteGeneration);
    EXPECT_DOUBLE_EQ(api.readAppearance().values.roughness, 2.75);
    auto live = api.readAppearance();
    r = {};
    r.action = runtime::AppearanceAction::kBegin;
    r.stamp = live.stamp;
    r.eligible = live.eligible;
    auto editing = api.appearanceCommand(r);
    ASSERT_TRUE(editing.applied);
    r.action = runtime::AppearanceAction::kPreview;
    r.token = editing.token;
    r.patch.roughness = 3;
    ASSERT_TRUE(api.appearanceCommand(r).applied);
    ASSERT_TRUE(owner.close());
    r.action = runtime::AppearanceAction::kCommit;
    r.patch = {};
    EXPECT_FALSE(api.appearanceCommand(r).applied);
}
TEST(SketchRuntime, DefaultsRetainDisabledPaintPreferencesWithoutDocumentHistory) {
    using namespace canvas;
    runtime::RuntimeComposition owner;
    ASSERT_TRUE(owner.openDocument(ObjectId::fromUint64(1)));
    ASSERT_TRUE(owner.bindSurface(800, 600));
    auto &api = owner.controlFacade();
    runtime::AppearanceRequest r;
    r.action = runtime::AppearanceAction::kDefaults;
    r.patch.fill = ColorValue{1, 0, 0, 1};
    r.patch.stroke = ColorValue{0, 1, 0, 1};
    r.patch.strokeWidth = 5;
    r.patch.gap = 12;
    ASSERT_TRUE(api.appearanceCommand(r).applied);
    EXPECT_EQ(api.readAppearance(true).values.gap, 12);
    r.patch = {};
    r.patch.fillKind = runtime::AppearanceFill::kNone;
    r.patch.strokeEnabled = false;
    ASSERT_TRUE(api.appearanceCommand(r).applied);
    r.patch = {};
    r.patch.fillKind = runtime::AppearanceFill::kSolid;
    r.patch.strokeEnabled = true;
    ASSERT_TRUE(api.appearanceCommand(r).applied);
    const auto values = api.readAppearance(true).values;
    EXPECT_EQ(values.fill, (ColorValue{1, 0, 0, 1}));
    EXPECT_EQ(values.stroke, (ColorValue{0, 1, 0, 1}));
    EXPECT_EQ(values.strokeWidth, 5);
    EXPECT_EQ(values.gap, 12);
    EXPECT_FALSE(api.readRuntimeState().history.canUndo);
}
TEST(SketchRuntime, OneRemotelyLockedTargetRejectsEntireAppearanceBatch) {
    using namespace canvas;
    runtime::RuntimeComposition owner;
    ASSERT_TRUE(owner.openDocument(ObjectId::fromUint64(1)));
    ASSERT_TRUE(owner.bindSurface(800, 600));
    auto &api = owner.controlFacade();
    ASSERT_TRUE(
        api.setMultiSelectionPermission(runtime::MultiSelectionPermissionMode::kLocalSingleUser));
    runtime::StickyCreateRequest sticky;
    sticky.objectId = ObjectId::fromUint64(100);
    sticky.content = {160, 100};
    ASSERT_EQ(api.createSticky(sticky).state, runtime::ConnectorCommandState::kApplied);
    sticky.objectId = ObjectId::fromUint64(101);
    sticky.transform.tx = 200;
    ASSERT_EQ(api.createSticky(sticky).state, runtime::ConnectorCommandState::kApplied);
    auto group = api.groupObjects({ObjectId::fromUint64(100), ObjectId::fromUint64(101)},
                                  ObjectId::fromUint64(900), 1, 90);
    ASSERT_EQ(group.state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(owner.ownerPump());
    ASSERT_EQ(api.canvasControlReceipt({90, 1})->state,
              runtime::CanvasControlReceiptState::kApplied);
    auto ungroup = api.ungroupObject(ObjectId::fromUint64(900), 2, 90);
    ASSERT_EQ(ungroup.state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(owner.ownerPump());
    ASSERT_EQ(api.canvasControlReceipt({90, 2})->state,
              runtime::CanvasControlReceiptState::kApplied);
    ASSERT_EQ(api.readAppearance().eligible.size(), 2);
    const auto lock = Operation{
        OperationId{ObjectId::fromUint64(901)}, DocumentId{ObjectId::fromUint64(1)}, 1, 1,
        PatchPropertiesOp{{{sticky.objectId, 2, PropertyPatchAction::kSet, PropertyValue{true}}}}};
    ASSERT_EQ(owner.applyRemoteOperation(lock).disposition, ApplyDisposition::kApplied);
    const auto selected = api.readAppearance();
    ASSERT_EQ(selected.eligible.size(), 2);
    EXPECT_FALSE(selected.writable);
    runtime::AppearanceRequest request;
    request.stamp = selected.stamp;
    request.eligible = selected.eligible;
    request.patch.enabled = true;
    request.patch.roughness = 2;
    EXPECT_FALSE(api.appearanceCommand(request).applied);
    const auto after = api.readAppearance();
    EXPECT_EQ(after.stamp.semanticGeneration, selected.stamp.semanticGeneration);
    for (const auto &object : after.objects)
        EXPECT_FALSE(object.values.enabled);
}
TEST(SketchSemantic, StickyNoFillRequiresVersionedAppearanceRatherThanSilentLegacyElision) {
    auto o = sketchObject();
    o.kind = ObjectKind::kSticky;
    o.content = StickyContent{120, 80};
    o.properties.entries = {{0x100U, FillStyleValue{NoFill{}}}};
    EXPECT_FALSE(SemanticCodec::encodeProtobufOperation(op(o)).ok());
    o.properties.entries.push_back({0x102U, SketchStyleV1{1, 99, 0, false}});
    const auto wire = SemanticCodec::encodeProtobufOperation(op(o));
    ASSERT_TRUE(wire.ok());
    const auto read = SemanticCodec::decodeProtobufOperation(wire.bytes);
    ASSERT_TRUE(read.ok());
    EXPECT_EQ(read.operation.payload, op(o).payload);
}
TEST(SketchRuntime, HatchGapsDoNotCreateTriangleSelectionHoles) {
    using namespace canvas;
    runtime::RuntimeComposition owner;
    ASSERT_TRUE(owner.openDocument(ObjectId::fromUint64(1)));
    ASSERT_TRUE(owner.bindSurface(800, 600));
    auto &api = owner.controlFacade();
    auto object = sketchObject();
    object.kind_version = 2;
    object.content = ShapeContent{3, 120, 80};
    object.properties.entries.insert(
        object.properties.entries.begin(),
        {0x100U, FillStyleValue{
                     PatternFillV1{1, {.2F, .5F, .8F, 1}, PatternKind::kDiagonal, -45, 100, 1}}});
    ASSERT_EQ(owner.applyRemoteOperation(op(object)).disposition, ApplyDisposition::kApplied);
    runtime::CanvasControlRequest select;
    select.target = api.mountedCanvasTargets().front();
    select.clientId = 99;
    select.requestId = 1;
    select.payload.kind = runtime::CanvasControlPayloadKind::kSelectTool;
    select.payload.tool = runtime::CanvasToolKind::kSelect;
    ASSERT_EQ(api.submitCanvasControl(select).state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(owner.ownerPump());
    ASSERT_TRUE(owner.registerSource(1));
    input::PlatformPointerSample sample;
    sample.source = 1;
    sample.pointer = 1;
    sample.sequence = 1;
    sample.timestampNs = 1;
    sample.x = 60;
    sample.y = 50;
    sample.phase = input::PointerPhase::kDown;
    input::PlatformPointerBatch batch;
    batch.samples.push_back(sample);
    const auto key = owner.beginContact(batch);
    ASSERT_TRUE(key);
    sample.phase = input::PointerPhase::kUp;
    sample.sequence = 2;
    sample.timestampNs = 2;
    batch.samples[0] = sample;
    ASSERT_TRUE(owner.submitContact(*key, batch));
    EXPECT_EQ(api.readSelectionSummary().primary, object.id);
}
TEST(SketchRuntime, CreationSnapsToAuthoredEdgesInsteadOfRoughInkBounds) {
    using namespace canvas;
    runtime::RuntimeComposition owner;
    ASSERT_TRUE(owner.openDocument(ObjectId::fromUint64(1)));
    ASSERT_TRUE(owner.bindSurface(800, 600));
    auto &api = owner.controlFacade();
    auto object = sketchObject();
    object.transform.tx = 40;
    object.transform.ty = 40;
    std::get<SketchStyleV1>(object.properties.entries[0].value).roughness = 3;
    object.properties.entries.insert(
        object.properties.entries.begin(),
        {0x101U, StrokeStyleValue{
                     SolidStroke{{0, 0, 0, 1}, 2, StrokeCap::kButt, MiterJoin{4}, SolidDash{}}}});
    ASSERT_EQ(owner.applyRemoteOperation(op(object)).disposition, ApplyDisposition::kApplied);
    runtime::CanvasControlRequest tool;
    tool.target = api.mountedCanvasTargets().front();
    tool.clientId = 99;
    tool.requestId = 1;
    tool.payload.kind = runtime::CanvasControlPayloadKind::kSelectCreationPreset;
    tool.payload.creationPreset = runtime::CanvasCreationPreset::kRectangle;
    ASSERT_EQ(api.submitCanvasControl(tool).state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(owner.ownerPump());
    ASSERT_TRUE(owner.registerSource(1));
    input::PlatformPointerSample sample;
    sample.source = 1;
    sample.pointer = 1;
    sample.sequence = 1;
    sample.timestampNs = 1;
    sample.x = 40;
    sample.y = 200;
    sample.phase = input::PointerPhase::kDown;
    input::PlatformPointerBatch batch;
    batch.samples.push_back(sample);
    const auto key = owner.beginContact(batch);
    ASSERT_TRUE(key);
    sample.phase = input::PointerPhase::kUp;
    sample.sequence = 2;
    sample.timestampNs = 2;
    sample.x = 160;
    sample.y = 280;
    batch.samples[0] = sample;
    ASSERT_TRUE(owner.submitContact(*key, batch));
    runtime::DocumentSnapshotRequest request;
    request.expectedSemanticGeneration = api.readSelectionSummary().semanticGeneration;
    request.documentRevision = 1;
    request.recoveryFrontier = {8, 1, 16, 1, 24, 1};
    const auto saved = owner.captureDocumentSnapshot(request);
    ASSERT_EQ(saved.status, runtime::DocumentSnapshotStatus::kCaptured);
    const auto decoded = SnapshotCodec::decode(saved.bytes, SnapshotCapability::kAllWithSketchV1);
    ASSERT_TRUE(decoded.ok());
    ASSERT_EQ(decoded.snapshot->objects.size(), 2);
    const auto created =
        std::find_if(decoded.snapshot->objects.begin(), decoded.snapshot->objects.end(),
                     [&](const auto &r) { return r.id != object.id; });
    ASSERT_NE(created, decoded.snapshot->objects.end());
    EXPECT_NEAR(std::get<ShapeContent>(created->content).width, 120, 1e-5);
}
TEST(SketchRuntime, SmallRoughEllipseCanCommitAfterIntermediateMoves) {
    using namespace canvas;
    runtime::RuntimeComposition owner;
    ASSERT_TRUE(owner.openDocument(ObjectId::fromUint64(1)));
    ASSERT_TRUE(owner.bindSurface(800, 550));
    auto &api = owner.controlFacade();
    auto target = sketchObject();
    target.transform.tx = 600;
    target.transform.ty = 210;
    target.content = ShapeContent{1, 140, 100};
    ASSERT_EQ(owner.applyRemoteOperation(op(target)).disposition, ApplyDisposition::kApplied);
    runtime::AppearanceRequest defaults;
    defaults.action = runtime::AppearanceAction::kDefaults;
    defaults.patch.enabled = true;
    defaults.patch.roughness = 3;
    defaults.patch.fillKind = runtime::AppearanceFill::kDiagonal;
    defaults.patch.strokeWidth = 1;
    ASSERT_TRUE(api.appearanceCommand(defaults).applied);
    runtime::CanvasControlRequest tool;
    tool.target = api.mountedCanvasTargets().front();
    tool.clientId = 99;
    tool.requestId = 1;
    tool.payload.kind = runtime::CanvasControlPayloadKind::kSelectCreationPreset;
    tool.payload.creationPreset = runtime::CanvasCreationPreset::kEllipse;
    ASSERT_EQ(api.submitCanvasControl(tool).state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(owner.ownerPump());
    ASSERT_TRUE(owner.registerSource(1));
    input::PlatformPointerSample sample;
    sample.source = 1;
    sample.pointer = 1;
    sample.sequence = 1;
    sample.timestampNs = 1;
    sample.x = 600;
    sample.y = 380;
    sample.phase = input::PointerPhase::kDown;
    input::PlatformPointerBatch batch;
    batch.samples.push_back(sample);
    const auto key = owner.beginContact(batch);
    ASSERT_TRUE(key);
    sample.phase = input::PointerPhase::kMove;
    sample.sequence = 2;
    sample.timestampNs = 2;
    sample.x = 610;
    sample.y = 387;
    batch.samples[0] = sample;
    ASSERT_TRUE(owner.submitContact(*key, batch));
    sample.phase = input::PointerPhase::kMove;
    sample.sequence = 3;
    sample.timestampNs = 3;
    sample.x = 620;
    sample.y = 394;
    batch.samples[0] = sample;
    ASSERT_TRUE(owner.submitContact(*key, batch));
    sample.phase = input::PointerPhase::kUp;
    sample.sequence = 4;
    sample.timestampNs = 4;
    batch.samples[0] = sample;
    ASSERT_TRUE(owner.submitContact(*key, batch));
    EXPECT_EQ(api.readSelectionSummary().semanticGeneration, 2);
    runtime::DocumentSnapshotRequest snapshotRequest;
    snapshotRequest.expectedSemanticGeneration = 2;
    snapshotRequest.documentRevision = 1;
    snapshotRequest.recoveryFrontier = {8, 1, 16, 1, 24, 1};
    const auto saved = owner.captureDocumentSnapshot(snapshotRequest);
    ASSERT_EQ(saved.status, runtime::DocumentSnapshotStatus::kCaptured);
    const auto decoded = SnapshotCodec::decode(saved.bytes, SnapshotCapability::kAllWithSketchV1);
    ASSERT_TRUE(decoded.ok());
    const auto created =
        std::find_if(decoded.snapshot->objects.begin(), decoded.snapshot->objects.end(),
                     [&](const auto &r) { return r.id != target.id; });
    ASSERT_NE(created, decoded.snapshot->objects.end());
    EXPECT_EQ(std::get<ShapeContent>(created->content).width, 20);
    EXPECT_EQ(std::get<ShapeContent>(created->content).height, 14);
}

#include "canvas/render/image_resource.hpp"
TEST(SketchRuntime, FailedRouteRetainsExistingDiagnosticRendering) {
    using namespace canvas;
    runtime::RuntimeComposition owner;
    ASSERT_TRUE(owner.openDocument(ObjectId::fromUint64(1)));
    ASSERT_TRUE(owner.bindSurface(800, 550));
    auto box = sketchObject();
    box.properties.entries.clear();
    box.kind = ObjectKind::kSticky;
    box.content = StickyContent{100, 100};
    box.transform.tx = 200;
    box.transform.ty = 200;
    auto line = sketchObject();
    line.id = ObjectId::fromUint64(11);
    line.placement.order_key = OrderKey({129});
    line.kind = ObjectKind::kConnector;
    line.kind_version = 1;
    line.content = ConnectorContent{{FreePointEndpoint{{230, 230}}},
                                    {FreePointEndpoint{{650, 230}}},
                                    ConnectorRouting::kOrthogonal};
    line.properties.entries.insert(
        line.properties.entries.begin(),
        {0x101U, StrokeStyleValue{
                     SolidStroke{{0, 0, 1, 1}, 4, StrokeCap::kButt, MiterJoin{4}, SolidDash{}}}});
    Operation insert{OperationId{ObjectId::fromUint64(2)}, DocumentId{ObjectId::fromUint64(1)}, 1,
                     1, InsertObjectsOp{{box, line}}};
    ASSERT_EQ(owner.applyRemoteOperation(insert).disposition, ApplyDisposition::kApplied);
    for (int n = 0; n < 16; ++n)
        ASSERT_TRUE(owner.ownerPump());
    auto &api = owner.controlFacade();
    ASSERT_EQ(api.readConnectorState(line.id).routeStatus,
              runtime::ConnectorRouteStatus::kNoRouteInDomain);
    runtime::ImageExportOptions options;
    options.bounds = std::array<double, 4>{0, 0, 800, 550};
    options.padding = 0;
    const auto image = api.exportImage(options);
    ASSERT_EQ(image.status, runtime::ImageExportStatus::kExported);
    runtime::MemoryResourceProvider resources;
    const auto id = ObjectId::fromUint64(99);
    resources.publish(id, image.bytes);
    render::ImageResourceResolver decoder(resources);
    const auto pixels = decoder.resolve(ResourceId{id});
    ASSERT_EQ(pixels->state, render::ImageResourceState::kReady);
    std::size_t diagnostic = 0, blue = 0;
    for (unsigned y = 225; y < 236; ++y)
        for (unsigned x = 400; x < 640; ++x) {
            auto i = (y * 800 + x) * 4;
            auto r = pixels->rgba[i], g = pixels->rgba[i + 1], b = pixels->rgba[i + 2];
            if (r > 180 && r < 250 && g < 140 && b < 100)
                ++diagnostic;
            if (b > 180 && r < 80 && g < 80)
                ++blue;
        }
    EXPECT_GT(diagnostic, 0);
    EXPECT_EQ(blue, 0);
}
