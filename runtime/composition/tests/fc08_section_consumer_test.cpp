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
}
