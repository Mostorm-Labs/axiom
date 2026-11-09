#include "canvas/scene/incremental_runtime_coordinator.hpp"
#include "canvas/scene/full_scene_compiler.hpp"
#include "canvas/scene/testing/fake_render_scene.hpp"
#include "canvas/scene/testing/fake_spatial_index.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "incremental_runtime_test_access.hpp"
#include "object_store_mutator.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>
using namespace canvas;
namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
SceneRecord sceneRecord(const auto& r,semantic::SemanticGeneration generation){
 SceneRecord out;out.objectId=r.objectId;out.orderKey=SceneOrderKey(r.placement.order_key.bytes());
 out.kind=r.kind==semantic::ObjectKind::kVectorStroke?SceneObjectKind::kVectorStroke:SceneObjectKind::kShape;
 out.flags=SceneRecordFlags::kVisible;out.worldBounds=r.worldBounds;out.localTransform=r.transform;out.derivedGeometry=r.derivedGeometry;out.properties=r.properties;
 out.contentRevision=ContentRevision(generation.value());out.renderPayload={unsigned(r.objectId.bytes.back()),unsigned(generation.value())};out.hitGeometry={out.renderPayload.slot,out.renderPayload.generation};return out;
}
class Compiler:public ISemanticSceneCompiler {
 public:
 foundation::Result<CompiledSceneSnapshot> compileFull(const semantic::SemanticReadView& view)const override{
  auto full=scene::FullSceneCompiler::compile(view);if(!full)return foundation::Result<CompiledSceneSnapshot>::failure(full.error());
  CompiledSceneSnapshot result{SceneRevision(view.generation().value()),{}};for(const auto& r:full.value().records)result.records.push_back(sceneRecord(r,view.generation()));
  return foundation::Result<CompiledSceneSnapshot>::success(std::move(result));
 }
 foundation::Result<CompiledSceneDelta> compileDelta(const semantic::SemanticReadView&,const semantic::ChangeSet&)const override{
  return foundation::Result<CompiledSceneDelta>::failure({foundation::ErrorCode::kRequiresFullRebuild,"test full fallback"});
 }
 foundation::Result<CompiledSceneDelta> compilePreparedDelta(const semantic::SemanticReadView& view,const semantic::ChangeSet& changes,std::span<const RuntimeSceneRecord> affected,const SceneReadView& before)const override{
  CompiledSceneDelta delta{before.revision(),SceneRevision(view.generation().value()),{},std::nullopt};
  for(const auto& change:changes.objects()){
   const auto* old=before.find(change.object_id);auto found=std::find_if(affected.begin(),affected.end(),[&](const auto& r){return r.objectId==change.object_id;});
   if(old&&found!=affected.end())delta.mutations.push_back({SceneMutationKind::kUpdate,change.object_id,*old,sceneRecord(*found,view.generation())});
  }return foundation::Result<CompiledSceneDelta>::success(std::move(delta));
 }
};
semantic::EraseMaskRecord mask(unsigned id){semantic::SweptCircleMask sweep;sweep.segments.push_back({{{double(id*2),30},5},{{double(id*2),90},5},{double(id*2),40},{double(id*2),80}});return {ObjectId::fromUint64(1000+id),sweep};}
struct Fixture {
 semantic::ReferenceObjectStore store;Scene scene{std::make_unique<testing::FakeRenderScene>(),std::make_unique<testing::FakeSpatialIndex>()};SceneBinding binding{scene};IncrementalRuntimeCoordinator coordinator{binding};Compiler compiler;std::uint64_t generation=1;
 Fixture(bool mixed){
  semantic::ObjectRecord ink;ink.id=ObjectId::fromUint64(1);ink.kind=semantic::ObjectKind::kVectorStroke;ink.kind_version=3;ink.placement.order_key=semantic::OrderKey({1});
  semantic::CompactStrokeContent content;content.stroke.config.version=2;content.stroke.config.algorithmVersion=4;content.stroke.config.signalVersion=3;content.stroke.seed=42;
  for(unsigned i=0;i<7199;++i)content.stroke.knots.push_back({i*.2,64+25*std::sin(i*.023),32768});ink.content=content;
  require(semantic::internal::ObjectStoreMutator::insertFresh(store,ink),"seed ink");
  if(mixed)for(unsigned i=2;i<=5;++i){auto r=ink;r.id=ObjectId::fromUint64(i);r.kind_version=1;r.placement.order_key=semantic::OrderKey({std::uint8_t(i)});r.transform.tx=1800+i*100;
   if(i==2){r.kind=semantic::ObjectKind::kShape;r.content=semantic::ShapeContent{1,20,40};}
   if(i==3){r.kind=semantic::ObjectKind::kImage;r.content=semantic::ImageContent{.width=20,.height=30};}
   if(i==4){r.kind=semantic::ObjectKind::kSticky;r.content=semantic::StickyContent{20,30};}
   if(i==5){r.kind=semantic::ObjectKind::kTable;r.content=semantic::TableContent{};}
   require(semantic::internal::ObjectStoreMutator::insertFresh(store,r),"mixed seed");
  }
  auto view=semantic::SemanticReadView(store,semantic::SemanticGeneration{generation});require(bool(coordinator.recover(compiler,SceneCommitInput(view.generation(),view))),"seed recovery");parity();
 }
 void parity(){
  const auto expected=scene::FullSceneCompiler::compile(semantic::SemanticReadView(store,semantic::SemanticGeneration{generation}));require(bool(expected),"independent full compiler");
  const auto& actual=coordinator.runtimeScene();require(actual.generation().value()==generation&&actual.records().size()==expected.value().records.size(),"full generation/count parity");
  for(const auto& e:expected.value().records){const auto* a=actual.find(e.objectId);require(a&&a->kind==e.kind&&a->kindVersion==e.kindVersion&&a->placement==e.placement&&a->transform==e.transform&&a->properties==e.properties&&a->content==e.content&&a->eraseMasks==e.eraseMasks&&a->derivedGeometry==e.derivedGeometry&&a->geometryBounds==e.geometryBounds&&a->visualBounds==e.visualBounds&&a->worldBounds==e.worldBounds&&a->referenceGeometryDigest==e.referenceGeometryDigest&&a->directDependencies==e.directDependencies&&a->geometryChunks==e.geometryChunks&&a->textLayout==e.textLayout&&a->hierarchyOrder==e.hierarchyOrder,"all projection fields/full authority mismatch");}
 }
 auto update(semantic::ObjectRecord object,semantic::SemanticChangeFlags flags=semantic::SemanticChangeFlags::kEraseMasks){
  const auto before=semantic::SemanticGeneration(generation),after=semantic::SemanticGeneration(++generation);require(semantic::internal::ObjectStoreMutator::replaceExisting(store,std::move(object)),"replace canonical test bootstrap");
  const auto change=semantic::ChangeSet::fromChanges(before,after,{{ObjectId::fromUint64(1),flags,{}}});const auto view=semantic::SemanticReadView(store,after);return coordinator.apply(compiler,SceneCommitInput(before,after,view,&change));
 }
};
void reuse(bool mixed){
 Fixture f(mixed);const auto old=f.coordinator.runtimeScene().records();const auto* first=f.coordinator.runtimeScene().find(ObjectId::fromUint64(1));const auto content=first->content.shared();const auto chunks=first->geometryChunks.storage();const auto index=first->geometryIndex;
 const auto initial=f.coordinator.runtimeScene().publicationWork();
 for(unsigned i=0;i<24;++i){auto object=*f.store.find(ObjectId::fromUint64(1));if(i%3==2)object.erase_masks.pop_back();else object.erase_masks.push_back(mask(i+1));
  auto result=f.update(std::move(object));require(bool(result)&&result.value().disposition==SceneSyncDisposition::kAppliedIncremental,"mask-only edit used broad mixed recovery");f.parity();
  const auto* now=f.coordinator.runtimeScene().find(ObjectId::fromUint64(1));require(now->content.shared()==content&&now->geometryChunks.storage()==chunks&&now->geometryIndex==index,"body/chunks/index identity replaced on masks");require(old[0].eraseMasks.empty(),"published old mask snapshot mutated");
 }
 const auto work=f.coordinator.runtimeScene().publicationWork();require(work.materializationCalls==initial.materializationCalls&&work.bodyCloneBytes==initial.bodyCloneBytes&&work.maskOnlyHits-initial.maskOnlyHits==24,"mask-only rematerialized body");
 std::cout<<"{\"mixed\":"<<mixed<<",\"mask_only_prepares\":24,\"materialization_delta\":0,\"body_clone_bytes_delta\":0,\"full_projection_parity\":24,\"body_comparison_knots\":"<<work.bodyComparisonKnots-initial.bodyComparisonKnots<<"}\n";
 // A false flags hint never proves changed config/geometry/placement/properties.
 for(unsigned kind=0;kind<5;++kind){auto object=*f.store.find(ObjectId::fromUint64(1));if(kind==0)std::get<semantic::CompactStrokeContent>(object.content).stroke.knots[12].y+=1;if(kind==1)std::get<semantic::CompactStrokeContent>(object.content).stroke.seed++;if(kind==2)object.transform.tx+=1;if(kind==3)object.properties.entries.push_back({3U,.6F});if(kind==4)object.placement.order_key=semantic::OrderKey({27});const auto before=f.coordinator.runtimeScene().publicationWork();require(bool(f.update(std::move(object))),"conservative changed-body fallback failed");f.parity();require(f.coordinator.runtimeScene().publicationWork().maskOnlyHits==before.maskOnlyHits,"forged mask-only flags trusted body");}
}
void atomic(){
 for(unsigned cp=0;cp<9;++cp){Fixture f(true);const auto old=f.coordinator.runtimeScene().records();const auto before=f.scene.revision();auto object=*f.store.find(ObjectId::fromUint64(1));object.erase_masks.push_back(mask(1));
  IncrementalRuntimeTestAccess::failAt(f.coordinator,static_cast<RuntimeCheckpoint>(cp));auto result=f.update(object);IncrementalRuntimeTestAccess::clear(f.coordinator);
  if(result){require(false,"injected existing incremental checkpoint ignored");}
  else{require(f.coordinator.runtimeScene().generation().value()==1&&f.scene.revision()==before&&f.coordinator.runtimeScene().find(object.id)->eraseMasks.empty(),"partial Scene publication after failure");
   const auto view=semantic::SemanticReadView(f.store,semantic::SemanticGeneration{f.generation});require(bool(f.coordinator.recover(f.compiler,SceneCommitInput(view.generation(),view))),"recover latest canonical state");f.parity();}
  require(old[0].eraseMasks.empty(),"held snapshot mutated during recovery");require(f.store.find(object.id)->erase_masks.size()==1,"recovery reapplied/undid canonical mutation");
 }
 std::cout<<"{\"mask_only_checkpoint_sweep\":9,\"canonical_mutations_preserved\":true,\"old_snapshots_immutable\":true}\n";
}
}
int main(int argc,char** argv){try{if(argc>1&&std::string_view(argv[1])=="--atomic")atomic();else reuse(argc>1&&std::string_view(argv[1])=="--mixed");return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
