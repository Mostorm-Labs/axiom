#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/runtime/runtime_facade.hpp"
#include <gtest/gtest.h>
#include <thread>

namespace {
using namespace canvas;
TEST(FC08SectionConsumer, EmptyCreateRenameResizeReleaseUseRealHistory) {
    runtime::RuntimeComposition c;ASSERT_TRUE(c.open());ASSERT_TRUE(c.bindSurface(800,600));
    runtime::RuntimeFacade& f=c.controlFacade();
    runtime::SectionIntent intent;intent.objectId=foundation::ObjectId::fromUint64(5100);
    intent.transform.tx=50;intent.transform.ty=60;
    auto submit=[&](std::uint64_t request) {
        EXPECT_EQ(f.submitSectionIntent(intent,request,8800).state,runtime::ProductControlState::kQueued);
        EXPECT_TRUE(c.ownerPump());
        const auto receipt=f.canvasControlReceipt({8800,request});
        EXPECT_TRUE(receipt.has_value());
        if(receipt)EXPECT_EQ(receipt->state,runtime::CanvasControlReceiptState::kApplied)<<receipt->detail;
    };
    submit(1);ASSERT_EQ(c.semanticObjectCount(),1U);
    auto s=f.readSection(intent.objectId);ASSERT_TRUE(s.available);ASSERT_TRUE(s.projectionReady);
    EXPECT_EQ(s.content.title,"Section");EXPECT_FLOAT_EQ(s.worldQuad[0].x,50);
    intent.action=runtime::SectionAction::kRename;intent.content.title="区域 A";submit(2);
    EXPECT_EQ(f.readSection(intent.objectId).content.title,"区域 A");
    intent.action=runtime::SectionAction::kResize;intent.content.boundary={-20,0,120,80};submit(3);
    EXPECT_DOUBLE_EQ(f.readSection(intent.objectId).content.boundary.x,-20);
    const auto state=f.readRuntimeState();
    EXPECT_EQ(f.undo(4,state.identity.runtimeGeneration,8800).state,runtime::ProductControlState::kQueued);
    ASSERT_TRUE(c.ownerPump());EXPECT_DOUBLE_EQ(f.readSection(intent.objectId).content.boundary.x,0);
    intent.action=runtime::SectionAction::kRelease;submit(5);EXPECT_EQ(c.semanticObjectCount(),0U);
    EXPECT_EQ(f.undo(6,state.identity.runtimeGeneration,8800).state,runtime::ProductControlState::kQueued);
    ASSERT_TRUE(c.ownerPump());EXPECT_EQ(f.readSection(intent.objectId).content.title,"区域 A");
    EXPECT_TRUE(c.close());
}
TEST(FC08SectionConsumer, WrapReparentReleasePreserveWorldAndRejectCycleAtomically) {
    runtime::RuntimeComposition c;ASSERT_TRUE(c.open());ASSERT_TRUE(c.bindSurface(800,600));
    auto& f=c.controlFacade();
    const auto a=foundation::ObjectId::fromUint64(5200),b=foundation::ObjectId::fromUint64(5201);
    runtime::StickyCreateRequest sticky;sticky.objectId=a;sticky.content={80,80};sticky.transform.tx=40;sticky.transform.ty=60;
    ASSERT_EQ(f.createSticky(sticky).state,runtime::ConnectorCommandState::kApplied);
    sticky.objectId=b;sticky.transform.tx=240;
    ASSERT_EQ(f.createSticky(sticky).state,runtime::ConnectorCommandState::kApplied);
    auto command=[&](runtime::SectionIntent intent,std::uint64_t n,bool accepted=true) {
        EXPECT_EQ(f.submitSectionIntent(intent,n,8801).state,runtime::ProductControlState::kQueued);
        EXPECT_TRUE(c.ownerPump());const auto receipt=f.canvasControlReceipt({8801,n});
        EXPECT_TRUE(receipt);if(receipt)EXPECT_EQ(receipt->state==runtime::CanvasControlReceiptState::kApplied,accepted)<<receipt->detail;
    };
    runtime::SectionIntent wrap;wrap.action=runtime::SectionAction::kWrap;wrap.objects={a,b};
    wrap.objectId=foundation::ObjectId::fromUint64(5300);command(wrap,1);
    auto s=f.readSection(wrap.objectId);ASSERT_TRUE(s.available);ASSERT_EQ(s.members.size(),2U);
    EXPECT_DOUBLE_EQ(s.members[0].worldTransform.tx,40);EXPECT_DOUBLE_EQ(s.members[1].worldTransform.tx,240);
    runtime::SectionIntent create;create.objectId=foundation::ObjectId::fromUint64(5301);create.transform={0,2,-2,0,400,300};command(create,2);
    runtime::SectionIntent move;move.action=runtime::SectionAction::kReparent;move.objects={a};move.parentId=create.objectId;command(move,3);
    auto target=f.readSection(create.objectId);ASSERT_EQ(target.members.size(),1U);
    EXPECT_DOUBLE_EQ(target.members[0].worldTransform.tx,40);EXPECT_DOUBLE_EQ(target.members[0].worldTransform.ty,60);
    const auto before=c.readback().semanticGeneration;
    move.objects={create.objectId};move.parentId=create.objectId;command(move,4,false);
    EXPECT_EQ(c.readback().semanticGeneration,before);EXPECT_TRUE(f.readCanvasControlSnapshot(f.mountedCanvasTargets()[0]).canUndo);
    runtime::SectionIntent release;release.action=runtime::SectionAction::kRelease;release.objectId=create.objectId;command(release,5);
    EXPECT_FALSE(f.readSection(create.objectId).available);EXPECT_EQ(c.semanticObjectCount(),3U);
    EXPECT_TRUE(c.close());
}
TEST(FC08SectionConsumer, DuplicateCollisionStaleAndRecoveryHaveTruthfulReceipts) {
    runtime::RuntimeComposition c;ASSERT_TRUE(c.open());ASSERT_TRUE(c.bindSurface(800,600));auto& f=c.controlFacade();
    runtime::SectionIntent intent;intent.objectId=foundation::ObjectId::fromUint64(5400);
    EXPECT_EQ(f.submitSectionIntent(intent,1,8802).state,runtime::ProductControlState::kQueued);
    EXPECT_EQ(f.submitSectionIntent(intent,1,8802).state,runtime::ProductControlState::kQueued);
    ASSERT_TRUE(c.ownerPump());const auto generation=c.readback().semanticGeneration;
    EXPECT_EQ(f.submitSectionIntent(intent,1,8802).state,runtime::ProductControlState::kApplied);
    ASSERT_TRUE(c.ownerPump());EXPECT_EQ(c.readback().semanticGeneration,generation);
    intent.content.title="collision";
    EXPECT_EQ(f.submitSectionIntent(intent,1,8802).state,runtime::ProductControlState::kRejected);
    EXPECT_EQ(f.readSection(intent.objectId).content.title,"Section");
    intent.action=runtime::SectionAction::kRename;intent.expectedSemanticGeneration=generation+50;
    EXPECT_EQ(f.submitSectionIntent(intent,2,8802).state,runtime::ProductControlState::kQueued);
    ASSERT_TRUE(c.ownerPump());const auto stale=f.canvasControlReceipt({8802,2});ASSERT_TRUE(stale&&stale->section);
    EXPECT_EQ(stale->section->failure,runtime::SectionFailure::kStale);EXPECT_EQ(c.readback().semanticGeneration,generation);
    ASSERT_TRUE(c.loseSurface());ASSERT_TRUE(c.rebindSurface());ASSERT_TRUE(c.ownerPump());
    EXPECT_TRUE(f.readSection(intent.objectId).projectionReady);EXPECT_EQ(c.readback().semanticGeneration,generation);
    runtime::RuntimeFrameOpportunity frame;EXPECT_TRUE(c.serviceFrameOpportunity(true,frame));
    EXPECT_TRUE(c.close());
}
TEST(FC08SectionConsumer, BoundaryPointerChangesOnlyBoundaryAndCancelWritesNothing) {
    runtime::RuntimeComposition c;ASSERT_TRUE(c.open());ASSERT_TRUE(c.bindSurface(800,600));auto& f=c.controlFacade();
    runtime::SectionIntent create;create.objectId=foundation::ObjectId::fromUint64(5500);create.content.boundary={0,0,200,160};create.transform.tx=40;create.transform.ty=50;
    ASSERT_EQ(f.submitSectionIntent(create,1,8803).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
    auto state=f.readRuntimeState();ASSERT_EQ(f.setSelectionMode(true,2,state.identity.runtimeGeneration,8803).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
    auto view=f.readCanvasControlSnapshot(f.mountedCanvasTargets().front());
    ASSERT_TRUE(c.registerSource(8803));
    std::uint64_t sequence=0;
    const auto pointer=[&](input::PointerPhase phase,float x,float y) {
        input::PlatformPointerBatch batch;
        ++sequence;batch.samples.push_back({8803,99,sequence,sequence*1000000,x*view.cameraScale+view.cameraTranslationX,y*view.cameraScale+view.cameraTranslationY,0,0,0,{},{},input::SampleProvenance::kConfirmedCurrent,phase});
        return c.submit(batch);
    };
    ASSERT_TRUE(pointer(input::PointerPhase::kDown,240,210));
    ASSERT_TRUE(pointer(input::PointerPhase::kMove,300,250));
    EXPECT_DOUBLE_EQ(f.readSection(create.objectId).content.boundary.width,200); // transient only
    ASSERT_TRUE(pointer(input::PointerPhase::kUp,300,250));
    const auto resized=f.readSection(create.objectId);EXPECT_DOUBLE_EQ(resized.content.boundary.width,260);EXPECT_DOUBLE_EQ(resized.content.boundary.height,200);
    EXPECT_EQ(resized.localTransform,create.transform);
    const auto generation=c.readback().semanticGeneration;
    ASSERT_TRUE(pointer(input::PointerPhase::kDown,300,250));
    ASSERT_TRUE(pointer(input::PointerPhase::kMove,350,300));
    (void)pointer(input::PointerPhase::kCancel,350,300);
    EXPECT_EQ(c.readback().semanticGeneration,generation);EXPECT_EQ(f.readSection(create.objectId).content,resized.content);
    EXPECT_TRUE(c.close());
}
TEST(FC08SectionConsumer, DefaultSectionRemovalReleasesChildren) {
    runtime::RuntimeComposition c;ASSERT_TRUE(c.open());ASSERT_TRUE(c.bindSurface(800,600));auto& f=c.controlFacade();
    const auto a=foundation::ObjectId::fromUint64(5600),section=foundation::ObjectId::fromUint64(5601);
    runtime::StickyCreateRequest sticky;sticky.objectId=a;sticky.content={80,80};
    ASSERT_EQ(f.createSticky(sticky).state,runtime::ConnectorCommandState::kApplied);
    runtime::SectionIntent wrap;wrap.action=runtime::SectionAction::kWrap;wrap.objects={a};wrap.objectId=section;
    ASSERT_EQ(f.submitSectionIntent(wrap,1,8804).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
    runtime::CanvasControlRequest remove;remove.clientId=8804;remove.requestId=2;remove.target=f.mountedCanvasTargets()[0];
    remove.payload.kind=runtime::CanvasControlPayloadKind::kDeleteSelected;
    ASSERT_EQ(f.submitCanvasControl(remove).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
    EXPECT_EQ(c.semanticObjectCount(),1U);EXPECT_FALSE(f.readSection(section).available);
    EXPECT_TRUE(c.close());
}
TEST(FC08SectionConsumer, MixedRemovalAndInvalidResizePreserveRedo) {
    runtime::RuntimeComposition c;ASSERT_TRUE(c.open());ASSERT_TRUE(c.bindSurface(800,600));auto& f=c.controlFacade();
    std::uint64_t n=0;
    auto command=[&](runtime::SectionIntent intent) {
        const auto request=++n;EXPECT_EQ(f.submitSectionIntent(intent,request,8805).state,runtime::ProductControlState::kQueued);
        EXPECT_TRUE(c.ownerPump());return f.canvasControlReceipt({8805,request});
    };
    runtime::SectionIntent outer;outer.objectId=foundation::ObjectId::fromUint64(5700);ASSERT_TRUE(command(outer));
    runtime::SectionIntent inner;inner.objectId=foundation::ObjectId::fromUint64(5701);inner.parentId=outer.objectId;ASSERT_TRUE(command(inner));
    runtime::StickyCreateRequest sticky;sticky.objectId=foundation::ObjectId::fromUint64(5702);sticky.content={80,80};
    ASSERT_EQ(f.createSticky(sticky).state,runtime::ConnectorCommandState::kApplied);
    runtime::SectionIntent move;move.action=runtime::SectionAction::kReparent;move.objects={sticky.objectId};move.parentId=outer.objectId;ASSERT_TRUE(command(move));
    auto rename=outer;rename.action=runtime::SectionAction::kRename;rename.content.title="Redo retained";ASSERT_TRUE(command(rename));
    const auto target=f.mountedCanvasTargets().front();
    EXPECT_EQ(f.undo(++n,target.runtimeEpoch,8805).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
    auto invalid=outer;invalid.action=runtime::SectionAction::kResize;invalid.content.boundary.width=0;
    const auto before=c.readback().semanticGeneration;
    const auto rejected=command(invalid);ASSERT_TRUE(rejected&&rejected->section);
    EXPECT_EQ(rejected->section->canonical,runtime::SectionCanonicalOutcome::kNotCommitted);
    EXPECT_EQ(c.readback().semanticGeneration,before);EXPECT_TRUE(f.readCanvasControlSnapshot(target).canRedo);
    auto select=outer;select.action=runtime::SectionAction::kSelectChildren;ASSERT_TRUE(command(select));
    ASSERT_EQ(f.readCanvasControlSnapshot(target).selectedObjectCount,2U);
    runtime::CanvasControlRequest remove;remove.clientId=8805;remove.requestId=++n;remove.target=target;remove.payload.kind=runtime::CanvasControlPayloadKind::kDeleteSelected;
    ASSERT_EQ(f.submitCanvasControl(remove).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
    const auto mixed=f.canvasControlReceipt({8805,n});ASSERT_TRUE(mixed&&mixed->section);
    EXPECT_EQ(mixed->section->failure,runtime::SectionFailure::kUnsupportedMixedRemoval);
    EXPECT_EQ(c.readback().semanticGeneration,before);EXPECT_EQ(c.semanticObjectCount(),3U);EXPECT_TRUE(f.readCanvasControlSnapshot(target).canRedo);
    EXPECT_TRUE(c.close());
}
TEST(FC08SectionConsumer, ExistingGroupScopeResolvesSectionWithoutNewMembershipModel) {
    runtime::RuntimeComposition c;ASSERT_TRUE(c.open());ASSERT_TRUE(c.bindSurface(800,600));auto& f=c.controlFacade();
    semantic::ObjectRecord group;group.id=foundation::ObjectId::fromUint64(5800);group.kind=semantic::ObjectKind::kGroup;group.kind_version=1;
    group.placement.order_key=semantic::OrderKey({1});group.transform={0,2,-2,0,100,200};group.content=semantic::GroupContent{};
    auto section=group;section.id=foundation::ObjectId::fromUint64(5801);section.kind=semantic::ObjectKind::kSection;section.placement.parent_id=group.id;
    section.transform={1,0,0,1,10,20};section.content=semantic::SectionContent{{0,0,100,80},"Nested",0,0};
    semantic::EditContainerTopologyOp topology;topology.effect=semantic::IntroduceWrappersEffect{{group,section}};
    topology.guard.child_sets.push_back({std::nullopt,{}});
    semantic::Operation seed{semantic::OperationId{foundation::ObjectId::fromUint64(5802)},semantic::DocumentId{foundation::ObjectId::fromUint64(1)},1,1,topology};
    ASSERT_EQ(c.applyExternalOperation(seed,semantic::ApplySource::kRemoteSync).disposition,semantic::ApplyDisposition::kApplied);
    std::uint64_t request=0;
    const auto command=[&](runtime::SectionAction action,foundation::ObjectId id) {
        runtime::SectionIntent intent;intent.action=action;intent.objectId=id;
        EXPECT_EQ(f.submitSectionIntent(intent,++request,8807).state,runtime::ProductControlState::kQueued);EXPECT_TRUE(c.ownerPump());
        const auto receipt=f.canvasControlReceipt({8807,request});EXPECT_TRUE(receipt);if(receipt)EXPECT_EQ(receipt->state,runtime::CanvasControlReceiptState::kApplied);
    };
    const auto snapshot=f.readSection(section.id);ASSERT_TRUE(snapshot.projectionReady);
    EXPECT_DOUBLE_EQ(snapshot.worldTransform.tx,60);EXPECT_DOUBLE_EQ(snapshot.worldTransform.ty,220);
    runtime::SectionIntent createNested;createNested.objectId=foundation::ObjectId::fromUint64(5803);createNested.parentId=group.id;
    ASSERT_EQ(f.submitSectionIntent(createNested,++request,8807).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
    EXPECT_EQ(f.readSection(createNested.objectId).selectedRoots,std::vector{group.id})<<"Created child follows existing root-first Group selection";
    command(runtime::SectionAction::kRelease,createNested.objectId);
    createNested.objectId=foundation::ObjectId::fromUint64(5804);createNested.parentId.reset();
    ASSERT_EQ(f.submitSectionIntent(createNested,++request,8807).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
    runtime::SectionIntent move;move.action=runtime::SectionAction::kReparent;move.objects={createNested.objectId};move.parentId=group.id;
    ASSERT_EQ(f.submitSectionIntent(move,++request,8807).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
    EXPECT_EQ(f.readSection(createNested.objectId).selectedRoots,std::vector{group.id})<<"Reparent consequence follows the same Group scope";
    command(runtime::SectionAction::kRelease,createNested.objectId);
    command(runtime::SectionAction::kSelect,section.id);
    EXPECT_EQ(f.readSection(section.id).selectedRoots,std::vector{group.id});
    command(runtime::SectionAction::kEnterGroupScope,group.id);command(runtime::SectionAction::kSelect,section.id);
    EXPECT_EQ(f.readSection(section.id).groupScope,group.id);EXPECT_EQ(f.readSection(section.id).selectedRoots,std::vector{section.id});
    command(runtime::SectionAction::kLeaveGroupScope,{});EXPECT_FALSE(f.readSection(section.id).groupScope);
    EXPECT_TRUE(c.close());
}
TEST(FC08SectionConsumer, ProducerOnlyQueuesAndClosingRetiresPendingRequest) {
    runtime::RuntimeComposition c;ASSERT_TRUE(c.open());ASSERT_TRUE(c.bindSurface(800,600));auto& f=c.controlFacade();
    runtime::SectionIntent intent;intent.objectId=foundation::ObjectId::fromUint64(5900);
    runtime::ProductControlReceipt accepted;
    std::thread producer([&]{accepted=f.submitSectionIntent(intent,1,8808);});producer.join();
    ASSERT_EQ(accepted.state,runtime::ProductControlState::kQueued);EXPECT_EQ(c.semanticObjectCount(),0U);
    ASSERT_TRUE(c.ownerPump());EXPECT_EQ(c.semanticObjectCount(),1U);
    const auto generation=c.readback().semanticGeneration;
    auto stale=intent;stale.action=runtime::SectionAction::kRename;stale.content.title="stale selection";
    stale.selectionRevision=f.readSection(intent.objectId).selectionRevision-1;
    ASSERT_EQ(f.submitSectionIntent(stale,1,8809).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());
    const auto rejected=f.canvasControlReceipt({8809,1});ASSERT_TRUE(rejected&&rejected->section);
    EXPECT_EQ(rejected->section->failure,runtime::SectionFailure::kStale);EXPECT_EQ(c.readback().semanticGeneration,generation);
    intent.action=runtime::SectionAction::kRename;intent.content.title="Must not execute after close";
    std::thread second([&]{accepted=f.submitSectionIntent(intent,2,8808);});second.join();
    ASSERT_EQ(accepted.state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.close());
    const auto retired=f.canvasControlReceipt({8808,2});ASSERT_TRUE(retired);
    EXPECT_EQ(retired->state,runtime::CanvasControlReceiptState::kTargetDestroyed);
    EXPECT_EQ(c.readback().semanticGeneration,generation);EXPECT_TRUE(f.mountedCanvasTargets().empty());
}
class SectionDropTest : public testing::Test {
protected:
    runtime::RuntimeComposition c;
    runtime::RuntimeFacade& f=c.controlFacade();
    std::uint64_t request=0,sequence=0;
    const foundation::ObjectId actor=foundation::ObjectId::fromUint64(6601);
    const foundation::ObjectId section=foundation::ObjectId::fromUint64(6602);
    void SetUp() override {
        ASSERT_TRUE(c.open());ASSERT_TRUE(c.bindSurface(800,600));ASSERT_TRUE(c.registerSource(6600));
        ASSERT_EQ(f.setSelectionMode(true,++request,f.readRuntimeState().identity.runtimeGeneration,6600).state,runtime::ProductControlState::kQueued);
        ASSERT_TRUE(c.ownerPump());
    }
    void seed(bool ink=false) {
        semantic::ObjectRecord object;object.id=actor;object.kind_version=1;
        object.placement.order_key=semantic::OrderKey({1});object.transform.tx=20;object.transform.ty=40;
        if(ink) {
            semantic::StrokeRecord stroke;stroke.brush.brush_family_id=1;stroke.brush.brush_version=1;
            stroke.brush.color={0,0,0,1};stroke.brush.nominal_size=2;stroke.brush.opacity=1;
            stroke.data=semantic::VectorStrokeData{{{{0,0},1,{0,0}},{{80,60},1,{0,0}}}};
            object.kind=semantic::ObjectKind::kVectorStroke;object.content=semantic::VectorStrokeContent{stroke};
        } else {object.kind=semantic::ObjectKind::kShape;object.content=semantic::ShapeContent{1,80,60};}
        semantic::Operation operation{semantic::OperationId{foundation::ObjectId::fromUint64(6603)},
            semantic::DocumentId{foundation::ObjectId::fromUint64(1)},1,1,semantic::InsertObjectsOp{{object}}};
        ASSERT_EQ(c.applyExternalOperation(operation,semantic::ApplySource::kRemoteSync).disposition,semantic::ApplyDisposition::kApplied);
    }
    void command(runtime::SectionIntent intent) {
        const auto n=++request;ASSERT_EQ(f.submitSectionIntent(intent,n,6600).state,runtime::ProductControlState::kQueued);
        ASSERT_TRUE(c.ownerPump());const auto receipt=f.canvasControlReceipt({6600,n});
        ASSERT_TRUE(receipt&&receipt->section);ASSERT_EQ(receipt->section->failure,runtime::SectionFailure::kNone);
    }
    void create(semantic::Transform2D transform={1,0,0,1,300,200},semantic::Rect2 boundary={0,0,240,220}) {
        runtime::SectionIntent intent;intent.objectId=section;intent.transform=transform;intent.content.boundary=boundary;command(intent);
    }
    bool pointer(input::PointerPhase phase,float x,float y) {
        const auto view=f.readCanvasControlSnapshot(f.mountedCanvasTargets().front());
        input::PlatformPointerBatch batch;++sequence;
        batch.samples.push_back({6600,1,sequence,sequence*1000000,x*view.cameraScale+view.cameraTranslationX,
            y*view.cameraScale+view.cameraTranslationY,0,0,0,{},{},input::SampleProvenance::kConfirmedCurrent,phase});
        return c.submit(batch);
    }
    bool click(float x,float y) {return pointer(input::PointerPhase::kDown,x,y)&&pointer(input::PointerPhase::kUp,x,y);}
    bool drag(float x,float y,float tx,float ty) {return pointer(input::PointerPhase::kDown,x,y)&&pointer(input::PointerPhase::kMove,tx,ty)&&pointer(input::PointerPhase::kUp,tx,ty);}
    void undo() {ASSERT_EQ(f.undo(++request,f.readRuntimeState().identity.runtimeGeneration,6600).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());}
    void redo() {ASSERT_EQ(f.redo(++request,f.readRuntimeState().identity.runtimeGeneration,6600).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(c.ownerPump());}
    void exercise(bool ink) {
        seed(ink);create();ASSERT_TRUE(click(60,70));
        const auto before=c.readback().semanticGeneration;
        ASSERT_TRUE(pointer(input::PointerPhase::kDown,60,70));ASSERT_TRUE(pointer(input::PointerPhase::kMove,390,290));
        EXPECT_TRUE(f.readSection(section).directChildren.empty());EXPECT_EQ(c.readback().semanticGeneration,before);
        ASSERT_TRUE(pointer(input::PointerPhase::kUp,390,290));
        auto s=f.readSection(section);ASSERT_EQ(s.directChildren,std::vector{actor});ASSERT_EQ(s.members.size(),1U);
        EXPECT_DOUBLE_EQ(s.members[0].localTransform.tx,50);EXPECT_DOUBLE_EQ(s.members[0].localTransform.ty,60);
        EXPECT_DOUBLE_EQ(s.members[0].worldTransform.tx,350);EXPECT_DOUBLE_EQ(s.members[0].worldTransform.ty,260);
        EXPECT_EQ(c.readback().semanticGeneration,before+1); // final pose + parent is ONE operation
        undo();EXPECT_TRUE(f.readSection(section).directChildren.empty());
        auto control=f.readCanvasControlSnapshot(f.mountedCanvasTargets().front());
        EXPECT_DOUBLE_EQ(control.selectedTransform[4],20);EXPECT_DOUBLE_EQ(control.selectedTransform[5],40);
        redo();ASSERT_EQ(f.readSection(section).directChildren,std::vector{actor});
        runtime::SectionIntent select;select.action=runtime::SectionAction::kSelect;select.objectId=section;command(select);
        ASSERT_TRUE(drag(310,210,340,250));s=f.readSection(section);ASSERT_EQ(s.members.size(),1U);
        EXPECT_DOUBLE_EQ(s.members[0].localTransform.tx,50);EXPECT_DOUBLE_EQ(s.members[0].worldTransform.tx,380);
        EXPECT_DOUBLE_EQ(s.members[0].worldTransform.ty,300);
        ASSERT_TRUE(click(420,330));const auto preExit=c.readback().semanticGeneration;
        ASSERT_TRUE(drag(420,330,100,120));EXPECT_TRUE(f.readSection(section).directChildren.empty());
        control=f.readCanvasControlSnapshot(f.mountedCanvasTargets().front());
        EXPECT_DOUBLE_EQ(control.selectedTransform[4],60);EXPECT_DOUBLE_EQ(control.selectedTransform[5],90);
        EXPECT_EQ(c.readback().semanticGeneration,preExit+1);
        undo();s=f.readSection(section);ASSERT_EQ(s.members.size(),1U);
        EXPECT_DOUBLE_EQ(s.members[0].worldTransform.tx,380);EXPECT_DOUBLE_EQ(s.members[0].worldTransform.ty,300);
    }
};
TEST_F(SectionDropTest, ShapeDropUpdatesMembershipAndFollowsSectionWithOneStepHistory) {exercise(false);}
TEST_F(SectionDropTest, StrokeDropUpdatesMembershipAndFollowsSectionWithOneStepHistory) {exercise(true);}
TEST_F(SectionDropTest, ActualAffineBoundaryAndFinalPreviewPoseDetermineDrop) {
    seed();create({1,1,-1,1,400,200},{0,0,100,100});ASSERT_TRUE(click(60,70));
    ASSERT_TRUE(drag(60,70,310,210)); // inside AABB, outside the actual parallelogram
    EXPECT_TRUE(f.readSection(section).directChildren.empty());
    ASSERT_TRUE(drag(310,210,400,300));auto s=f.readSection(section);ASSERT_EQ(s.members.size(),1U);
    const semantic::Transform2D expected{.5,-.5,.5,.5,15,55};EXPECT_EQ(s.members[0].localTransform,expected);
    EXPECT_DOUBLE_EQ(s.members[0].worldTransform.tx,360);EXPECT_DOUBLE_EQ(s.members[0].worldTransform.ty,270);
}
TEST_F(SectionDropTest, CancelAndLateUpNeverChangeMembershipOrHistory) {
    seed();create();ASSERT_TRUE(click(60,70));const auto before=c.readback().semanticGeneration;
    ASSERT_TRUE(pointer(input::PointerPhase::kDown,60,70));ASSERT_TRUE(pointer(input::PointerPhase::kMove,390,290));
    (void)pointer(input::PointerPhase::kCancel,390,290);(void)pointer(input::PointerPhase::kUp,390,290);
    EXPECT_EQ(c.readback().semanticGeneration,before);EXPECT_TRUE(f.readSection(section).directChildren.empty());
    const auto control=f.readCanvasControlSnapshot(f.mountedCanvasTargets().front());
    EXPECT_DOUBLE_EQ(control.selectedTransform[4],20);EXPECT_DOUBLE_EQ(control.selectedTransform[5],40);
}

TEST_F(SectionDropTest, NestedSectionWinsAndCannotBecomeDescendantOfItsOwnChild) {
    seed();create({1,0,0,1,250,150},{0,0,400,300});
    const auto inner=foundation::ObjectId::fromUint64(6604);
    runtime::SectionIntent nested;nested.objectId=inner;nested.parentId=section;
    nested.transform.tx=50;nested.transform.ty=50;nested.content.boundary={0,0,200,160};command(nested);
    ASSERT_TRUE(click(60,70));ASSERT_TRUE(drag(60,70,390,290));
    ASSERT_EQ(f.readSection(inner).directChildren,std::vector{actor});
    EXPECT_EQ(f.readSection(section).directChildren,std::vector{inner});
    runtime::SectionIntent select;select.action=runtime::SectionAction::kSelect;select.objectId=section;command(select);
    ASSERT_TRUE(drag(260,160,390,280)); // release inside committed child; must not introduce a cycle
    EXPECT_FALSE(f.readSection(section).parentId);
    const auto child=f.readSection(inner);ASSERT_EQ(child.members.size(),1U);
    EXPECT_DOUBLE_EQ(child.members[0].worldTransform.tx,480);EXPECT_DOUBLE_EQ(child.members[0].worldTransform.ty,380);
}
TEST_F(SectionDropTest, SurfaceMetricsChangeInvalidatesDropAndLateUp) {
    seed();create();ASSERT_TRUE(click(60,70));const auto before=c.readback().semanticGeneration;
    ASSERT_TRUE(pointer(input::PointerPhase::kDown,60,70));ASSERT_TRUE(pointer(input::PointerPhase::kMove,390,290));
    ASSERT_TRUE(c.resizeSurface(810,610));(void)pointer(input::PointerPhase::kUp,390,290);
    EXPECT_EQ(c.readback().semanticGeneration,before);EXPECT_TRUE(f.readSection(section).directChildren.empty());
}

}
