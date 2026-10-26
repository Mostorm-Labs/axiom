#include "canvas/scene/scene_types.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "canvas/scene/full_scene_compiler.hpp"
#include "canvas/scene/direct_render_scene.hpp"
#include "canvas/scene/uniform_grid_spatial_index.hpp"
#include "canvas/scene/incremental_runtime_coordinator.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "object_store_mutator.hpp"
#include "incremental_runtime_test_access.hpp"
#include <gtest/gtest.h>

namespace {
using namespace canvas;
semantic::ObjectRecord object(std::uint64_t value, semantic::ObjectKind kind,
                              semantic::ObjectContent content) {
    semantic::ObjectRecord r{};
    r.id=semantic::ObjectId::fromUint64(value);r.kind=kind;r.kind_version=1;
    r.placement.order_key=semantic::OrderKey({static_cast<std::uint8_t>(value)});r.content=std::move(content);
    return r;
}
TEST(FC08SectionGeometry, AncestorsComposeOnceAndBoundaryDoesNotClipChildren) {
    auto group=object(1,semantic::ObjectKind::kGroup,semantic::GroupContent{});
    group.transform={0,2,-2,0,100,200};
    auto section=object(2,semantic::ObjectKind::kSection,semantic::SectionContent{{-10,-20,80,60},"Area",8,20});
    section.placement.parent_id=group.id;section.transform.tx=10;section.transform.ty=20;
    semantic::ShapeContent shapeContent{};shapeContent.shape_kind=1;shapeContent.width=20;shapeContent.height=10;
    auto shape=object(3,semantic::ObjectKind::kShape,shapeContent);
    shape.placement.parent_id=section.id;shape.transform.tx=150;shape.transform.ty=5;
    const std::vector<semantic::ObjectRecord> source{shape,section,group};
    const auto projection=projectRuntimeScene(source,semantic::SemanticGeneration(1));
    const auto* s=projection.find(section.id);ASSERT_NE(s,nullptr);
    EXPECT_DOUBLE_EQ(s->transform.tx,60);EXPECT_DOUBLE_EQ(s->transform.ty,220);
    EXPECT_FLOAT_EQ(s->geometryBounds.left,-10);EXPECT_FLOAT_EQ(s->geometryBounds.bottom,40);
    EXPECT_FLOAT_EQ(s->worldBounds.left,-20);EXPECT_FLOAT_EQ(s->worldBounds.right,100);
    const auto* child=projection.find(shape.id);ASSERT_NE(child,nullptr);
    EXPECT_DOUBLE_EQ(child->transform.tx,50);EXPECT_DOUBLE_EQ(child->transform.ty,520);
    EXPECT_FLOAT_EQ(child->worldBounds.left,30);EXPECT_FLOAT_EQ(child->worldBounds.bottom,560);
    EXPECT_FLOAT_EQ(projection.find(group.id)->worldBounds.bottom,560); // includes descendants outside Section
    EXPECT_EQ(projection.records.front().objectId,group.id);
    EXPECT_EQ(projection.records.back().objectId,shape.id);
    EXPECT_EQ(shape.transform.tx,150); // the canonical input remains local
}
TEST(FC08SectionGeometry, NegativeBoundaryResizePreservesChildrenWorldPose) {
    auto section=object(1,semantic::ObjectKind::kSection,semantic::SectionContent{{0,0,100,80},"",0,0});
    section.transform={2,0,0,2,100,200};
    auto child=object(2,semantic::ObjectKind::kSticky,semantic::StickyContent{30,30});
    child.placement.parent_id=section.id;child.transform.tx=30;child.transform.ty=20;
    const auto before=projectRuntimeScene(std::vector{section,child});
    std::get<semantic::SectionContent>(section.content).boundary={-20,0,120,80};
    const auto after=projectRuntimeScene(std::vector{section,child});
    ASSERT_NE(after.find(child.id),nullptr);
    EXPECT_EQ(before.find(child.id)->transform,after.find(child.id)->transform);
    EXPECT_FLOAT_EQ(after.find(section.id)->worldBounds.left,60);
    EXPECT_FLOAT_EQ(after.find(child.id)->worldBounds.left,160);
}
// Adapter maps the real FullSceneCompiler projection to the existing Scene
// participant interface. No geometry or expected coordinate is implemented here.
class SectionCompiler final : public ISemanticSceneCompiler {
public:
    foundation::Result<CompiledSceneSnapshot> compileFull(const semantic::SemanticReadView& view) const override {
        const auto projection=scene::FullSceneCompiler::compile(view);
        if(!projection)return foundation::Result<CompiledSceneSnapshot>::failure(projection.error());
        CompiledSceneSnapshot result{SceneRevision(view.generation().value()),{}};
        std::uint32_t slot=0;
        for(const auto& p:projection.value().records) {
            ++slot;SceneRecord r{};r.objectId=p.objectId;r.orderKey=SceneOrderKey(slot);
            r.kind=p.kind==semantic::ObjectKind::kSection?SceneObjectKind::kSection:SceneObjectKind::kSticky;
            r.flags=static_cast<SceneRecordFlags>(5);r.worldBounds=p.worldBounds;
            r.contentRevision=ContentRevision(view.generation().value());r.renderPayload={slot,1};r.hitGeometry={slot,1};
            r.localTransform=p.transform;r.derivedGeometry=p.derivedGeometry;r.hierarchyOrder=p.hierarchyOrder;
            result.records.push_back(std::move(r));
        }
        return foundation::Result<CompiledSceneSnapshot>::success(std::move(result));
    }
    foundation::Result<CompiledSceneDelta> compileDelta(const semantic::SemanticReadView&,const semantic::ChangeSet&) const override {
        return foundation::Result<CompiledSceneDelta>::failure({foundation::ErrorCode::kRequiresFullRebuild,"same full-projection lane as the editor compiler"});
    }
};
TEST(FC08SectionGeometry, PublicationFailureRetainsOldSceneAndRecoveryDoesNotClipChildren) {
    auto section=object(1,semantic::ObjectKind::kSection,semantic::SectionContent{{0,0,100,80},"Area",0,0});
    section.transform.tx=50;section.transform.ty=60;
    auto child=object(2,semantic::ObjectKind::kSticky,semantic::StickyContent{20,20});
    child.placement.parent_id=section.id;child.transform.tx=150;child.transform.ty=10;
    semantic::ReferenceObjectStore store;
    ASSERT_TRUE(semantic::internal::ObjectStoreMutator::insertFresh(store,section));
    ASSERT_TRUE(semantic::internal::ObjectStoreMutator::insertFresh(store,child));
    Scene painted(std::make_unique<DirectRenderScene>(),std::make_unique<UniformGridSpatialIndex>());
    SceneBinding binding(painted);IncrementalRuntimeCoordinator coordinator(binding);SectionCompiler compiler;
    semantic::SemanticReadView before(store,semantic::SemanticGeneration(1));
    ASSERT_TRUE(coordinator.recover(compiler,SceneCommitInput(before.generation(),before)));
    const auto outside=painted.query({{199,69,221,91}});ASSERT_TRUE(outside);
    EXPECT_EQ(outside.value().backToFront,std::vector{child.id});
    std::get<semantic::SectionContent>(section.content).boundary={-20,0,200,80};
    ASSERT_TRUE(semantic::internal::ObjectStoreMutator::replaceExisting(store,section));
    semantic::SemanticReadView after(store,semantic::SemanticGeneration(2));
    const auto changes=semantic::ChangeSet::fromChanges(before.generation(),after.generation(),{{section.id,semantic::SemanticChangeFlags::kContent,{}}});
    IncrementalRuntimeTestAccess::failAt(coordinator,RuntimeCheckpoint::kBeforePublication);
    EXPECT_FALSE(coordinator.apply(compiler,SceneCommitInput(before.generation(),after.generation(),after,&changes)));
    EXPECT_EQ(painted.semanticGeneration(),before.generation());EXPECT_EQ(coordinator.runtimeScene().generation(),before.generation());
    EXPECT_FLOAT_EQ(painted.read().find(section.id)->worldBounds.right,150);
    IncrementalRuntimeTestAccess::clear(coordinator);
    ASSERT_TRUE(coordinator.recover(compiler,SceneCommitInput(after.generation(),after)));
    EXPECT_EQ(painted.semanticGeneration(),after.generation());EXPECT_EQ(coordinator.runtimeScene().generation(),after.generation());
    EXPECT_FLOAT_EQ(painted.read().find(section.id)->worldBounds.right,230);
    EXPECT_DOUBLE_EQ(coordinator.runtimeScene().find(child.id)->transform.tx,200);
    const auto hit=painted.hitTest({{205,75},0,{},1});ASSERT_TRUE(hit);ASSERT_EQ(hit.value().frontToBack.size(),1U);
    EXPECT_EQ(hit.value().frontToBack[0],child.id);EXPECT_TRUE(coordinator.publicationObservationCoherent());
}
}
