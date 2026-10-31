#include "canvas/interaction/region_selection_geometry.hpp"
#include "canvas/render/image_resource.hpp"
#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "canvas/scene/sketch_geometry.hpp"
#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/snapshot.hpp"
#include <algorithm>
#include <gtest/gtest.h>
using namespace canvas;
namespace {
semantic::PropertyBag sketchStyle(double roughness = 2) {
    return {{{0x102U, semantic::SketchStyleV1{1, 9223372036854775809ULL, roughness, true}}}};
}
semantic::VectorPathGeometry openCurve() {
    return {semantic::FillRule::kEvenOdd,
            {semantic::MoveTo{{0, 0}}, semantic::QuadTo{{50, -40}, {100, 0}},
             semantic::CubicTo{{130, 20}, {160, -20}, {200, 0}}}};
}
semantic::TableContent table() {
    semantic::TableContent t;
    t.rows = {{{foundation::ObjectId::fromUint64(11)}, 30},
              {{foundation::ObjectId::fromUint64(12)}, 30}};
    t.columns = {{{foundation::ObjectId::fromUint64(21)}, 40},
                 {{foundation::ObjectId::fromUint64(22)}, 40},
                 {{foundation::ObjectId::fromUint64(23)}, 40}};
    t.cells.resize(6);
    return t;
}
} // namespace
TEST(SketchExtendedGeometry, OpenCurveKeepsCanonicalCommandsAndNeverClosesItsOutline) {
    const auto path = openCurve();
    const auto g = scene::sketchGeometry(semantic::VectorPathContent{path}, sketchStyle());
    ASSERT_TRUE(g);
    EXPECT_EQ(g->body, path);
    EXPECT_EQ(
        std::count_if(g->outline.commands.begin(), g->outline.commands.end(),
                      [](const auto &c) { return std::holds_alternative<semantic::ClosePath>(c); }),
        0);
    const auto *curve = std::get_if<semantic::CubicTo>(&g->outline.commands.back());
    ASSERT_TRUE(curve);
    EXPECT_EQ(curve->end, (semantic::Vec2{200, 0}));
}
TEST(SketchExtendedGeometry, HoleAndFillRuleRemainAnExactClipInsteadOfPolygonFlattening) {
    auto path = openCurve();
    path.commands.push_back(semantic::ClosePath{});
    path.commands.insert(path.commands.end(),
                         {semantic::MoveTo{{40, -5}}, semantic::LineTo{{60, -5}},
                          semantic::LineTo{{60, 5}}, semantic::LineTo{{40, 5}},
                          semantic::ClosePath{}});
    auto props = sketchStyle();
    props.entries.insert(
        props.entries.begin(),
        {0x100U, semantic::FillStyleValue{semantic::PatternFillV1{
                     1, {0, 0, 1, 1}, semantic::PatternKind::kCrosshatch, -45, 8, 1}}});
    auto g = scene::sketchGeometry(semantic::VectorPathContent{path}, props);
    ASSERT_TRUE(g);
    EXPECT_EQ(g->body, path);
    EXPECT_FALSE(g->hatches.commands.empty());
    path.fill_rule = semantic::FillRule::kNonZero;
    auto other = scene::sketchGeometry(semantic::VectorPathContent{path}, props);
    ASSERT_TRUE(other);
    EXPECT_EQ(other->body, path);
    EXPECT_NE(other->body.fill_rule, g->body.fill_rule);
}
TEST(SketchExtendedGeometry, TableDrawsEachWholeGridLineOnceAndRetainsUnchangedLineNoise) {
    auto t = table();
    const auto before = scene::sketchGeometry(t, sketchStyle());
    ASSERT_TRUE(before);
    EXPECT_EQ(
        std::count_if(before->outline.commands.begin(), before->outline.commands.end(),
                      [](const auto &c) { return std::holds_alternative<semantic::MoveTo>(c); }),
        14);
    t.rows.push_back({foundation::ObjectId::fromUint64(13), 30});
    t.cells.resize(9);
    const auto after = scene::sketchGeometry(t, sketchStyle());
    ASSERT_TRUE(after);
    ASSERT_GE(after->outline.commands.size(), 12);
    // The original three horizontal boundaries are geometrically unchanged,
    // including the former outer bottom edge which becomes an interior edge.
    EXPECT_EQ(std::vector<semantic::PathCommand>(before->outline.commands.begin(),
                                                 before->outline.commands.begin() + 12),
              std::vector<semantic::PathCommand>(after->outline.commands.begin(),
                                                 after->outline.commands.begin() + 12));
}
TEST(SketchExtendedGeometry, SectionBoundaryUsesItsAuthoredOffsetAndHasStableGeometryBounds) {
    semantic::ObjectRecord r;
    r.kind = semantic::ObjectKind::kSection;
    r.kind_version = 1;
    r.content = semantic::SectionContent{{10, 20, 120, 80}, "Section", 6, 24};
    r.properties = sketchStyle();
    const auto g = scene::sketchGeometry(r.content, r.properties);
    ASSERT_TRUE(g);
    EXPECT_EQ(std::get<semantic::MoveTo>(g->body.commands.front()).point, (semantic::Vec2{10, 20}));
    EXPECT_EQ(scene::computeBounds(r).geometry, (foundation::WorldRect{10, 20, 130, 100}));
}
TEST(SketchExtendedSemantic, NewKindsAdmitVersionedAppearanceAndSectionExplicitNoStroke) {
    for (auto content :
         {semantic::ObjectContent{semantic::VectorPathContent{openCurve()}},
          semantic::ObjectContent{semantic::SectionContent{{10, 20, 120, 80}, "Section", 6, 24}}}) {
        semantic::ObjectRecord r;
        r.id = foundation::ObjectId::fromUint64(10);
        r.kind = std::holds_alternative<semantic::VectorPathContent>(content)
                     ? semantic::ObjectKind::kVectorPath
                     : semantic::ObjectKind::kSection;
        r.kind_version = 1;
        r.placement.order_key = semantic::OrderKey({128});
        r.content = content;
        r.properties = sketchStyle();
        if (r.kind == semantic::ObjectKind::kSection)
            r.properties.entries.insert(r.properties.entries.begin(),
                                        {0x101U, semantic::StrokeStyleValue{semantic::NoStroke{}}});
        semantic::Operation op{semantic::OperationId{foundation::ObjectId::fromUint64(1)},
                               semantic::DocumentId{foundation::ObjectId::fromUint64(1)}, 1, 1,
                               semantic::InsertObjectsOp{{r}}};
        const auto encoded = semantic::SemanticCodec::encodeProtobufOperation(op);
        ASSERT_TRUE(encoded.ok());
        const auto decoded = semantic::SemanticCodec::decodeProtobufOperation(encoded.bytes);
        ASSERT_TRUE(decoded.ok());
        EXPECT_EQ(decoded.operation.payload, op.payload);
    }
}
TEST(SketchExtendedSemantic, SnapshotRequiresNewObjectCapabilityEvenForDisabledStyle) {
    semantic::ObjectRecord r;
    r.id = foundation::ObjectId::fromUint64(10);
    r.kind = semantic::ObjectKind::kVectorPath;
    r.kind_version = 1;
    r.placement.order_key = semantic::OrderKey({128});
    r.content = semantic::VectorPathContent{openCurve()};
    r.properties = sketchStyle();
    std::get<semantic::SketchStyleV1>(r.properties.entries[0].value).enabled = false;
    semantic::SemanticSnapshot s;
    s.document_id = semantic::DocumentId{foundation::ObjectId::fromUint64(1)};
    s.schema_version = 3;
    s.objects = {r};
    s.manifest_present = true;
    s.manifest.schema_version = 1;
    s.document_revision = 1;
    s.digest_algorithm = 1;
    s.recovery_frontier = {8, 1, 16, 1, 24, 1};
    ASSERT_TRUE(semantic::SnapshotCodec::populateRequirements(s));
    const auto profile = static_cast<semantic::SnapshotCapability>(511);
    ASSERT_TRUE(semantic::SnapshotCodec::bindDigests(s, profile));
    const auto saved = semantic::SnapshotCodec::encode(s, profile);
    ASSERT_TRUE(saved.ok());
    EXPECT_FALSE(
        semantic::SnapshotCodec::decode(saved.bytes, semantic::SnapshotCapability::kAllWithSketchV1)
            .ok());
    EXPECT_TRUE(semantic::SnapshotCodec::decode(saved.bytes, profile).ok());
}
TEST(SketchExtendedRuntime, TableUsesSharedAppearanceSessionWithoutDuplicatingGridPaint) {
    runtime::RuntimeComposition owner;
    ASSERT_TRUE(owner.openDocument(foundation::ObjectId::fromUint64(1)));
    ASSERT_TRUE(owner.bindSurface(800, 600));
    auto &api = owner.controlFacade();
    ASSERT_TRUE(
        api.setMultiSelectionPermission(runtime::MultiSelectionPermissionMode::kLocalSingleUser));
    runtime::AppearanceRequest defaults;
    defaults.action = runtime::AppearanceAction::kDefaults;
    defaults.patch.enabled = true;
    defaults.patch.roughness = 2;
    ASSERT_TRUE(api.appearanceCommand(defaults).applied);
    runtime::TableRequest create;
    create.rows = 2;
    create.columns = 2;
    const auto created = api.tableCommand(create);
    ASSERT_TRUE(created.applied);
    const auto selected = api.readAppearance();
    ASSERT_EQ(selected.eligible.size(), 1);
    EXPECT_TRUE(selected.fillEligible.empty());
    EXPECT_TRUE(selected.values.enabled);
    EXPECT_GT(selected.values.seed, 0);
    EXPECT_DOUBLE_EQ(selected.values.roughness, 2);
    const auto grid = api.readTable().content.grid;
    runtime::AppearanceRequest r;
    r.action = runtime::AppearanceAction::kBegin;
    r.stamp = selected.stamp;
    r.eligible = selected.eligible;
    const auto begin = api.appearanceCommand(r);
    ASSERT_TRUE(begin.applied);
    r = {};
    r.action = runtime::AppearanceAction::kPreview;
    r.token = begin.token;
    r.patch.roughness = 1.25;
    ASSERT_TRUE(api.appearanceCommand(r).applied);
    EXPECT_EQ(api.readAppearance().stamp.semanticGeneration, selected.stamp.semanticGeneration);
    r = {};
    r.action = runtime::AppearanceAction::kCommit;
    r.token = begin.token;
    ASSERT_TRUE(api.appearanceCommand(r).changed);
    EXPECT_EQ(api.readAppearance().stamp.semanticGeneration, selected.stamp.semanticGeneration + 1);
    EXPECT_EQ(api.readAppearance().values.seed, selected.values.seed);
    EXPECT_EQ(api.readTable().content.grid, grid);
    runtime::CanvasControlRequest undo;
    undo.target = api.mountedCanvasTargets().front();
    undo.clientId = 99;
    undo.requestId = 1;
    undo.payload.kind = runtime::CanvasControlPayloadKind::kUndo;
    ASSERT_EQ(api.submitCanvasControl(undo).state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(owner.ownerPump());
    EXPECT_DOUBLE_EQ(api.readAppearance().values.roughness, 2);
    EXPECT_EQ(api.readAppearance().values.seed, selected.values.seed);
    const auto current = api.readAppearance();
    r = {};
    r.stamp = current.stamp;
    r.eligible = current.eligible;
    r.patch.strokeWidth = 5;
    EXPECT_FALSE(api.appearanceCommand(r).applied);
    EXPECT_EQ(api.readTable().content.grid, grid);
}
TEST(SketchExtendedRuntime, SectionBoundaryAppearanceDoesNotChangeItsContentOrMembership) {
    runtime::RuntimeComposition owner;
    ASSERT_TRUE(owner.openDocument(foundation::ObjectId::fromUint64(1)));
    ASSERT_TRUE(owner.bindSurface(800, 600));
    auto &api = owner.controlFacade();
    ASSERT_TRUE(
        api.setMultiSelectionPermission(runtime::MultiSelectionPermissionMode::kLocalSingleUser));
    semantic::ObjectRecord section;
    section.id = foundation::ObjectId::fromUint64(10);
    section.kind = semantic::ObjectKind::kSection;
    section.kind_version = 1;
    section.placement.order_key = semantic::OrderKey({128});
    section.content = semantic::SectionContent{{10, 20, 200, 120}, "Title", 12, 32};
    semantic::Operation insert{semantic::OperationId{foundation::ObjectId::fromUint64(1)},
                               semantic::DocumentId{foundation::ObjectId::fromUint64(1)}, 1, 1,
                               semantic::InsertObjectsOp{{section}}};
    ASSERT_EQ(owner.applyRemoteOperation(insert).disposition, semantic::ApplyDisposition::kApplied);
    runtime::SectionIntent select;
    select.action = runtime::SectionAction::kSelect;
    select.objectId = section.id;
    ASSERT_EQ(api.submitSectionIntent(select, 1, 99).state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(owner.ownerPump());
    const auto original = api.readSection(section.id);
    const auto selected = api.readAppearance();
    ASSERT_EQ(selected.eligible.size(), 1);
    runtime::AppearanceRequest r;
    r.stamp = selected.stamp;
    r.eligible = selected.eligible;
    r.patch.enabled = true;
    r.patch.roughness = 2;
    r.patch.strokeEnabled = false;
    ASSERT_TRUE(api.appearanceCommand(r).applied);
    EXPECT_FALSE(api.readAppearance().values.strokeEnabled);
    const auto changed = api.readSection(section.id);
    EXPECT_EQ(changed.content, original.content);
    EXPECT_EQ(changed.directChildren, original.directChildren);
}
namespace {
std::shared_ptr<const render::DecodedImage> image(runtime::RuntimeFacade &api) {
    runtime::ImageExportOptions options;
    options.bounds = std::array<double, 4>{0, 0, 256, 256};
    options.padding = 0;
    const auto result = api.exportImage(options);
    if (result.status != runtime::ImageExportStatus::kExported)
        return {};
    runtime::MemoryResourceProvider resources;
    const auto id = foundation::ObjectId::fromUint64(99);
    resources.publish(id, result.bytes);
    render::ImageResourceResolver decoder(resources);
    return decoder.resolve(semantic::ResourceId{id});
}
std::size_t painted(const render::DecodedImage &p, unsigned l, unsigned t, unsigned r, unsigned b) {
    std::size_t n = 0;
    for (unsigned y = t; y < b; ++y)
        for (unsigned x = l; x < r; ++x) {
            const auto i = (y * p.width + x) * 4;
            if (p.rgba[i] < 240 || p.rgba[i + 1] < 240 || p.rgba[i + 2] < 240)
                ++n;
        }
    return n;
}
} // namespace
TEST(SketchExtendedRender, PatternUsesExactEvenOddHoleAndLeavesGapsBetweenHatches) {
    runtime::RuntimeComposition owner;
    ASSERT_TRUE(owner.openDocument(foundation::ObjectId::fromUint64(1)));
    ASSERT_TRUE(owner.bindSurface(256, 256));
    semantic::ObjectRecord r;
    r.id = foundation::ObjectId::fromUint64(10);
    r.kind = semantic::ObjectKind::kVectorPath;
    r.kind_version = 1;
    r.placement.order_key = semantic::OrderKey({128});
    r.content = semantic::VectorPathContent{
        {semantic::FillRule::kEvenOdd,
         {semantic::MoveTo{{20, 20}}, semantic::LineTo{{180, 20}}, semantic::LineTo{{180, 180}},
          semantic::LineTo{{20, 180}}, semantic::ClosePath{}, semantic::MoveTo{{60, 60}},
          semantic::LineTo{{140, 60}}, semantic::LineTo{{140, 140}}, semantic::LineTo{{60, 140}},
          semantic::ClosePath{}}}};
    r.properties = sketchStyle(2);
    r.properties.entries.insert(
        r.properties.entries.begin(),
        {0x100U, semantic::FillStyleValue{semantic::PatternFillV1{
                     1, {1, 0, 0, 1}, semantic::PatternKind::kDiagonal, -45, 8, 1}}});
    semantic::Operation op{semantic::OperationId{foundation::ObjectId::fromUint64(1)},
                           semantic::DocumentId{foundation::ObjectId::fromUint64(1)}, 1, 1,
                           semantic::InsertObjectsOp{{r}}};
    ASSERT_EQ(owner.applyRemoteOperation(op).disposition, semantic::ApplyDisposition::kApplied);
    const auto pixels = image(owner.controlFacade());
    ASSERT_TRUE(pixels);
    EXPECT_EQ(painted(*pixels, 75, 75, 125, 125), 0);
    const auto n = painted(*pixels, 25, 25, 55, 55);
    EXPECT_GT(n, 20);
    EXPECT_LT(n, 650);
}
TEST(SketchExtendedRender, SectionExplicitNoStrokeHidesOnlyItsDocumentBorder) {
    runtime::RuntimeComposition owner;
    ASSERT_TRUE(owner.openDocument(foundation::ObjectId::fromUint64(1)));
    ASSERT_TRUE(owner.bindSurface(256, 256));
    semantic::ObjectRecord r;
    r.id = foundation::ObjectId::fromUint64(10);
    r.kind = semantic::ObjectKind::kSection;
    r.kind_version = 1;
    r.placement.order_key = semantic::OrderKey({128});
    r.content = semantic::SectionContent{{20, 20, 160, 160}, "", 8, 24};
    r.properties = sketchStyle(2);
    r.properties.entries.insert(r.properties.entries.begin(),
                                {0x101U, semantic::StrokeStyleValue{semantic::NoStroke{}}});
    semantic::Operation op{semantic::OperationId{foundation::ObjectId::fromUint64(1)},
                           semantic::DocumentId{foundation::ObjectId::fromUint64(1)}, 1, 1,
                           semantic::InsertObjectsOp{{r}}};
    ASSERT_EQ(owner.applyRemoteOperation(op).disposition, semantic::ApplyDisposition::kApplied);
    const auto pixels = image(owner.controlFacade());
    ASSERT_TRUE(pixels);
    EXPECT_EQ(painted(*pixels, 0, 0, 220, 220), 0);
}
TEST(SketchExtendedRuntime, PublicVectorPathCreationUsesOwnerHistoryAndBoundedCanonicalGeometry) {
    runtime::RuntimeComposition owner;
    ASSERT_TRUE(owner.openDocument(foundation::ObjectId::fromUint64(1)));
    ASSERT_TRUE(owner.bindSurface(800, 600));
    auto &api = owner.controlFacade();
    runtime::VectorPathCreateRequest request;
    request.objectId = foundation::ObjectId::fromUint64(100);
    request.geometry = openCurve();
    request.transform.tx = 35;
    request.transform.ty = 180;
    ASSERT_TRUE(api.createVectorPath(request).applied);
    const auto read = api.readVectorPath();
    ASSERT_TRUE(read.available);
    EXPECT_EQ(read.objectId, request.objectId);
    EXPECT_EQ(read.geometry, request.geometry);
    EXPECT_EQ(read.transform, request.transform);
    EXPECT_NE(read.appearance.seed, 0U);
    EXPECT_FALSE(read.appearance.enabled);
    const auto generation = api.readSelectionSummary().semanticGeneration;
    request.objectId = foundation::ObjectId::fromUint64(101);
    request.geometry.commands.resize(4097, semantic::LineTo{{1, 1}});
    EXPECT_FALSE(api.createVectorPath(request).applied);
    EXPECT_EQ(api.readSelectionSummary().semanticGeneration, generation);
    runtime::CanvasControlRequest undo;
    undo.target = api.mountedCanvasTargets().front();
    undo.clientId = 99;
    undo.requestId = 1;
    undo.payload.kind = runtime::CanvasControlPayloadKind::kUndo;
    ASSERT_EQ(api.submitCanvasControl(undo).state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(owner.ownerPump());
    EXPECT_FALSE(api.readVectorPath(read.objectId).available);
}
TEST(SketchExtendedRuntime, FilledPathHoleDoesNotAcquireItsBoundingBox) {
    runtime::RuntimeComposition owner;
    ASSERT_TRUE(owner.openDocument(foundation::ObjectId::fromUint64(1)));
    ASSERT_TRUE(owner.bindSurface(256, 256));
    auto &api = owner.controlFacade();
    runtime::VectorPathCreateRequest r;
    r.objectId = foundation::ObjectId::fromUint64(100);
    r.geometry = {semantic::FillRule::kEvenOdd,
                  {semantic::MoveTo{{20, 20}}, semantic::LineTo{{180, 20}},
                   semantic::LineTo{{180, 180}}, semantic::LineTo{{20, 180}}, semantic::ClosePath{},
                   semantic::MoveTo{{60, 60}}, semantic::LineTo{{140, 60}},
                   semantic::LineTo{{140, 140}}, semantic::LineTo{{60, 140}},
                   semantic::ClosePath{}}};
    ASSERT_TRUE(api.createVectorPath(r).applied);
    runtime::CanvasControlRequest tool;
    tool.target = api.mountedCanvasTargets().front();
    tool.clientId = 99;
    tool.requestId = 1;
    tool.payload.kind = runtime::CanvasControlPayloadKind::kSelectTool;
    tool.payload.tool = runtime::CanvasToolKind::kSelect;
    ASSERT_EQ(api.submitCanvasControl(tool).state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(owner.ownerPump());
    ASSERT_TRUE(owner.registerSource(1));
    input::PlatformPointerSample sample;
    sample.source = 1;
    sample.pointer = 1;
    sample.sequence = 1;
    sample.timestampNs = 1;
    sample.x = 100;
    sample.y = 100;
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
    EXPECT_TRUE(api.readSelectionSummary().primary.isZero());
}

TEST(SketchExtendedGeometry, UnknownVersionsAndInvalidSeedsNeverGenerateExtendedPaths) {
    auto props = sketchStyle();
    std::get<semantic::SketchStyleV1>(props.entries[0].value).version = 2;
    EXPECT_FALSE(scene::sketchGeometry(semantic::VectorPathContent{openCurve()}, props));
    std::get<semantic::SketchStyleV1>(props.entries[0].value) = {1, 0, 2, true};
    EXPECT_FALSE(scene::sketchGeometry(table(), props));
    props = sketchStyle();
    props.entries.insert(
        props.entries.begin(),
        {0x100U, semantic::FillStyleValue{semantic::PatternFillV1{
                     2, {0, 0, 1, 1}, semantic::PatternKind::kDiagonal, -45, 8, 1}}});
    EXPECT_FALSE(
        scene::sketchGeometry(semantic::SectionContent{{0, 0, 100, 80}, "", 8, 24}, props));
}

TEST(SketchExtendedRuntime, SectionRegionAcquiresVisibleBoundaryOutsideAuthoredBody) {
    semantic::ObjectRecord o;
    o.kind = semantic::ObjectKind::kSection;
    o.kind_version = 1;
    o.content = semantic::SectionContent{{0, 0, 120, 80}, "", 8, 24};
    o.properties = sketchStyle();
    o.properties.entries.insert(
        o.properties.entries.begin(),
        {0x101U, semantic::StrokeStyleValue{semantic::SolidStroke{{0, 0, 1, 1}, 8}}});
    const std::array<interaction::RegionPoint, 4> region{{{-3, 38}, {-1, 38}, {-1, 42}, {-3, 42}}};
    EXPECT_EQ(interaction::intersectSelectionRegion(o, region),
              interaction::RegionIntersection::kHit);
}

TEST(SketchExtendedRuntime, UnversionedPathKeepsItsFillUntilAnActualAppearanceChange) {
    runtime::RuntimeComposition owner;
    const auto document = foundation::ObjectId::fromUint64(1);
    ASSERT_TRUE(owner.openDocument(document));
    ASSERT_TRUE(owner.bindSurface(256, 256));
    auto &api = owner.controlFacade();
    ASSERT_TRUE(
        api.setMultiSelectionPermission(runtime::MultiSelectionPermissionMode::kLocalSingleUser));
    semantic::ObjectRecord o;
    o.id = foundation::ObjectId::fromUint64(10);
    o.kind = semantic::ObjectKind::kVectorPath;
    o.kind_version = 1;
    o.placement.order_key = semantic::OrderKey({128});
    o.content = semantic::VectorPathContent{
        {semantic::FillRule::kNonZero,
         {semantic::MoveTo{{20, 20}}, semantic::LineTo{{180, 20}}, semantic::LineTo{{180, 180}},
          semantic::LineTo{{20, 180}}, semantic::ClosePath{}}}};
    const semantic::Operation operation{semantic::OperationId{foundation::ObjectId::fromUint64(99)},
                                        semantic::DocumentId{document}, 1, 1,
                                        semantic::InsertObjectsOp{{o}}};
    ASSERT_EQ(owner.applyRemoteOperation(operation).disposition,
              semantic::ApplyDisposition::kApplied);
    runtime::CanvasControlRequest tool;
    tool.target = api.mountedCanvasTargets().front();
    tool.clientId = 99;
    tool.requestId = 1;
    tool.payload.kind = runtime::CanvasControlPayloadKind::kSelectTool;
    tool.payload.tool = runtime::CanvasToolKind::kSelect;
    ASSERT_EQ(api.submitCanvasControl(tool).state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(owner.ownerPump());
    ASSERT_TRUE(owner.registerSource(1));
    input::PlatformPointerSample sample;
    sample.source = 1;
    sample.pointer = 1;
    sample.sequence = 1;
    sample.timestampNs = 1;
    sample.x = 100;
    sample.y = 100;
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
    const auto selected = api.readAppearance();
    ASSERT_EQ(selected.values.fillKind, runtime::AppearanceFill::kSolid);
    EXPECT_EQ(selected.values.fill, (semantic::ColorValue{0, 0, 1, 1}));
    runtime::AppearanceRequest edit;
    edit.action = runtime::AppearanceAction::kPatch;
    edit.stamp = selected.stamp;
    edit.eligible = selected.eligible;
    edit.patch.opacity = 1;
    ASSERT_TRUE(api.appearanceCommand(edit).applied);
    EXPECT_EQ(api.readAppearance().values.seed, 0U);
    edit.stamp = api.readAppearance().stamp;
    edit.patch = {};
    edit.patch.fillKind = runtime::AppearanceFill::kNone;
    ASSERT_TRUE(api.appearanceCommand(edit).applied);
    EXPECT_NE(api.readAppearance().values.seed, 0U);
    const auto pixels = image(api);
    ASSERT_TRUE(pixels);
    EXPECT_EQ(painted(*pixels, 0, 0, 220, 220), 0);
}
