#include "canvas/runtime/runtime_composition.hpp"
#include <gtest/gtest.h>
#include <algorithm>

namespace canvas::ink_playground {
class InkPlaygroundHistoryTestAccess {
public:
    static std::size_t historySize(const interaction::CanvasEditingRuntime& runtime) {return runtime.history_.size();}
};
}

namespace {
using namespace canvas;
class SelectionInput : public testing::Test {
protected:
    runtime::RuntimeComposition c;
    runtime::RuntimeFacade& f=c.controlFacade();
    std::uint64_t seq=0,request=0;
    const foundation::ObjectId a=foundation::ObjectId::fromUint64(9101);
    const foundation::ObjectId b=foundation::ObjectId::fromUint64(9102);
    void SetUp() override {
        ASSERT_TRUE(c.open());ASSERT_TRUE(c.bindSurface(800,600));ASSERT_TRUE(c.registerSource(9100));
        semantic::InsertObjectsOp insert;
        for (unsigned i=0;i<3;++i) {
            semantic::ObjectRecord r;r.id=foundation::ObjectId::fromUint64(9101+i);
            r.kind=semantic::ObjectKind::kShape;r.kind_version=i==2?2:1;
            r.content=semantic::ShapeContent{i+1,80,60};r.placement.order_key=semantic::OrderKey({static_cast<std::uint8_t>(i+1)});
            r.transform.tx=100+i*160;r.transform.ty=100;insert.objects.push_back(r);
        }
        semantic::Operation op{semantic::OperationId{foundation::ObjectId::fromUint64(9200)},
            semantic::DocumentId{foundation::ObjectId::fromUint64(1)},1,1,insert};
        ASSERT_EQ(c.applyExternalOperation(op,semantic::ApplySource::kRemoteSync).disposition,semantic::ApplyDisposition::kApplied);
        ASSERT_EQ(f.setSelectionMode(true,++request,f.readRuntimeState().identity.runtimeGeneration,9100).state,runtime::ProductControlState::kQueued);
        ASSERT_TRUE(c.ownerPump());
    }
    runtime::CanvasControlSnapshot snapshot() {return f.readCanvasControlSnapshot(f.mountedCanvasTargets().front());}
    std::vector<foundation::ObjectId> selected() {return f.readSection().selectedRoots;}
    bool pointer(input::PointerPhase phase,float x,float y,bool shift=false,std::uint64_t pointerId=1,
                 std::uint64_t source=9100,input::PointerTool tool=input::PointerTool::kUnknown) {
        const auto s=snapshot();input::PlatformPointerBatch batch;
        input::PlatformPointerSample p;p.source=source;p.pointer=pointerId;p.sequence=++seq;p.timestampNs=seq*1000000;
        p.phase=phase;p.x=x*s.cameraScale+s.cameraTranslationX;p.y=y*s.cameraScale+s.cameraTranslationY;
        p.modifiers=shift?input::PointerModifiers::kShift:input::PointerModifiers::kNone;p.capabilities.tool=tool;
        batch.samples.push_back(p);return c.submit(batch);
    }
    void click(float x,float y,bool shift=false) {
        ASSERT_TRUE(pointer(input::PointerPhase::kDown,x,y,shift));ASSERT_TRUE(pointer(input::PointerPhase::kUp,x,y,shift));
    }
    void rectangle(float x,float y,float r,float btm,bool shift=false) {
        ASSERT_TRUE(pointer(input::PointerPhase::kDown,x,y,shift));
        ASSERT_TRUE(pointer(input::PointerPhase::kMove,r,btm));ASSERT_TRUE(pointer(input::PointerPhase::kUp,r,btm));
    }
    void mode(runtime::CanvasSelectionMode value) {
        ASSERT_EQ(f.setSelectionAcquisitionMode(value,++request,9100).state,runtime::ProductControlState::kQueued);
        ASSERT_TRUE(c.ownerPump());ASSERT_EQ(snapshot().selectionAcquisitionMode,value);
        ASSERT_EQ(f.readRuntimeState().control.selectionAcquisitionMode,value);
    }
    void TearDown() override {EXPECT_TRUE(c.close());}
};
TEST_F(SelectionInput, BlankDownIsPendingUpClearsAndShiftEmptyPreserves) {
    click(140,130);ASSERT_EQ(selected(),std::vector{a});
    ASSERT_TRUE(pointer(input::PointerPhase::kDown,20,20));EXPECT_EQ(selected(),std::vector{a});
    ASSERT_TRUE(pointer(input::PointerPhase::kUp,20,20));EXPECT_TRUE(selected().empty());
    click(140,130);click(20,20,true);EXPECT_EQ(selected(),std::vector{a});
}
TEST_F(SelectionInput, BlankDragUsesPreciseRegionAndShiftDownIsLatched) {
    rectangle(90,90,185,165);EXPECT_EQ(selected(),std::vector{a});
    rectangle(250,90,345,165,true);EXPECT_EQ(selected(),(std::vector{a,b}));
    rectangle(250,90,345,165,true);EXPECT_EQ(selected(),(std::vector{a,b}));
}
TEST_F(SelectionInput, CancelAndLateUpPreserveSelectionAndDocument) {
    click(140,130);const auto before=snapshot();
    ASSERT_TRUE(pointer(input::PointerPhase::kDown,20,20));ASSERT_TRUE(pointer(input::PointerPhase::kMove,500,200));
    (void)pointer(input::PointerPhase::kCancel,500,200);(void)pointer(input::PointerPhase::kUp,500,200);
    EXPECT_EQ(selected(),std::vector{a});const auto after=snapshot();
    EXPECT_EQ(after.semanticGeneration,before.semanticGeneration);EXPECT_EQ(after.lastAppliedOperation,before.lastAppliedOperation);
    EXPECT_EQ(after.canUndo,before.canUndo);EXPECT_EQ(after.canRedo,before.canRedo);
    EXPECT_EQ(c.semanticObjectCount(),3U);
}
TEST_F(SelectionInput, SwitchToHandPreservesSelection) {
    click(140,130);
    ASSERT_EQ(f.setTool(4103,++request,f.readRuntimeState().identity.runtimeGeneration).state,runtime::ProductControlState::kQueued);
    ASSERT_TRUE(c.ownerPump());EXPECT_EQ(selected(),std::vector{a});
    ASSERT_EQ(f.setSelectionMode(true,++request,f.readRuntimeState().identity.runtimeGeneration,9100).state,runtime::ProductControlState::kQueued);
    ASSERT_TRUE(c.ownerPump());EXPECT_EQ(selected(),std::vector{a});
}
TEST_F(SelectionInput, ExplicitRegionStartsOverObjectWithoutTransformingIt) {
    click(140,130);const auto before=snapshot();mode(runtime::CanvasSelectionMode::kRectangle);
    rectangle(120,120,330,155);EXPECT_EQ(selected(),(std::vector{b,a}));
    EXPECT_EQ(snapshot().semanticGeneration,before.semanticGeneration);
    mode(runtime::CanvasSelectionMode::kFreeform);
    ASSERT_TRUE(pointer(input::PointerPhase::kDown,90,90));
    ASSERT_TRUE(pointer(input::PointerPhase::kMove,185,90));ASSERT_TRUE(pointer(input::PointerPhase::kMove,185,165));
    ASSERT_TRUE(pointer(input::PointerPhase::kUp,90,165));EXPECT_EQ(selected(),std::vector{a});
}
TEST_F(SelectionInput, EllipseTriangleCornersAndZoomDprHaveSamePreciseResult) {
    mode(runtime::CanvasSelectionMode::kRectangle);
    rectangle(260,100,264,104);EXPECT_TRUE(selected().empty());
    rectangle(420,100,424,104);EXPECT_TRUE(selected().empty());
    runtime::CanvasControlRequest q;q.target=f.mountedCanvasTargets().front();q.clientId=9100;q.requestId=++request;
    q.payload.kind=runtime::CanvasControlPayloadKind::kSetZoom;q.payload.scale=2;q.payload.x=400;q.payload.y=300;
    ASSERT_EQ(f.submitCanvasControl(q).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
    ASSERT_TRUE(c.resizeSurface(render::SurfaceMetrics{800,600,1600,1200,2,1}));
    rectangle(90,90,185,165);EXPECT_EQ(selected(),std::vector{a});
    rectangle(260,100,264,104);EXPECT_TRUE(selected().empty());
}
TEST_F(SelectionInput, NewSelectionAndDeletionCannotBeOverwrittenByOldRegion) {
    const auto section=foundation::ObjectId::fromUint64(9301);
    runtime::SectionIntent create;create.objectId=section;create.transform.tx=620;create.transform.ty=300;
    ASSERT_EQ(f.submitSectionIntent(create,++request,9100).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
    click(140,130);ASSERT_TRUE(pointer(input::PointerPhase::kDown,20,20));
    ASSERT_TRUE(pointer(input::PointerPhase::kMove,500,200));
    runtime::SectionIntent select;select.action=runtime::SectionAction::kSelect;select.objectId=section;
    ASSERT_EQ(f.submitSectionIntent(select,++request,9100).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
    const auto receipt=f.canvasControlReceipt({9100,request});ASSERT_TRUE(receipt&&receipt->section);
    ASSERT_EQ(receipt->section->failure,runtime::SectionFailure::kNone);
    ASSERT_TRUE(pointer(input::PointerPhase::kUp,500,200));EXPECT_EQ(selected(),std::vector{section});
    ASSERT_EQ(snapshot().selectionQueryStatus,runtime::SelectionQueryStatus::kStale);
    runtime::RuntimeFrameOpportunity frame;ASSERT_TRUE(c.serviceFrameOpportunity(true,frame));
    EXPECT_FALSE(c.qualificationRuntime().previewSurfaceProvider()->overlayVisible());
}
TEST_F(SelectionInput, MetricsAndSourceLossCancelOnlyOwnedRegion) {
    click(140,130);ASSERT_TRUE(pointer(input::PointerPhase::kDown,20,20));
    ASSERT_TRUE(pointer(input::PointerPhase::kMove,500,200));
    ASSERT_TRUE(c.registerSource(9109));ASSERT_TRUE(c.unregisterSource(9109,++seq,seq*1000000));
    EXPECT_TRUE(snapshot().regionSelectionActive);
    ASSERT_TRUE(c.resizeSurface(810,610));(void)pointer(input::PointerPhase::kUp,500,200);
    EXPECT_EQ(selected(),std::vector{a});EXPECT_FALSE(snapshot().regionSelectionActive);
    ASSERT_TRUE(pointer(input::PointerPhase::kDown,20,20));
    ASSERT_TRUE(c.unregisterSource(9100,++seq,seq*1000000));
    EXPECT_FALSE(snapshot().regionSelectionActive);EXPECT_EQ(selected(),std::vector{a});
}
TEST_F(SelectionInput, RegionDoesNotConsumeRedoOrChangeCanonicalOutcome) {
    runtime::StickyCreateRequest create;create.objectId=foundation::ObjectId::fromUint64(9300);
    ASSERT_EQ(f.createSticky(create).state,runtime::ConnectorCommandState::kApplied);
    const auto expectedSticky=*c.qualificationRuntime().qualificationObjectRecord(create.objectId);
    ASSERT_EQ(f.undo(++request,f.readRuntimeState().identity.runtimeGeneration,9100).state,runtime::ProductControlState::kQueued);
    ASSERT_TRUE(c.ownerPump());ASSERT_TRUE(snapshot().canRedo);const auto before=snapshot();
    const auto historySize=ink_playground::InkPlaygroundHistoryTestAccess::historySize(c.qualificationRuntime());
    std::vector<semantic::ObjectRecord> records;
    for(std::uint64_t i=9101;i<=9103;++i)records.push_back(*c.qualificationRuntime().qualificationObjectRecord(foundation::ObjectId::fromUint64(i)));
    rectangle(90,90,185,165);rectangle(250,90,345,165,true);
    const auto after=snapshot();EXPECT_EQ(after.semanticGeneration,before.semanticGeneration);
    EXPECT_EQ(after.lastAppliedOperation,before.lastAppliedOperation);EXPECT_EQ(after.appliedCommitEpoch,before.appliedCommitEpoch);
    EXPECT_EQ(after.appliedCommitOrdinal,before.appliedCommitOrdinal);EXPECT_EQ(after.canUndo,before.canUndo);EXPECT_EQ(after.canRedo,before.canRedo);
    EXPECT_EQ(ink_playground::InkPlaygroundHistoryTestAccess::historySize(c.qualificationRuntime()),historySize);
    for(const auto& record:records)EXPECT_EQ(*c.qualificationRuntime().qualificationObjectRecord(record.id),record);
    ASSERT_EQ(f.redo(++request,f.readRuntimeState().identity.runtimeGeneration,9100).state,runtime::ProductControlState::kQueued);
    ASSERT_TRUE(c.ownerPump());EXPECT_EQ(c.semanticObjectCount(),4U);
    EXPECT_EQ(*c.qualificationRuntime().qualificationObjectRecord(create.objectId),expectedSticky);
}
TEST_F(SelectionInput, DeletingOriginalSelectionDoesNotRestoreDeletedIdentity) {
    click(140,130);ASSERT_TRUE(pointer(input::PointerPhase::kDown,20,20));
    ASSERT_TRUE(pointer(input::PointerPhase::kMove,500,200));
    semantic::Operation op{semantic::OperationId{foundation::ObjectId::fromUint64(9400)},
        semantic::DocumentId{foundation::ObjectId::fromUint64(1)},1,1,semantic::DeleteObjectsOp{{a}}};
    ASSERT_EQ(c.applyExternalOperation(op,semantic::ApplySource::kRemoteSync).disposition,semantic::ApplyDisposition::kApplied);
    ASSERT_TRUE(pointer(input::PointerPhase::kUp,500,200));EXPECT_TRUE(selected().empty());
    EXPECT_EQ(c.semanticObjectCount(),2U);
    runtime::RuntimeFrameOpportunity frame;ASSERT_TRUE(c.serviceFrameOpportunity(true,frame));
    EXPECT_FALSE(c.qualificationRuntime().previewSurfaceProvider()->overlayVisible());
}
TEST_F(SelectionInput, SceneMutationRequeriesAtUpButCameraMutationCancels) {
    mode(runtime::CanvasSelectionMode::kRectangle);
    ASSERT_TRUE(pointer(input::PointerPhase::kDown,90,90));ASSERT_TRUE(pointer(input::PointerPhase::kMove,185,165));
    semantic::Operation op{semantic::OperationId{foundation::ObjectId::fromUint64(9401)},
        semantic::DocumentId{foundation::ObjectId::fromUint64(1)},1,1,semantic::SetTransformsOp{{{b,{1,0,0,1,100,100}}}}};
    ASSERT_EQ(c.applyExternalOperation(op,semantic::ApplySource::kRemoteSync).disposition,semantic::ApplyDisposition::kApplied);
    ASSERT_TRUE(pointer(input::PointerPhase::kUp,185,165));EXPECT_EQ(selected(),(std::vector{b,a}));
    ASSERT_TRUE(pointer(input::PointerPhase::kDown,20,20));ASSERT_TRUE(pointer(input::PointerPhase::kMove,500,200));
    runtime::CanvasControlRequest q;q.target=f.mountedCanvasTargets().front();q.clientId=9100;q.requestId=++request;
    q.payload.kind=runtime::CanvasControlPayloadKind::kZoomAt;q.payload.scale=2;
    ASSERT_EQ(f.submitCanvasControl(q).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
    ASSERT_TRUE(pointer(input::PointerPhase::kUp,500,200));EXPECT_EQ(selected(),(std::vector{b,a}));
}
TEST_F(SelectionInput, SecondFingerNavigationCancelsRegionBeforeTakingOver) {
    click(140,130);mode(runtime::CanvasSelectionMode::kRectangle);const auto before=snapshot();
    ASSERT_TRUE(pointer(input::PointerPhase::kDown,20,20,false,1,9100,input::PointerTool::kFinger));
    // Region slop is crossed, while the existing 8px Ink slop is not.
    ASSERT_TRUE(pointer(input::PointerPhase::kMove,24,24,false,1,9100,input::PointerTool::kFinger));
    ASSERT_TRUE(pointer(input::PointerPhase::kDown,600,220,false,2,9100,input::PointerTool::kFinger));
    EXPECT_FALSE(snapshot().regionSelectionActive);EXPECT_FLOAT_EQ(snapshot().cameraScale,before.cameraScale);
    ASSERT_TRUE(pointer(input::PointerPhase::kMove,620,240,false,2,9100,input::PointerTool::kFinger));
    (void)pointer(input::PointerPhase::kUp,500,200,false,1,9100,input::PointerTool::kFinger);
    (void)pointer(input::PointerPhase::kUp,620,240,false,2,9100,input::PointerTool::kFinger);
    EXPECT_EQ(selected(),std::vector{a});EXPECT_EQ(snapshot().semanticGeneration,before.semanticGeneration);
}
TEST_F(SelectionInput, SectionRegionBackgroundIsSelectableWithoutContent) {
    runtime::SectionIntent create;create.objectId=foundation::ObjectId::fromUint64(9302);
    create.transform.tx=20;create.transform.ty=300;create.content.boundary={0,0,100,100};
    ASSERT_EQ(f.submitSectionIntent(create,++request,9100).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
    click(140,130);mode(runtime::CanvasSelectionMode::kRectangle);const auto before=snapshot();
    rectangle(10,290,130,410);EXPECT_EQ(selected(),std::vector{create.objectId});
    EXPECT_EQ(snapshot().selectionQueryStatus,runtime::SelectionQueryStatus::kComplete);
    EXPECT_TRUE(snapshot().regionUnsupportedKinds.empty());
    EXPECT_EQ(snapshot().semanticGeneration,before.semanticGeneration);
}
TEST_F(SelectionInput, MultipleRootsRejectSingleObjectEditingAtRuntimeAndFacade) {
    rectangle(90,90,345,165);ASSERT_EQ(selected().size(),2U);const auto before=snapshot();
    runtime::CanvasControlRequest q;q.target=f.mountedCanvasTargets().front();q.clientId=9100;
    for (const auto kind:{runtime::CanvasControlPayloadKind::kSelectedSize,runtime::CanvasControlPayloadKind::kSelectedTransform,
         runtime::CanvasControlPayloadKind::kSelectedAppearance,runtime::CanvasControlPayloadKind::kSelectedOrder,
         runtime::CanvasControlPayloadKind::kSelectedLock,runtime::CanvasControlPayloadKind::kUnlockSelected}) {
        q.requestId=++request;q.payload.kind=kind;q.payload.width=500;q.payload.height=400;
        ASSERT_EQ(f.submitCanvasControl(q).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
        const auto receipt=f.canvasControlReceipt({9100,request});ASSERT_TRUE(receipt);
        EXPECT_EQ(receipt->error,runtime::CanvasControlError::kUnsupported);
    }
    auto& editor=c.qualificationRuntime();
    EXPECT_FALSE(editor.setSelectedObjectSize(400,300));EXPECT_FALSE(editor.setSelectedTransform({1,0,0,1,10,10}));
    EXPECT_FALSE(editor.patchSelectedProperty(2,semantic::PropertyValue{true}));
    EXPECT_FALSE(editor.setSelectedAppearance({}));EXPECT_FALSE(editor.setSelectedOrder(true));
    EXPECT_FALSE(editor.selectionPointer(input::PointerKey{9100,77,1},input::PointerPhase::kDown,300,130));
    EXPECT_FALSE(editor.selectionTransformActive());
    EXPECT_EQ(snapshot().semanticGeneration,before.semanticGeneration);
}
TEST_F(SelectionInput, MultipleRootsHaveNoSingleObjectHandlesOrPorts) {
    rectangle(90,90,345,165);ASSERT_EQ(selected().size(),2U);
    auto& editor=c.qualificationRuntime();ASSERT_TRUE(editor.renderSelectionOverlay());
    const auto* overlay=editor.selectionOverlay();ASSERT_TRUE(overlay);
    EXPECT_EQ(overlay->handle(render::HandleKind::kTopLeft).kind,render::HandleKind::kNone);
    EXPECT_TRUE(overlay->attachmentPoints().empty());
    EXPECT_EQ(overlay->readOnlyOutlines.size(),2U);
}
TEST_F(SelectionInput, FitSelectionIncludesAllRootsWithoutChangingDocument) {
    rectangle(90,90,345,165);ASSERT_EQ(selected().size(),2U);const auto before=snapshot();
    runtime::CanvasControlRequest q;q.target=f.mountedCanvasTargets().front();q.clientId=9100;q.requestId=++request;
    q.payload.kind=runtime::CanvasControlPayloadKind::kFit;q.payload.fitTarget=runtime::CanvasFitTarget::kSelection;
    ASSERT_EQ(f.submitCanvasControl(q).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
    const auto after=snapshot();EXPECT_LT(after.cameraScale,3.34F);
    EXPECT_GE(100*after.cameraScale+after.cameraTranslationX,0);
    EXPECT_LE(340*after.cameraScale+after.cameraTranslationX,800);
    EXPECT_EQ(after.semanticGeneration,before.semanticGeneration);
}
TEST_F(SelectionInput, MixedSelectionDoesNotExposePrimaryConnectorAsSingleEditableTarget) {
    runtime::ConnectorCreateRequest q;q.objectId=foundation::ObjectId::fromUint64(9600);
    q.content.start.value=semantic::FreePointEndpoint{{20,300}};q.content.end.value=semantic::FreePointEndpoint{{120,300}};
    ASSERT_EQ(f.createConnector(q).state,runtime::ConnectorCommandState::kApplied);
    click(60,300);ASSERT_TRUE(f.readSelectedConnector().available);
    click(140,130,true);ASSERT_EQ(selected().size(),2U);EXPECT_FALSE(f.readSelectedConnector().available);
}
TEST_F(SelectionInput, RegionExcludesHiddenAndLockedWhilePointCanInspectLockedObject) {
    semantic::PatchPropertiesOp patch{{{a,1,semantic::PropertyPatchAction::kSet,semantic::PropertyValue{false}},
        {b,2,semantic::PropertyPatchAction::kSet,semantic::PropertyValue{true}}}};
    semantic::Operation op{semantic::OperationId{foundation::ObjectId::fromUint64(9601)},
        semantic::DocumentId{foundation::ObjectId::fromUint64(1)},1,1,patch};
    ASSERT_EQ(c.applyExternalOperation(op,semantic::ApplySource::kRemoteSync).disposition,semantic::ApplyDisposition::kApplied);
    const auto before=snapshot();mode(runtime::CanvasSelectionMode::kRectangle);
    rectangle(90,90,510,170);EXPECT_EQ(selected(),std::vector{foundation::ObjectId::fromUint64(9103)});
    mode(runtime::CanvasSelectionMode::kPoint);click(300,130);EXPECT_EQ(selected(),std::vector{b});
    EXPECT_TRUE(snapshot().selectedLocked);EXPECT_EQ(snapshot().semanticGeneration,before.semanticGeneration);
}

TEST_F(SelectionInput, AddResolvesRetainedRootsAfterExternalReparent) {
    const auto groupId=foundation::ObjectId::fromUint64(9700);
    semantic::ObjectRecord group;group.id=groupId;group.kind=semantic::ObjectKind::kGroup;group.kind_version=1;
    group.content=semantic::GroupContent{};group.placement.order_key=semantic::OrderKey({4});
    semantic::Operation seed{semantic::OperationId{foundation::ObjectId::fromUint64(9701)},
        semantic::DocumentId{foundation::ObjectId::fromUint64(1)},1,1,semantic::InsertObjectsOp{{group}}};
    ASSERT_EQ(c.applyExternalOperation(seed,semantic::ApplySource::kRemoteSync).disposition,semantic::ApplyDisposition::kApplied);
    click(140,130);ASSERT_EQ(selected(),std::vector{a});
    ASSERT_TRUE(pointer(input::PointerPhase::kDown,250,90,true));ASSERT_TRUE(pointer(input::PointerPhase::kMove,345,165));
    const auto before=*c.qualificationRuntime().qualificationObjectRecord(a);
    semantic::EditContainerTopologyOp move;move.effect=semantic::RelocateObjectsEffect{};
    move.guard.nodes={{a,before.kind,before.kind_version,before.placement,before.transform},
        {groupId,group.kind,group.kind_version,group.placement,group.transform}};
    move.guard.child_sets={{{},{{a,semantic::OrderKey({1})},{b,semantic::OrderKey({2})},
        {foundation::ObjectId::fromUint64(9103),semantic::OrderKey({3})},{groupId,semantic::OrderKey({4})}}},{groupId,{}}};
    move.relocations={{a,{groupId,semantic::OrderKey({1})},before.transform}};
    semantic::Operation op{semantic::OperationId{foundation::ObjectId::fromUint64(9702)},
        semantic::DocumentId{foundation::ObjectId::fromUint64(1)},1,1,move};
    ASSERT_EQ(c.applyExternalOperation(op,semantic::ApplySource::kRemoteSync).disposition,semantic::ApplyDisposition::kApplied);
    ASSERT_TRUE(pointer(input::PointerPhase::kUp,345,165));
    EXPECT_EQ(selected(),(std::vector{groupId,b}));
    EXPECT_EQ(snapshot().selectionQueryStatus,runtime::SelectionQueryStatus::kComplete);
    // Clicking an already acquired Group never leaves its old child selected too.
    click(140,130,true);EXPECT_EQ(selected(),(std::vector{groupId,b}));
    runtime::SectionIntent enter;enter.action=runtime::SectionAction::kEnterGroupScope;enter.objectId=groupId;
    ASSERT_EQ(f.submitSectionIntent(enter,++request,9100).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
    semantic::Operation overlap{semantic::OperationId{foundation::ObjectId::fromUint64(9704)},
        semantic::DocumentId{foundation::ObjectId::fromUint64(1)},1,1,semantic::SetTransformsOp{{{b,{1,0,0,1,100,100}}}}};
    ASSERT_EQ(c.applyExternalOperation(overlap,semantic::ApplySource::kRemoteSync).disposition,semantic::ApplyDisposition::kApplied);
    overlap.id=semantic::OperationId{foundation::ObjectId::fromUint64(9705)};
    overlap.payload=semantic::SetPlacementsOp{{{b,{{},semantic::OrderKey({5})}}}};
    ASSERT_EQ(c.applyExternalOperation(overlap,semantic::ApplySource::kRemoteSync).disposition,semantic::ApplyDisposition::kApplied);
    click(140,130);EXPECT_EQ(selected(),std::vector{a});
}
TEST_F(SelectionInput, StaleRegionRetiresVisiblePreviewAfterCanonicalReplacement) {
    auto& editor=c.qualificationRuntime();
    click(140,130);mode(runtime::CanvasSelectionMode::kRectangle);
    ASSERT_TRUE(pointer(input::PointerPhase::kDown,20,20));ASSERT_TRUE(pointer(input::PointerPhase::kMove,500,200));
    ASSERT_TRUE(editor.previewSurfaceProvider()->overlayVisible());ASSERT_FALSE(editor.transientOverlayDirty());
    runtime::CanvasControlRequest q;q.target=f.mountedCanvasTargets().front();q.clientId=9100;q.requestId=++request;
    q.payload.kind=runtime::CanvasControlPayloadKind::kZoomAt;q.payload.scale=2;
    ASSERT_EQ(f.submitCanvasControl(q).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
    runtime::RuntimeFrameOpportunity frame;ASSERT_TRUE(c.serviceFrameOpportunity(true,frame));
    EXPECT_FALSE(snapshot().regionSelectionActive);EXPECT_FALSE(editor.previewSurfaceProvider()->overlayVisible());
    ASSERT_TRUE(pointer(input::PointerPhase::kUp,500,200));
    EXPECT_EQ(selected(),std::vector{a});EXPECT_FALSE(editor.previewSurfaceProvider()->overlayVisible());
}
TEST_F(SelectionInput, MixedLockedSelectionRejectsDeletionInBothOrdersWithoutHistoryMutation) {
    semantic::Operation op{semantic::OperationId{foundation::ObjectId::fromUint64(9703)},
        semantic::DocumentId{foundation::ObjectId::fromUint64(1)},1,1,
        semantic::PatchPropertiesOp{{{b,2,semantic::PropertyPatchAction::kSet,semantic::PropertyValue{true}}}}};
    ASSERT_EQ(c.applyExternalOperation(op,semantic::ApplySource::kRemoteSync).disposition,semantic::ApplyDisposition::kApplied);
    for (bool lockedFirst:{false,true}) {
        mode(runtime::CanvasSelectionMode::kPoint);
        click(lockedFirst?300.F:140.F,130);click(lockedFirst?140.F:300.F,130,true);
        ASSERT_EQ(selected().size(),2U);EXPECT_TRUE(snapshot().selectedLocked);
        const auto before=snapshot();const auto historySize=ink_playground::InkPlaygroundHistoryTestAccess::historySize(c.qualificationRuntime());
        EXPECT_FALSE(c.qualificationRuntime().deleteSelectedObjects());
        runtime::CanvasControlRequest q;q.target=f.mountedCanvasTargets().front();q.clientId=9100;q.requestId=++request;
        q.payload.kind=runtime::CanvasControlPayloadKind::kDeleteSelected;
        ASSERT_EQ(f.submitCanvasControl(q).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
        const auto receipt=f.canvasControlReceipt({9100,request});ASSERT_TRUE(receipt);EXPECT_NE(receipt->error,runtime::CanvasControlError::kNone);
        EXPECT_EQ(c.semanticObjectCount(),3U);EXPECT_EQ(snapshot().semanticGeneration,before.semanticGeneration);
        EXPECT_EQ(snapshot().canUndo,before.canUndo);EXPECT_EQ(snapshot().canRedo,before.canRedo);
        EXPECT_EQ(ink_playground::InkPlaygroundHistoryTestAccess::historySize(c.qualificationRuntime()),historySize);
        ASSERT_TRUE(c.qualificationRuntime().qualificationObjectRecord(a));ASSERT_TRUE(c.qualificationRuntime().qualificationObjectRecord(b));
    }
}
TEST_F(SelectionInput, SectionBackgroundDoesNotMaskContentAndBoundaryRemainsSelectable) {
    const auto section=foundation::ObjectId::fromUint64(9800);
    runtime::SectionIntent create;create.objectId=section;create.content.boundary={50,50,500,200};
    ASSERT_EQ(f.submitSectionIntent(create,++request,9100).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
    click(140,130);EXPECT_EQ(selected(),std::vector{a});
    // A Section created later is above unrelated content in paint order.
    click(300,130);EXPECT_EQ(selected(),std::vector{b});
    click(70,210);EXPECT_EQ(selected(),std::vector{section});
    click(140,130);EXPECT_EQ(selected(),std::vector{a});
    click(50,200);EXPECT_EQ(selected(),std::vector{section});
    const auto before=snapshot();ASSERT_TRUE(pointer(input::PointerPhase::kDown,50,200));
    ASSERT_TRUE(pointer(input::PointerPhase::kMove,40,200));ASSERT_TRUE(pointer(input::PointerPhase::kCancel,40,200));
    EXPECT_EQ(selected(),std::vector{section});EXPECT_EQ(snapshot().semanticGeneration,before.semanticGeneration);
    // Hidden children are skipped; existing point inspection of locked ones remains.
    semantic::Operation op{semantic::OperationId{foundation::ObjectId::fromUint64(9801)},
        semantic::DocumentId{foundation::ObjectId::fromUint64(1)},1,1,
        semantic::PatchPropertiesOp{{{a,1,semantic::PropertyPatchAction::kSet,semantic::PropertyValue{false}},
            {b,2,semantic::PropertyPatchAction::kSet,semantic::PropertyValue{true}}}}};
    ASSERT_EQ(c.applyExternalOperation(op,semantic::ApplySource::kRemoteSync).disposition,semantic::ApplyDisposition::kApplied);
    click(140,130);EXPECT_EQ(selected(),std::vector{section});
    click(300,130);EXPECT_EQ(selected(),std::vector{b});EXPECT_TRUE(snapshot().selectedLocked);
}

TEST_F(SelectionInput, RealBrushRegionUsesPublishedVisibleOutlineWithoutDocumentWrites) {
    ASSERT_EQ(f.setTool(4101,++request,f.readRuntimeState().identity.runtimeGeneration,9100).state,runtime::ProductControlState::kQueued);
    ASSERT_TRUE(c.ownerPump());
    ASSERT_TRUE(pointer(input::PointerPhase::kDown,50,300));ASSERT_TRUE(pointer(input::PointerPhase::kMove,500,500));
    ASSERT_TRUE(pointer(input::PointerPhase::kUp,500,500));ASSERT_EQ(c.semanticObjectCount(),4U);
    mode(runtime::CanvasSelectionMode::kRectangle);const auto before=snapshot();
    rectangle(40,290,510,510);EXPECT_EQ(snapshot().selectionQueryStatus,runtime::SelectionQueryStatus::kComplete);
    ASSERT_EQ(selected().size(),1U);
    const auto object=c.qualificationRuntime().qualificationObjectRecord(selected().front());ASSERT_TRUE(object);
    EXPECT_EQ(object->kind,semantic::ObjectKind::kVectorStroke);
    rectangle(40,480,65,500);EXPECT_TRUE(selected().empty());
    EXPECT_EQ(snapshot().semanticGeneration,before.semanticGeneration);EXPECT_EQ(snapshot().lastAppliedOperation,before.lastAppliedOperation);
    EXPECT_EQ(snapshot().canUndo,before.canUndo);EXPECT_EQ(snapshot().canRedo,before.canRedo);
}

TEST_F(SelectionInput, SectionAreaPrefersContentAndWholeBoundaryProducesNonredundantRoots) {
    runtime::SectionIntent section;section.objectId=foundation::ObjectId::fromUint64(9900);section.content.boundary={50,50,500,200};
    ASSERT_EQ(f.submitSectionIntent(section,++request,9100).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
    runtime::SectionIntent move;move.action=runtime::SectionAction::kReparent;move.objects={a};move.parentId=section.objectId;
    ASSERT_EQ(f.submitSectionIntent(move,++request,9100).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
    const auto before=snapshot();mode(runtime::CanvasSelectionMode::kRectangle);
    rectangle(90,90,185,165);EXPECT_EQ(selected(),std::vector{a});
    rectangle(40,40,560,260);auto roots=selected();ASSERT_EQ(roots.size(),3U);
    EXPECT_NE(std::find(roots.begin(),roots.end(),section.objectId),roots.end());
    EXPECT_EQ(std::find(roots.begin(),roots.end(),a),roots.end());
    rectangle(40,40,560,260);EXPECT_EQ(selected(),roots);
    EXPECT_EQ(snapshot().semanticGeneration,before.semanticGeneration);EXPECT_EQ(snapshot().lastAppliedOperation,before.lastAppliedOperation);
}
TEST_F(SelectionInput, ImportedCropFrameExcludesTrimmedSourceAndCropKeepsInputOwnership) {
    runtime::ImageImportRequest image;image.objectId=foundation::ObjectId::fromUint64(9902);
    image.bytes={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,4,0,0,0,181,28,12,2,0,0,0,11,73,68,65,84,120,218,99,100,248,15,0,1,5,1,1,39,24,227,102,0,0,0,0,73,69,78,68,174,66,96,130};
    image.width=100;image.height=100;image.transform.tx=550;image.transform.ty=300;
    image.sourceRect=semantic::NormalizedRect{.25,.25,.5,.5};image.contentMode=semantic::ImageContentMode::kStretch;
    ASSERT_EQ(f.importImage(image).state,runtime::ImageCommandState::kApplied);
    mode(runtime::CanvasSelectionMode::kRectangle);const auto before=snapshot();
    rectangle(510,330,530,350);EXPECT_TRUE(selected().empty());
    rectangle(555,305,645,395);EXPECT_EQ(selected(),std::vector{image.objectId});
    EXPECT_EQ(snapshot().semanticGeneration,before.semanticGeneration);EXPECT_EQ(snapshot().lastAppliedOperation,before.lastAppliedOperation);
    const auto begin=f.beginImageCrop({image.objectId});ASSERT_EQ(begin.state,runtime::ImageCommandState::kApplied);
    ASSERT_TRUE(pointer(input::PointerPhase::kDown,570,330));ASSERT_TRUE(pointer(input::PointerPhase::kMove,580,335));
    EXPECT_FALSE(snapshot().regionSelectionActive);
    ASSERT_TRUE(pointer(input::PointerPhase::kUp,580,335));
    ASSERT_TRUE(f.readImageCrop().active);EXPECT_EQ(snapshot().semanticGeneration,before.semanticGeneration);
    ASSERT_EQ(f.cancelImageCrop(begin.cropToken).state,runtime::ImageCommandState::kApplied);
    const auto second=f.beginImageCrop({image.objectId});ASSERT_EQ(second.state,runtime::ImageCommandState::kApplied);
    ASSERT_TRUE(pointer(input::PointerPhase::kDown,570,330));ASSERT_TRUE(pointer(input::PointerPhase::kCancel,570,330));
    EXPECT_FALSE(f.readImageCrop().active);EXPECT_EQ(snapshot().semanticGeneration,before.semanticGeneration);
}

}
