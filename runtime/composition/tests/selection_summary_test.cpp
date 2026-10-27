#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/semantic/normalizer.hpp"
#include "canvas/text/text_layout.hpp"

#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <thread>

namespace canvas {
class IncrementalRuntimeTestAccess {
  public:
    static void stale(IncrementalRuntimeCoordinator& c) {
        c.corruptRuntimeProjectionForTest();
    }
};
namespace ink_playground {
class InkPlaygroundHistoryTestAccess {
  public:
    static void select(interaction::CanvasEditingRuntime& c,
                       std::vector<foundation::ObjectId> ids) {
        ASSERT_TRUE(c.selection_.applyAcquisition(
            ids, interaction::SelectionCombineMode::kReplace, c.selection_.revision()));
    }
    static void stale(interaction::CanvasEditingRuntime& c) {
        IncrementalRuntimeTestAccess::stale(*c.sceneCoordinator_);
    }
    static std::size_t history(const interaction::CanvasEditingRuntime& c) {
        return c.history_.size();
    }
};
} // namespace ink_playground
} // namespace canvas
namespace {
using namespace canvas;
using Status = runtime::SelectionSummaryStatus;
using Reason = runtime::SelectionReason;
using Action = runtime::SelectionAction;
using Cap = runtime::SelectionCapabilityState;
using Property = runtime::SelectionPropertyState;
using Access = ink_playground::InkPlaygroundHistoryTestAccess;
foundation::ObjectId id(std::uint64_t n) {
    return foundation::ObjectId::fromUint64(n);
}
semantic::ObjectRecord shape(std::uint64_t n, double x = 10, double y = 20) {
    semantic::ObjectRecord r;
    r.id = id(n);
    r.kind = semantic::ObjectKind::kShape;
    r.kind_version = 1;
    r.content = semantic::ShapeContent{1, 40, 30};
    r.transform.tx = x;
    r.transform.ty = y;
    r.placement.order_key = semantic::OrderKey({static_cast<std::uint8_t>(n)});
    return r;
}
semantic::ObjectRecord group(std::uint64_t n) {
    auto r = shape(n);
    r.kind = semantic::ObjectKind::kGroup;
    r.content = semantic::GroupContent{};
    r.transform = {};
    return r;
}
class Summary : public testing::Test {
  protected:
    runtime::RuntimeComposition c;
    runtime::RuntimeFacade& f = c.controlFacade();
    std::uint64_t operation = 9000;
    void SetUp() override {
        ASSERT_TRUE(c.open());
        ASSERT_TRUE(c.bindSurface(800, 600));
    }
    void apply(semantic::OperationPayload payload) {
        semantic::Operation op{semantic::OperationId{id(++operation)},
                               semantic::DocumentId{id(1)},
                               1,
                               1,
                               std::move(payload)};
        ASSERT_EQ(c.applyExternalOperation(op, semantic::ApplySource::kRemoteSync).disposition,
                  semantic::ApplyDisposition::kApplied);
    }
    void put(std::vector<semantic::ObjectRecord> records) {
        apply(semantic::InsertObjectsOp{std::move(records)});
    }
    void select(std::vector<foundation::ObjectId> roots) {
        Access::select(c.qualificationRuntime(), std::move(roots));
    }
    auto read() {
        return f.readSelectionSummary();
    }
    void TearDown() override {
        EXPECT_TRUE(c.close());
    }
};
TEST_F(Summary, EmptyIsNotAUniformPropertyOrAnExecutableSelection) {
    const auto s = read();
    EXPECT_EQ(s.status, Status::kAvailable);
    EXPECT_EQ(s.count, 0U);
    EXPECT_TRUE(s.roots.empty());
    EXPECT_TRUE(s.primary.isZero());
    EXPECT_TRUE(s.kindCounts.empty());
    EXPECT_FALSE(s.bounds.world);
    for (const auto& p : s.properties) {
        EXPECT_EQ(p.state, Property::kUnavailable);
        EXPECT_FALSE(p.value);
    }
    EXPECT_FALSE(s.capability(Action::kDelete).available());
    EXPECT_FALSE(s.capability(Action::kFit).available());
}
TEST_F(Summary, PublicSnapshotsShareOneNormalizedSummaryAndExactWorldBounds) {
    put({shape(1), shape(2, 100, 200)});
    select({id(2), id(1)});
    const auto before = c.readback();
    const auto history = Access::history(c.qualificationRuntime());
    const auto s = read();
    ASSERT_EQ(s.status, Status::kAvailable);
    EXPECT_EQ(s.count, 2U);
    EXPECT_EQ(s.primary, id(2));
    EXPECT_EQ(s.roots, (std::vector{id(2), id(1)}));
    ASSERT_EQ(s.kindCounts.size(), 1U);
    EXPECT_EQ(s.kindCounts[0].kind, 1U);
    EXPECT_EQ(s.kindCounts[0].count, 2U);
    ASSERT_EQ(s.bounds.status, Status::kAvailable);
    ASSERT_TRUE(s.bounds.world);
    EXPECT_EQ(*s.bounds.world, (foundation::WorldRect{10, 20, 140, 230}));
    EXPECT_EQ(s.semanticGeneration, before.semanticGeneration);
    EXPECT_EQ(s.sceneGeneration, s.semanticGeneration);
    EXPECT_GT(s.sceneRevision, 0U);
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(read(), s);
        EXPECT_EQ(f.readRuntimeState().control.selectionRevision, s.selectionRevision);
        EXPECT_EQ(f.readCanvasControlSnapshot(f.mountedCanvasTargets().front()).selectionSummary,
                  s);
    }
    EXPECT_EQ(c.readback().semanticGeneration, before.semanticGeneration);
    EXPECT_EQ(Access::history(c.qualificationRuntime()), history);
}
TEST_F(Summary, SectionAndNestedGroupCountRootsWithoutDescendants) {
    auto section = shape(1);
    section.kind = semantic::ObjectKind::kSection;
    section.content = semantic::SectionContent{{0, 0, 200, 120}, "S", 8, 20};
    section.transform = {};
    auto child = shape(2, 900, 900);
    child.placement.parent_id = section.id;
    auto outer = group(3), inner = group(4), member = shape(5, 300, 100);
    inner.placement.parent_id = outer.id;
    member.placement.parent_id = inner.id;
    put({section, child, outer, inner, member});
    select({child.id, section.id, member.id, outer.id});
    auto s = read();
    EXPECT_EQ(s.count, 2U);
    EXPECT_EQ(s.roots, (std::vector{section.id, outer.id}));
    EXPECT_EQ(s.primary, section.id);
    ASSERT_TRUE(s.bounds.world);
    EXPECT_EQ(*s.bounds.world, (foundation::WorldRect{0, 0, 340, 130}));
    runtime::SectionIntent enter;
    enter.action = runtime::SectionAction::kEnterGroupScope;
    enter.objectId = outer.id;
    ASSERT_EQ(f.submitSectionIntent(enter, 1, 7001).state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(c.ownerPump());
    select({member.id, child.id});
    s = read();
    EXPECT_EQ(s.roots, std::vector{inner.id});
    EXPECT_EQ(s.primary, inner.id);
    EXPECT_EQ(s.groupScope, outer.id);
}
TEST_F(Summary, TransformAndDeletionRefreshWithoutChangingSelectionOnRead) {
    put({shape(1), shape(2, 100, 200)});
    select({id(1), id(2)});
    const auto before = read();
    apply(semantic::SetTransformsOp{{{id(2), {1, 0, 0, 1, 300, 400}}}});
    const auto moved = read();
    EXPECT_EQ(moved.selectionRevision, before.selectionRevision);
    EXPECT_GT(moved.semanticGeneration, before.semanticGeneration);
    ASSERT_TRUE(moved.bounds.world);
    EXPECT_EQ(moved.bounds.world->right, 340);
    EXPECT_EQ(moved.bounds.world->bottom, 430);
    apply(semantic::DeleteObjectsOp{{id(1)}});
    const auto remaining = read();
    EXPECT_EQ(remaining.roots, std::vector{id(2)});
    EXPECT_EQ(remaining.primary, id(2));
    EXPECT_GT(remaining.selectionRevision, before.selectionRevision);
    EXPECT_EQ(remaining.count, 1U);
}
TEST_F(Summary, StaleSceneCannotPublishAuthoritativeBoundsPropertiesOrActions) {
    put({shape(1)});
    select({id(1)});
    const auto before = c.readback();
    Access::stale(c.qualificationRuntime());
    const auto s = read();
    EXPECT_EQ(s.status, Status::kStale);
    EXPECT_EQ(s.reason, Reason::kStaleScene);
    EXPECT_FALSE(s.bounds.world);
    EXPECT_EQ(s.bounds.status, Status::kStale);
    for (const auto& p : s.properties)
        EXPECT_EQ(p.state, Property::kUnavailable);
    EXPECT_FALSE(s.capability(Action::kDelete).available());
    EXPECT_EQ(c.readback().semanticGeneration, before.semanticGeneration);
}
TEST_F(Summary, MissingTextMetricsAreNotAZeroSizedCompleteBound) {
    semantic::ResourceManifest manifest;
    semantic::ResourceManifestEntry font;
    font.resource_id = semantic::ResourceId{id(500)};
    font.resource_kind = semantic::ResourceKind::kFont;
    font.resource_revision = 1;
    font.content_hash = "sha256:" + std::string(text::kRobotoSha256);
    font.byte_length = std::filesystem::file_size(SELECTION_TEST_FONT);
    font.decode_version = font.layout_version = 1;
    font.metadata = {8, 1, 16, 0, 24, 1};
    manifest.entries = {font};
    ASSERT_TRUE(c.bindManifest(
        manifest, 1)); // Admit real locked font identity; deliberately do not publish bytes.
    auto r = shape(1);
    r.kind = semantic::ObjectKind::kRichText;
    r.content = semantic::RichTextContent{};
    semantic::TextStyle style;
    style.font_resource_id = font.resource_id;
    style.font_size = 16;
    style.weight = 400;
    style.color = {0, 0, 0, 1};
    semantic::Paragraph p;
    p.id = id(7002);
    p.style.alignment = semantic::ParagraphAlignment::kLeft;
    p.style.line_height = 1.2;
    p.runs = {{"unknown metrics", style}};
    std::get<semantic::RichTextContent>(r.content).document.paragraphs = {p};
    put({r});
    ASSERT_EQ(c.semanticObjectCount(), 1U);
    select({r.id});
    const auto s = read();
    EXPECT_EQ(s.count, 1U);
    EXPECT_EQ(s.bounds.status, Status::kUnavailable);
    EXPECT_FALSE(s.bounds.world);
    EXPECT_EQ(s.bounds.reason, Reason::kBoundsUnavailable);
    EXPECT_FALSE(s.capability(Action::kFit).available());
}
TEST_F(Summary, WrongThreadAndClosedRuntimeHaveNoUsableProjection) {
    runtime::SelectionSummary other;
    std::thread worker([&] { other = f.readSelectionSummary(); });
    worker.join();
    EXPECT_EQ(other.status, Status::kUnavailable);
    EXPECT_EQ(other.reason, Reason::kWrongThread);
    EXPECT_EQ(other.capability(Action::kDelete).reason, Reason::kWrongThread);
    EXPECT_EQ(other.bounds.reason, Reason::kWrongThread);
    ASSERT_TRUE(c.close());
    const auto closed = read();
    EXPECT_EQ(closed.status, Status::kUnavailable);
    EXPECT_EQ(closed.reason, Reason::kClosed);
    EXPECT_TRUE(closed.roots.empty());
    EXPECT_EQ(closed.capability(Action::kDelete).reason, Reason::kClosed);
    EXPECT_EQ(closed.properties[0].reason, Reason::kClosed);
}
TEST_F(Summary, MissingRootsAndEmptyGroupsNeverFabricateUsableGeometry) {
    put({group(1)});
    select({id(99)});
    const auto invalid = read();
    EXPECT_EQ(invalid.status, Status::kInvalid);
    EXPECT_TRUE(invalid.roots.empty());
    EXPECT_TRUE(invalid.primary.isZero());
    EXPECT_EQ(invalid.capability(Action::kDelete).state, Cap::kInvalidSelection);
    EXPECT_EQ(invalid.bounds.reason, Reason::kInvalidHierarchy);
    select({id(1)});
    const auto emptyGroup = read();
    EXPECT_EQ(emptyGroup.count, 1U);
    EXPECT_EQ(emptyGroup.bounds.status, Status::kUnavailable);
    EXPECT_FALSE(emptyGroup.bounds.world);
    EXPECT_FALSE(emptyGroup.capability(Action::kFit).available());
}
TEST_F(Summary, PropertiesReportUniformMixedPartialAndAcceptedDefaults) {
    auto a = shape(1), b = shape(2, 100, 100);
    a.properties.entries = {{3U, 0.5F}};
    b.properties.entries = {{3U, 0.5F}};
    put({a, b, group(3)});
    select({a.id, b.id});
    auto s = read();
    ASSERT_EQ(s.properties[0].state, Property::kUniform);
    ASSERT_TRUE(s.properties[0].value);
    EXPECT_TRUE(std::get<bool>(*s.properties[0].value));
    ASSERT_EQ(s.properties[1].state, Property::kUniform);
    ASSERT_TRUE(s.properties[1].value);
    EXPECT_FALSE(std::get<bool>(*s.properties[1].value));
    ASSERT_EQ(s.properties[2].state, Property::kUniform);
    ASSERT_TRUE(s.properties[2].value);
    EXPECT_EQ(std::get<float>(*s.properties[2].value), 0.5F);
    EXPECT_EQ(s.properties[2].knownRoots, 2U);
    apply(semantic::PatchPropertiesOp{
        {{b.id, 3U, semantic::PropertyPatchAction::kSet, semantic::PropertyValue{0.75F}}}});
    s = read();
    EXPECT_EQ(s.properties[2].state, Property::kMixed);
    EXPECT_FALSE(s.properties[2].value);
    EXPECT_TRUE(s.properties[2].applicableValuesMixed);
    select({a.id, id(3)});
    s = read();
    EXPECT_EQ(s.properties[2].state, Property::kPartial);
    EXPECT_EQ(s.properties[2].applicableRoots, 1U);
    EXPECT_EQ(s.properties[2].knownRoots, 1U);
    EXPECT_FALSE(s.properties[2].value);
    select({id(3)});
    s = read();
    EXPECT_EQ(s.properties[2].state, Property::kUnavailable);
    EXPECT_EQ(s.properties[2].reason, Reason::kNotApplicable);
    apply(semantic::PatchPropertiesOp{{{b.id, 3U, semantic::PropertyPatchAction::kClear, {}}}});
    select({a.id, b.id});
    s = read();
    EXPECT_EQ(s.properties[2].state, Property::kMixed);
    EXPECT_EQ(s.properties[2].knownRoots, 2U);
    EXPECT_FALSE(s.properties[2].value);
    select({b.id});
    s = read();
    ASSERT_EQ(s.properties[2].state, Property::kUniform);
    ASSERT_TRUE(s.properties[2].value);
    EXPECT_EQ(std::get<float>(*s.properties[2].value), 1.0F);
}
TEST(SummaryDefaults, PublishedReadAccessorMatchesExistingCanonicalDefaultElision) {
    const auto opacity = semantic::propertyDefaultValue(3U);
    ASSERT_TRUE(opacity);
    EXPECT_EQ(std::get<float>(*opacity), 1.0F);
    EXPECT_EQ(std::get<bool>(*semantic::propertyDefaultValue(1U)), true);
    EXPECT_EQ(std::get<bool>(*semantic::propertyDefaultValue(2U)), false);
    EXPECT_FALSE(semantic::propertyDefaultValue(0xFFFFFFFFU));
    auto explicitDefault = shape(1);
    explicitDefault.properties.entries = {{1U, true}, {2U, false}, {3U, 1.0F}};
    semantic::Operation op{semantic::OperationId{id(9)},
                           semantic::DocumentId{id(1)},
                           1,
                           1,
                           semantic::InsertObjectsOp{{explicitDefault}}};
    const auto canonical = semantic::normalizeOperation(op);
    ASSERT_TRUE(canonical.ok());
    EXPECT_TRUE(std::get<semantic::InsertObjectsOp>(canonical.value.payload)
                    .objects.front()
                    .properties.entries.empty());
}
TEST_F(Summary, CapabilitiesUseRealOwnersAndNeverFallBackToPrimaryForMultiEdit) {
    put({shape(1), shape(2, 100, 100)});
    select({id(1)});
    auto s = read();
    for (const auto action : {Action::kFit,
                              Action::kDelete,
                              Action::kTransform,
                              Action::kSize,
                              Action::kAppearance,
                              Action::kLock,
                              Action::kOrder,
                              Action::kFitPrimary})
        EXPECT_TRUE(s.capability(action).available()) << static_cast<unsigned>(action);
    select({id(1), id(2)});
    s = read();
    EXPECT_TRUE(s.capability(Action::kFit).available());
    EXPECT_TRUE(s.capability(Action::kDelete).available());
    for (const auto action : {Action::kTransform,
                              Action::kSize,
                              Action::kAppearance,
                              Action::kLock,
                              Action::kOrder,
                              Action::kFitPrimary}) {
        EXPECT_FALSE(s.capability(action).available());
        EXPECT_EQ(s.capability(action).reason, Reason::kMultipleRoots);
    }
    for (const auto action : {Action::kGroup,
                              Action::kUngroup,
                              Action::kBulkProperties,
                              Action::kSnap,
                              Action::kAlignment})
        EXPECT_EQ(s.capability(action).state, Cap::kNotImplemented);
    EXPECT_FALSE(s.capability(Action::kWrapInSection).available());
    EXPECT_EQ(s.capability(Action::kWrapInSection).reason, Reason::kOutsideBatch);
    const auto before = c.readback();
    const auto history = Access::history(c.qualificationRuntime());
    runtime::CanvasControlRequest q;
    q.target = f.mountedCanvasTargets().front();
    q.clientId = 7001;
    q.requestId = 1;
    q.payload.kind = runtime::CanvasControlPayloadKind::kSelectedTransform;
    q.payload.transform = {1, 0, 0, 1, 400, 400};
    ASSERT_EQ(f.submitCanvasControl(q).state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(c.ownerPump());
    EXPECT_NE(f.canvasControlReceipt({7001, 1})->state,
              runtime::CanvasControlReceiptState::kApplied);
    EXPECT_EQ(c.readback().semanticGeneration, before.semanticGeneration);
    EXPECT_EQ(Access::history(c.qualificationRuntime()), history);
}
TEST_F(Summary, MixedLocksAndInheritedLocksRemainVisibleInAggregatesAndCapabilities) {
    auto a = shape(1), b = shape(2, 100, 100);
    b.properties.entries = {{2U, true}};
    put({a, b});
    select({a.id, b.id});
    auto s = read();
    EXPECT_EQ(s.properties[1].state, Property::kMixed);
    EXPECT_FALSE(s.capability(Action::kDelete).available());
    EXPECT_EQ(s.capability(Action::kDelete).reason, Reason::kLocked);
    select({b.id});
    s = read();
    EXPECT_TRUE(s.capability(Action::kUnlock).available());
    EXPECT_FALSE(s.capability(Action::kTransform).available());
    auto g = group(3), member = shape(4);
    g.properties.entries = {{2U, true}};
    member.placement.parent_id = g.id;
    put({g, member});
    runtime::SectionIntent enter;
    enter.action = runtime::SectionAction::kEnterGroupScope;
    enter.objectId = g.id;
    ASSERT_EQ(f.submitSectionIntent(enter, 1, 7002).state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(c.ownerPump());
    select({member.id});
    s = read();
    ASSERT_EQ(s.properties[1].state, Property::kUniform);
    EXPECT_TRUE(std::get<bool>(*s.properties[1].value));
    EXPECT_FALSE(s.capability(Action::kUnlock).available());
    EXPECT_EQ(s.capability(Action::kUnlock).reason, Reason::kLocked);
}
TEST_F(Summary, BusyRegionAndNonNormalizedRootsDoNotAdvertiseUnsafeCommands) {
    put({shape(1)});
    select({id(1)});
    ASSERT_TRUE(c.registerSource(7003));
    ASSERT_EQ(
        f.setSelectionAcquisitionMode(runtime::CanvasSelectionMode::kRectangle, 1, 7003).state,
        runtime::ProductControlState::kQueued);
    ASSERT_TRUE(c.ownerPump());
    input::PlatformPointerSample p;
    p.source = 7003;
    p.pointer = 1;
    p.sequence = 1;
    p.timestampNs = 1000000;
    p.phase = input::PointerPhase::kDown;
    p.x = 500;
    p.y = 500;
    input::PlatformPointerBatch batch;
    batch.samples = {p};
    ASSERT_TRUE(c.submit(batch));
    auto s = read();
    EXPECT_EQ(s.capability(Action::kDelete).reason, Reason::kBusy);
    EXPECT_FALSE(s.capability(Action::kFit).available());
    ASSERT_TRUE(c.cancelAllPointers());
    EXPECT_TRUE(read().capability(Action::kDelete).available());
    auto section = shape(2);
    section.kind = semantic::ObjectKind::kSection;
    section.content = semantic::SectionContent{{0, 0, 200, 120}, "S", 8, 20};
    section.transform = {};
    auto child = shape(3);
    child.placement.parent_id = section.id;
    put({section, child});
    select({child.id, section.id});
    s = read();
    EXPECT_EQ(s.count, 1U);
    EXPECT_EQ(s.primary, section.id);
    EXPECT_EQ(s.capability(Action::kDelete).reason, Reason::kNonNormalizedSelection);
    EXPECT_FALSE(s.capability(Action::kFit).available());
    select({section.id, id(1)});
    EXPECT_FALSE(read().capability(Action::kDelete).available());
}
TEST_F(Summary, SummaryRefreshPreservesRealRedoAndCanonicalOutcome) {
    put({shape(1)});
    runtime::StickyCreateRequest create;
    create.objectId = id(8);
    ASSERT_EQ(f.createSticky(create).state, runtime::ConnectorCommandState::kApplied);
    ASSERT_TRUE(c.qualificationRuntime().undo());
    select({id(1)});
    const auto before = f.readRuntimeState();
    ASSERT_TRUE(before.history.canRedo);
    const auto history = Access::history(c.qualificationRuntime());
    const auto summary = read();
    const auto outcome = f.readCanvasControlSnapshot(f.mountedCanvasTargets().front());
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(read(), summary);
        EXPECT_EQ(f.readRuntimeState().control.selectionRevision, summary.selectionRevision);
    }
    const auto after = f.readRuntimeState();
    EXPECT_EQ(after.identity.documentGeneration, before.identity.documentGeneration);
    EXPECT_EQ(after.identity.documentRevision, before.identity.documentRevision);
    EXPECT_EQ(after.history.canRedo, before.history.canRedo);
    EXPECT_EQ(after.history.canUndo, before.history.canUndo);
    EXPECT_EQ(Access::history(c.qualificationRuntime()), history);
    EXPECT_EQ(f.readCanvasControlSnapshot(f.mountedCanvasTargets().front()).lastAppliedOperation,
              outcome.lastAppliedOperation);
}
} // namespace
