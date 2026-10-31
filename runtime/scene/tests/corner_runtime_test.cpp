#include "canvas/runtime/runtime_composition.hpp"
#include <gtest/gtest.h>
using namespace canvas;
TEST(CornerRuntime, PreviewCommitsOnceUndoRestoresAbsenceAndSeedStaysStable) {
    runtime::RuntimeComposition owner;
    const auto document = foundation::ObjectId::fromUint64(1);
    ASSERT_TRUE(owner.openDocument(document));
    ASSERT_TRUE(owner.bindSurface(400, 300));
    auto &api = owner.controlFacade();
    ASSERT_TRUE(
        api.setMultiSelectionPermission(runtime::MultiSelectionPermissionMode::kLocalSingleUser));
    semantic::ObjectRecord o;
    o.id = foundation::ObjectId::fromUint64(10);
    o.kind = semantic::ObjectKind::kShape;
    o.kind_version = 1;
    o.placement.order_key = semantic::OrderKey({128});
    o.content = semantic::ShapeContent{1, 120, 80};
    o.transform.tx = o.transform.ty = 20;
    o.properties.entries = {
        {0x100U, semantic::FillStyleValue{semantic::SolidFill{{.2F, .5F, .8F, 1}}}},
        {0x102U, semantic::SketchStyleV1{1, 9999, 2, true}}};
    semantic::Operation operation{semantic::OperationId{foundation::ObjectId::fromUint64(2)},
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
    sample.x = 80;
    sample.y = 60;
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
    const auto before = api.readAppearance();
    ASSERT_EQ(before.radiusEligible.size(), 1);
    runtime::AppearanceRequest edit;
    edit.action = runtime::AppearanceAction::kBegin;
    edit.stamp = before.stamp;
    edit.eligible = before.eligible;
    const auto begin = api.appearanceCommand(edit);
    ASSERT_TRUE(begin.applied);
    edit.token = begin.token;
    edit.action = runtime::AppearanceAction::kPreview;
    for (double radius : {8., 24., 32.}) {
        edit.patch.cornerRadius = radius;
        ASSERT_TRUE(api.appearanceCommand(edit).applied);
    }
    EXPECT_EQ(api.readAppearance().values.cornerRadius, 0);
    EXPECT_EQ(api.readAppearance().stamp.semanticGeneration, before.stamp.semanticGeneration);
    edit.action = runtime::AppearanceAction::kCommit;
    ASSERT_TRUE(api.appearanceCommand(edit).applied);
    EXPECT_EQ(api.readAppearance().values.cornerRadius, 32);
    EXPECT_EQ(api.readAppearance().stamp.semanticGeneration, before.stamp.semanticGeneration + 1);
    EXPECT_EQ(api.readAppearance().values.seed, 9999);
    tool.payload.kind = runtime::CanvasControlPayloadKind::kUndo;
    tool.requestId = 2;
    ASSERT_EQ(api.submitCanvasControl(tool).state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(owner.ownerPump());
    EXPECT_EQ(api.readAppearance().values.cornerRadius, 0);
    EXPECT_EQ(api.readAppearance().values.seed, 9999);
    tool.payload.kind = runtime::CanvasControlPayloadKind::kRedo;
    tool.requestId = 3;
    ASSERT_EQ(api.submitCanvasControl(tool).state, runtime::ProductControlState::kQueued);
    ASSERT_TRUE(owner.ownerPump());
    EXPECT_EQ(api.readAppearance().values.cornerRadius, 32);
}
